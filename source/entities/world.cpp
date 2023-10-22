#include "world.h"
#include "core/profiler.h"
#include "core/log.h"
#include "engine/serialiser.h"
#include "component_storage.h"
#include "component_type_registry.h"
#include "entity_handle.h"
#include <cassert>

namespace R3
{
namespace Entities
{
	World::World()
	{
	}

	World::~World()
	{
	}

	JsonSerialiser World::SerialiseEntities(std::vector<EntityHandle> handles)
	{
		R3_PROF_EVENT();
		JsonSerialiser json(JsonSerialiser::Mode::Write);
		if (handles.size() == 0)	// collect all the entity handles first
		{
			handles.resize(m_allEntities.size());
			for (int i = 0; i < m_allEntities.size(); ++i)
			{
				handles[i] = EntityHandle(m_allEntities[i].m_publicID, i);
			}
		}
		std::vector<uint32_t> entityIDs;
		entityIDs.resize(handles.size());
		for (int i = 0; i < handles.size(); ++i)
		{
			entityIDs[i] = handles[i].GetID();
		}
		auto entityCount = entityIDs.size();
		json("EntityCount", entityCount);
		json("EntityIDs", entityIDs);	// needs to be available to build remapping table

		std::vector<nlohmann::json> allEntitiesJson;
		JsonSerialiser entityJson(JsonSerialiser::Mode::Write);
		for (int h = 0; h < handles.size(); ++h)
		{			
			if (IsHandleValid(handles[h]))
			{
				const auto& ped = m_allEntities[handles[h].GetPrivateIndex()];
				for (int typeIndex = 0; typeIndex < ped.m_componentIndices.size(); ++typeIndex)
				{
					auto typeMask = (PerEntityData::ComponentBitsetType)1 << typeIndex;
					if ((ped.m_ownedComponentBits & typeMask) == typeMask && ped.m_componentIndices[typeIndex] != -1)
					{
						m_allComponents[typeIndex]->Serialise(handles[h], ped.m_componentIndices[typeIndex], entityJson);
					}
				}
				if (!entityJson.GetJson().empty())	// only write entity data if there are components
				{
					entityJson("ID", entityIDs[h]);
					allEntitiesJson.push_back(std::move(entityJson.GetJson()));
				}
				entityJson.GetJson().clear();
			}
		}
		json.GetJson()["AllEntities"] = std::move(allEntitiesJson);

		return json;
	}

	size_t World::GetEntityDisplayName(const EntityHandle& h, char* nameBuffer, size_t maxLength) const
	{
		if (IsHandleValid(h))
		{
			return snprintf(nameBuffer, maxLength, "Entity %d", h.GetID());
		}
		else
		{
			nameBuffer[0] = '\0';
			return 0;	// is it small enough for small string optimisation?
		}
	}

	std::vector<EntityHandle> World::GetActiveEntities(uint32_t startIndex, uint32_t endIndex) const
	{
		R3_PROF_EVENT();
		assert(endIndex >= startIndex);
		
		const uint32_t totalCount = static_cast<uint32_t>(m_allEntities.size());
		const uint32_t maxCount = (endIndex == -1) ? totalCount - startIndex : endIndex - startIndex;
		std::vector<EntityHandle> entities;
		entities.reserve(maxCount);

		if (m_freeEntityIndices.size() == 0)	// fast path if all slots are allocated
		{
			uint32_t actualEnd = startIndex + maxCount;
			for (uint32_t i = startIndex; i < actualEnd; ++i)
			{
				entities.emplace_back(m_allEntities[i].m_publicID, i);
			}
		}
		else
		{
			uint32_t thisIndex = 0;
			for (uint32_t i = 0; i < totalCount && entities.size() < maxCount; ++i)
			{
				const uint32_t& id = m_allEntities[i].m_publicID;
				if (id != -1)
				{
					if (thisIndex++ >= startIndex)
					{
						entities.emplace_back(id, i);
					}
				}
			}
		}
		
		return entities;
	}

	EntityHandle World::AddEntity()
	{
		R3_PROF_EVENT();
		auto newId = m_entityIDCounter++;	// note we are not checking for duplicates here!
		auto toDelete = std::find_if(m_pendingDelete.begin(), m_pendingDelete.end(), [newId](const EntityHandle& p) {
			return p.GetID() == newId;
		});
		if (toDelete != m_pendingDelete.end())
		{
			LogError("Entity '%d' already existed and is being destroyed!", newId);
			return {};	// the old entity didn't clean up fully yet
		}
		uint32_t newIndex = -1;
		if (m_freeEntityIndices.size() > 0)		// pop from free list
		{
			newIndex = m_freeEntityIndices[0];
			m_freeEntityIndices.pop_front();
			assert(m_allEntities[newIndex].m_publicID == -1);
			assert(m_allEntities[newIndex].m_ownedComponentBits == 0);
			assert(m_allEntities[newIndex].m_componentIndices.size() == 0);
			m_allEntities[newIndex].m_publicID = newId;
		}
		else
		{
			PerEntityData newEntityData;
			newEntityData.m_publicID = newId;
			m_allEntities.push_back(newEntityData);
			newIndex = static_cast<uint32_t>(m_allEntities.size() - 1);
		}
		return EntityHandle(newId, newIndex);
	}

	void World::RemoveEntity(const EntityHandle& h)
	{
		R3_PROF_EVENT();
		assert(h.GetID() != -1);
		assert(h.GetPrivateIndex() != -1);
		assert(h.GetPrivateIndex() < m_allEntities.size());
		if (IsHandleValid(h))
		{
			auto& theEntity = m_allEntities[h.GetPrivateIndex()];
			theEntity.m_ownedComponentBits = 0;	// component checks will fail from this point
			m_pendingDelete.push_back(h);	// we keep around the component indices for later
		}
	}

	bool World::IsHandleValid(const EntityHandle& h) const
	{
		if (h.GetID() != -1 && h.GetPrivateIndex() != -1 && h.GetPrivateIndex() < m_allEntities.size())
		{
			return m_allEntities[h.GetPrivateIndex()].m_publicID == h.GetID();
		}
		else
		{
			return false;
		}
	}

	void World::AddComponentInternal(const EntityHandle& e, uint32_t resolvedTypeIndex)
	{
		R3_PROF_EVENT();
		if (resolvedTypeIndex != -1)
		{
			// do we need to allocate storage for this component type?
			if (m_allComponents.size() < resolvedTypeIndex + 1)
			{
				m_allComponents.resize(resolvedTypeIndex + 1);
			}
			if (m_allComponents[resolvedTypeIndex] == nullptr)
			{
				const auto& allTypes = ComponentTypeRegistry::GetInstance().AllTypes();
				m_allComponents[resolvedTypeIndex] = allTypes[resolvedTypeIndex].m_storageFactory(this);	// storage created from factory
			}

			uint32_t newCmpIndex = m_allComponents[resolvedTypeIndex]->Create(e);
			auto newBits = (PerEntityData::ComponentBitsetType)1 << resolvedTypeIndex;
			m_allEntities[e.GetPrivateIndex()].m_ownedComponentBits |= newBits;
			if (m_allEntities[e.GetPrivateIndex()].m_componentIndices.size() < resolvedTypeIndex + 1)
			{
				m_allEntities[e.GetPrivateIndex()].m_componentIndices.resize(resolvedTypeIndex + 1, -1);
			}
			m_allEntities[e.GetPrivateIndex()].m_componentIndices[resolvedTypeIndex] = newCmpIndex;
		}
	}

	bool World::HasComponent(const EntityHandle& e, std::string_view componentTypeName)
	{
		const uint32_t componentTypeIndex = ComponentTypeRegistry::GetInstance().GetTypeIndex(componentTypeName);
		assert(componentTypeIndex != -1);
		if (IsHandleValid(e) && componentTypeIndex != -1)
		{
			const auto testMask = (PerEntityData::ComponentBitsetType)1 << componentTypeIndex;
			const PerEntityData& ped = m_allEntities[e.GetPrivateIndex()];
			if ((ped.m_ownedComponentBits & testMask) == testMask && ped.m_componentIndices.size() > componentTypeIndex)
			{
				return ped.m_componentIndices[componentTypeIndex] != -1;
			}
		}
		return false;
	}

	void World::AddComponent(const EntityHandle& e, std::string_view componentTypeName)
	{
		if (IsHandleValid(e))
		{
			uint32_t componentTypeIndex = ComponentTypeRegistry::GetInstance().GetTypeIndex(componentTypeName);
			AddComponentInternal(e, componentTypeIndex);
		}
	}

	void World::CollectGarbage()
	{
		R3_PROF_EVENT();
		for (auto toDelete : m_pendingDelete)
		{
			if (IsHandleValid(toDelete))
			{
				// use the entity component indices to destroy the objects
				auto& theEntity = m_allEntities[toDelete.GetPrivateIndex()];
				for (int cmpType = 0; cmpType < theEntity.m_componentIndices.size(); ++cmpType)
				{
					if (theEntity.m_componentIndices[cmpType] != -1)
					{
						m_allComponents[cmpType]->Destroy(toDelete, theEntity.m_componentIndices[cmpType]);
					}
				}

				// reset + push the entity to the free list
				theEntity.m_publicID = -1;
				theEntity.m_componentIndices.clear();	// clear out the old values but keep the memory around
				m_freeEntityIndices.push_back(toDelete.GetPrivateIndex());
			}
		}
		m_pendingDelete.clear();
	}

	void World::OnComponentMoved(const EntityHandle& owner, uint32_t typeIndex, uint32_t oldIndex, uint32_t newIndex)
	{
		R3_PROF_EVENT();
		assert(IsHandleValid(owner));
		auto& theEntity = m_allEntities[owner.GetPrivateIndex()];
		assert(typeIndex < theEntity.m_componentIndices.size());
		assert(theEntity.m_componentIndices[typeIndex] == oldIndex);
		theEntity.m_componentIndices[typeIndex] = newIndex;
	}

	ComponentStorage* World::GetStorage(std::string_view componentTypeName)
	{
		const uint32_t typeIndex = ComponentTypeRegistry::GetInstance().GetTypeIndex(componentTypeName);
		if (typeIndex == -1 || typeIndex >= m_allComponents.size() )
		{
			return nullptr;
		}
		return m_allComponents[typeIndex].get();
	}
}
}