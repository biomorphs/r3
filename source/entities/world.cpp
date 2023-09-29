#include "world.h"
#include "core/profiler.h"
#include "core/log.h"
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

	EntityHandle World::AddEntity()
	{
		R3_PROF_EVENT();
		auto newId = m_entityIDCounter++;
		auto found = m_entityIdToIndex.find(newId);
		if (found != m_entityIdToIndex.end())
		{
			LogError("Entity '%d' already exists!", newId);
			return {};	// an entity already exists with that ID
		}
		auto toDelete = std::find_if(m_pendingDelete.begin(), m_pendingDelete.end(), [newId](const EntityHandle& p) {
			return p.GetID() == newId;
		});
		if (toDelete != m_pendingDelete.end())
		{
			LogError("Entity '%d' already existed and is being destroyed!", newId);
			return {};	// the old entity didn't clean up fully yet
		}
		uint32_t newIndex = -1;
		if (m_freeEntityIndices.size() > 0)
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
		m_entityIdToIndex[newId] = newIndex;
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
			// we keep around the component indices for later
			m_pendingDelete.push_back(h);
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

	void World::AddComponent(EntityHandle e, std::string_view componentTypeName)
	{
		R3_PROF_EVENT();
		if (IsHandleValid(e))
		{
			uint32_t componentTypeIndex = ComponentTypeRegistry::GetInstance().GetTypeIndex(componentTypeName);
			assert(componentTypeIndex != -1);
			if (componentTypeIndex != -1)
			{
				// do we need to allocate storage for this component type?
				if (m_allComponents.size() < componentTypeIndex + 1)
				{
					m_allComponents.resize(componentTypeIndex + 1);
				}
				if (m_allComponents[componentTypeIndex] == nullptr)
				{
					const auto& allTypes = ComponentTypeRegistry::GetInstance().AllTypes();
					m_allComponents[componentTypeIndex] = allTypes[componentTypeIndex].m_storageFactory(this);	// storage created from factory
				}
				uint32_t newCmpIndex = m_allComponents[componentTypeIndex]->Create(e);
				auto newBits = (PerEntityData::ComponentBitsetType)1 << componentTypeIndex;
				m_allEntities[e.GetPrivateIndex()].m_ownedComponentBits |= newBits;
				if (m_allEntities[e.GetPrivateIndex()].m_componentIndices.size() < componentTypeIndex + 1)
				{
					m_allEntities[e.GetPrivateIndex()].m_componentIndices.resize(componentTypeIndex + 1);
				}
				m_allEntities[e.GetPrivateIndex()].m_componentIndices[componentTypeIndex] = newCmpIndex;
			}
		}
	}

	void World::CollectGarbage()
	{
		for (auto toDelete : m_pendingDelete)
		{
			for (auto& components : m_allComponents)
			{
				if (components.get() != nullptr)
				{
					components->Destroy(toDelete);
				}
			}
			auto found = m_entityIdToIndex.find(toDelete.GetID());
			if (found != m_entityIdToIndex.end())
			{
				uint32_t oldIndex = found->second;
				m_allEntities[oldIndex].m_publicID = -1;
				m_allEntities[oldIndex].m_componentIndices.clear();	// clear out the old values but keep the memory around
				m_freeEntityIndices.push_back(oldIndex);
				m_entityIdToIndex.erase(toDelete.GetID());
			}
		}
		m_pendingDelete.clear();
	}

	void World::OnComponentMoved(const EntityHandle& owner, uint32_t typeIndex, uint32_t oldIndex, uint32_t newIndex)
	{
		assert(IsHandleValid(owner));
		if (IsHandleValid(owner) && typeIndex != -1 && oldIndex != -1 && newIndex != -1)
		{
			auto& theEntity = m_allEntities[owner.GetPrivateIndex()];
			assert(typeIndex < theEntity.m_componentIndices.size());
			assert(theEntity.m_componentIndices[typeIndex] == oldIndex);
			theEntity.m_componentIndices[typeIndex] = newIndex;
		}
	}
}
}