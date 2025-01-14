#include "world.h"
#include "core/profiler.h"
#include "core/log.h"
#include "core/file_io.h"
#include "engine/serialiser.h"
#include "component_storage.h"
#include "component_type_registry.h"
#include "entity_handle.h"
#include <cassert>
#include <bit>

namespace R3
{
namespace Entities
{
	World::World()
	{
		m_allEntities.reserve(1024 * 256);
		m_allEntityNames.reserve(1024 * 256);
	}

	World::~World()
	{
	}

	void World::SetEntityName(const EntityHandle& h, std::string_view name)
	{
		if (IsHandleValid(h))
		{
			m_allEntityNames[h.GetPrivateIndex()] = name;
		}
	}

	const std::string_view World::GetEntityName(const EntityHandle& h)
	{
		if (IsHandleValid(h))
		{
			return m_allEntityNames[h.GetPrivateIndex()];
		}
		return "";
	}

	EntityHandle World::GetEntityByName(std::string_view name)
	{
		R3_PROF_EVENT();
		EntityHandle foundEntity;
		ForEachActiveEntity([&](const EntityHandle& e) {
			if (GetEntityName(e) == name)
			{
				foundEntity = e;
				return false;
			}
			return true;
		});
		return foundEntity;
	}

	void World::SerialiseEntity(const EntityHandle& e, JsonSerialiser& target)
	{
		assert(IsHandleValid(e));
		auto entityID = e.GetID();
		target("ID", entityID);
		auto parentID = GetParent(e).GetID();
		target("Parent", parentID);
		if (e.GetPrivateIndex() != -1 && e.GetPrivateIndex() < m_allEntityNames.size())
		{
			target("Name", m_allEntityNames[e.GetPrivateIndex()]);
		}
		const auto& ped = m_allEntities[e.GetPrivateIndex()];
		for (int typeIndex = 0; typeIndex < ped.m_componentIndices.size(); ++typeIndex)
		{
			auto typeMask = (PerEntityData::ComponentBitsetType)1 << typeIndex;
			if ((ped.m_ownedComponentBits & typeMask) == typeMask && ped.m_componentIndices[typeIndex] != -1)
			{
				m_allComponents[typeIndex]->Serialise(e, ped.m_componentIndices[typeIndex], target);
			}
		}
	}

	JsonSerialiser World::SerialiseEntities()
	{
		R3_PROF_EVENT();
		JsonSerialiser allJson(JsonSerialiser::Mode::Write);
		JsonSerialiser entityJson(JsonSerialiser::Mode::Write);
		for (int h = 0; h < m_allEntities.size(); ++h)
		{
			if (m_allEntities[h].m_publicID != -1)
			{
				SerialiseEntity(EntityHandle(m_allEntities[h].m_publicID, h), entityJson);
				allJson.GetJson().emplace_back(std::move(entityJson.GetJson()));
			}
		}
		return allJson;
	}

	JsonSerialiser World::SerialiseEntities(const std::vector<EntityHandle>& handles)
	{
		R3_PROF_EVENT();
		JsonSerialiser allJson(JsonSerialiser::Mode::Write);
		JsonSerialiser entityJson(JsonSerialiser::Mode::Write);
		for (int h = 0; h < handles.size(); ++h)
		{
			if (IsHandleValid(handles[h]))
			{
				SerialiseEntity(handles[h], entityJson);
				allJson.GetJson().emplace_back(std::move(entityJson.GetJson()));
			}
		}
		return allJson;
	}

	std::vector<EntityHandle> World::SerialiseEntities(const JsonSerialiser& json, const std::vector<EntityHandle>& restoreHandles)
	{
		assert(restoreHandles.size() == 0 || restoreHandles.size() == json.GetJson().size());

		// We first create entities for each one in the json, and store a mapping of old id in json -> new handle in world
		// Then during serialisation, when any entity handle is encountered, we patch the old handle with this new one
		std::unordered_map<uint32_t, EntityHandle> oldEntityToNewEntity;
		std::vector<EntityHandle> allCreatedHandles;
		try
		{
			R3_PROF_EVENT();
			oldEntityToNewEntity.reserve(json.GetJson().size());
			allCreatedHandles.reserve(json.GetJson().size());
			if (restoreHandles.size() > 0)
			{
				for (int e = 0; e < json.GetJson().size(); ++e)
				{
					uint32_t id = json.GetJson()[e]["ID"];
					EntityHandle newEntity = AddEntityFromHandle(restoreHandles[e]);
					assert(IsHandleValid(newEntity));
					if (!IsHandleValid(newEntity))
					{
						LogError("Failed to restore entity handle {}/{}", restoreHandles[e].GetID(), restoreHandles[e].GetPrivateIndex());
					}
					oldEntityToNewEntity[id] = newEntity;
					allCreatedHandles.push_back(newEntity);
				}
			}
			else
			{
				for (int e = 0; e < json.GetJson().size(); ++e)
				{
					uint32_t id = json.GetJson()[e]["ID"];
					EntityHandle newEntity = AddEntity();
					oldEntityToNewEntity[id] = newEntity;
					allCreatedHandles.push_back(newEntity);
				}
			}
		}
		catch (std::exception e)
		{
			LogError("Failed to load an entity ID - {}", e.what());
			return allCreatedHandles;
		}

		auto RecreateHandle = [&](EntityHandle& e)
		{
			if (e.GetID() != -1)
			{
				auto foundRemap = oldEntityToNewEntity.find(e.GetID());
				if (foundRemap != oldEntityToNewEntity.end())
				{
					e = foundRemap->second;
				}
				else
				{
					LogError("Entity handle references ID ({}) that doesn't exist in the loaded world!", e.GetID());
				}
			}
		};
		EntityHandle::SetOnLoadFinishCallback(RecreateHandle);
		try
		{
			JsonSerialiser childSerialiser(JsonSerialiser::Read);
			for (int e = 0; e < json.GetJson().size(); ++e)	// for each entity
			{
				// remap any per-entity IDs to their new values
				const uint32_t oldID = json.GetJson()[e]["ID"];
				const EntityHandle& actualHandle = oldEntityToNewEntity[oldID];
				const uint32_t oldParent = json.GetJson()[e].value("Parent", (uint32_t)-1);
				EntityHandle actualParent = oldParent != -1 ? oldEntityToNewEntity[oldParent] : EntityHandle();
				SetParent(actualHandle, actualParent);
				std::string newName = json.GetJson()[e].value("Name", "");
				SetEntityName(actualHandle, newName);
				childSerialiser.GetJson() = std::move(json.GetJson()[e]);
				for (auto childJson = childSerialiser.GetJson().begin(); childJson != childSerialiser.GetJson().end(); childJson++)
				{
					if (childJson.key() != "ID" && childJson.key() != "Parent" && childJson.key() != "Name")
					{
						if (AddComponent(actualHandle, childJson.key()))
						{
							uint32_t cmpTypeIndex = ComponentTypeRegistry::GetInstance().GetTypeIndex(childJson.key());	// we need the type index to lookup the storage
							const auto& ped = m_allEntities[actualHandle.GetPrivateIndex()];	// we need the new component index from the entity data
							m_allComponents[cmpTypeIndex]->Serialise(actualHandle, ped.m_componentIndices[cmpTypeIndex], childSerialiser);
						}
					}
				}
			}
		}
		catch (std::exception e)
		{
			LogError("Something went wrong while loading entities! - {}", e.what());
		}
		EntityHandle::SetOnLoadFinishCallback(nullptr);

		return allCreatedHandles;
	}

	void World::SerialiseComponent(const EntityHandle& e, std::string_view componentType, JsonSerialiser& json)
	{
		if (IsHandleValid(e))
		{
			const uint32_t componentTypeIndex = ComponentTypeRegistry::GetInstance().GetTypeIndex(componentType);
			assert(componentTypeIndex != -1);
			const auto& ped = m_allEntities[e.GetPrivateIndex()];
			auto typeMask = (PerEntityData::ComponentBitsetType)1 << componentTypeIndex;
			if ((ped.m_ownedComponentBits & typeMask) == typeMask && ped.m_componentIndices[componentTypeIndex] != -1)
			{
				m_allComponents[componentTypeIndex]->Serialise(e, ped.m_componentIndices[componentTypeIndex], json);
			}
		}
	}

	std::vector<EntityHandle> World::Import(std::string_view path)
	{
		R3_PROF_EVENT();
		std::vector<EntityHandle> newEntities;
		JsonSerialiser loadedJson(JsonSerialiser::Read);
		{
			R3_PROF_EVENT("LoadFile");
			std::string loadedJsonData;
			if (FileIO::LoadTextFromFile(path, loadedJsonData))
			{
				loadedJson.LoadFromString(loadedJsonData);
			}
			else
			{
				LogError("Failed to load world file '{}'", path);
				return {};
			}
		}
		try
		{
			JsonSerialiser entityJson(JsonSerialiser::Read, std::move(loadedJson.GetJson()["AllEntities"]));
			newEntities = SerialiseEntities(entityJson);
		}
		catch (std::exception e)
		{
			LogError("Failed to serialise entities - {}", e.what());
			return {};
		}
		return newEntities;
	}

	bool World::Load(std::string_view path)
	{
		R3_PROF_EVENT();
		JsonSerialiser loadedJson(JsonSerialiser::Read);
		{
			R3_PROF_EVENT("LoadFile");
			std::string loadedJsonData;
			if (FileIO::LoadTextFromFile(path, loadedJsonData))
			{
				loadedJson.LoadFromString(loadedJsonData);
			}
			else
			{
				LogError("Failed to load world file '{}'", path);
				return false;
			}
		}
		loadedJson("WorldName", m_name);
		try
		{
			JsonSerialiser entityJson(JsonSerialiser::Read, std::move(loadedJson.GetJson()["AllEntities"]));
			SerialiseEntities(entityJson);
		}
		catch (std::exception e)
		{
			LogError("Failed to serialise entities - {}", e.what());
			return false;
		}

		return true;
	}

	bool World::Save(std::string_view path)
	{
		R3_PROF_EVENT();
		CollectGarbage();	// ensure any entities pending delete are removed before saving
		JsonSerialiser worldJson(JsonSerialiser::Write);
		worldJson("WorldName", m_name);
		JsonSerialiser entityJson = SerialiseEntities();
		worldJson.GetJson()["AllEntities"] = std::move(entityJson.GetJson());
		return FileIO::SaveTextToFile(path, worldJson.GetJson().dump(1));
	}

	size_t World::GetEntityDisplayName(const EntityHandle& h, char* nameBuffer, size_t maxLength) const
	{
		// annoyingly, good old printf is still way faster than std::format 
		if (IsHandleValid(h))
		{
			if (m_allEntityNames[h.GetPrivateIndex()].size() != 0)
			{
				return snprintf(nameBuffer, maxLength, "%s (#%d)", m_allEntityNames[h.GetPrivateIndex()].c_str(), h.GetID());
			}
			else
			{
				return snprintf(nameBuffer, maxLength, "Entity %d", h.GetID());
			}
		}
		else
		{
			nameBuffer[0] = '\0';
			return 0;	
		}
	}

	std::string World::GetEntityDisplayName(const EntityHandle& h) const
	{
		char nameBuffer[256] = "";
		GetEntityDisplayName(h, nameBuffer, sizeof(nameBuffer));
		return std::string(nameBuffer);
	}

	std::vector<EntityHandle> World::GetActiveEntities(uint32_t startIndex, uint32_t endIndex) const
	{
		R3_PROF_EVENT();
		assert(endIndex >= startIndex);
		
		const uint32_t totalCount = static_cast<uint32_t>(m_allEntities.size());
		const uint32_t maxCount = (endIndex == -1) ? totalCount - startIndex : endIndex - startIndex;
		std::vector<EntityHandle> entities;
		entities.reserve(maxCount);

		if (m_freeEntityIndices.size() == 0 && m_reservedSlots.size() == 0)	// fast path if all slots are allocated
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

	std::vector<EntityHandle> World::GetOwnersOfComponent1(std::string_view componentTypeName)
	{
		std::vector<EntityHandle> results;
		results.reserve(128);	// good idea?
		const uint32_t componentTypeIndex = ComponentTypeRegistry::GetInstance().GetTypeIndex(componentTypeName);
		assert(componentTypeIndex != -1);
		const auto typeMask = (PerEntityData::ComponentBitsetType)1 << componentTypeIndex;
		auto forEachEntity = [this,&results, typeMask](const EntityHandle& e)
		{
			if (HasAnyComponents(e, typeMask))
			{
				results.push_back(e);
			}
			return true;
		};
		ForEachActiveEntity(forEachEntity);
		return results;
	}

	EntityHandle World::AddEntity()
	{
		R3_PROF_EVENT();
		auto newId = m_entityIDCounter++;	// note we are not checking for duplicates here!
		auto toDelete = std::find_if(m_pendingDelete.begin(), m_pendingDelete.end(), [newId](const PendingDeleteEntity& pde) {
			return pde.m_handle.GetID() == newId;
		});
		if (toDelete != m_pendingDelete.end())
		{
			LogError("Entity '{}' already existed and is being destroyed!", newId);
			return {};	// the old entity didn't clean up fully yet
		}
		uint32_t newIndex = -1;
		if (m_freeEntityIndices.size() > 0)		// pop from free list
		{
			newIndex = m_freeEntityIndices[0];
			m_freeEntityIndices.pop_front();
			assert(m_allEntities[newIndex].m_publicID == -1);
			assert(m_allEntities[newIndex].m_ownedComponentBits == 0);
#ifdef R3_ENTITY_INDICES_IN_VECTOR
			assert(m_allEntities[newIndex].m_componentIndices.size() == 0);
#endif
			m_allEntities[newIndex].m_publicID = newId;
			m_allEntityNames[newIndex].clear();
		}
		else
		{
			PerEntityData newEntityData;
			newEntityData.m_publicID = newId;
			m_allEntities.push_back(newEntityData);
			m_allEntityNames.push_back("");
			newIndex = static_cast<uint32_t>(m_allEntities.size() - 1);
			assert(m_allEntityNames.size() == m_allEntities.size());
		}
		return EntityHandle(newId, newIndex);
	}

	EntityHandle World::AddEntityFromHandle(const EntityHandle& handleToRestore)
	{
		auto toDelete = std::find_if(m_pendingDelete.begin(), m_pendingDelete.end(), [handleToRestore](const PendingDeleteEntity& pde) {
			return pde.m_handle == handleToRestore;
		});
		if (toDelete != m_pendingDelete.end())
		{
			LogError("Entity '{}' already existed and is being destroyed!", handleToRestore.GetID());
			return {};	// the old entity didn't clean up fully yet
		}

		auto reservation = m_reservedSlots.find(handleToRestore.GetID());
		assert(reservation != m_reservedSlots.end());
		if (reservation != m_reservedSlots.end())	// do we have a reservation for this public id
		{
			auto reservedIndex = reservation->second;
			assert(reservedIndex == handleToRestore.GetPrivateIndex());
			if (reservedIndex == handleToRestore.GetPrivateIndex())	// does the slot match?
			{
				assert(m_allEntities[reservedIndex].m_publicID == -1);
				assert(m_allEntities[reservedIndex].m_ownedComponentBits == 0);
#ifdef R3_ENTITY_INDICES_IN_VECTOR
				assert(m_allEntities[reservedIndex].m_componentIndices.size() == 0);
#endif
				m_allEntities[reservedIndex].m_publicID = handleToRestore.GetID();
				m_allEntityNames[reservedIndex].clear();
				m_reservedSlots.erase(reservation);
				return handleToRestore;
			}
		}
		return {};
	}

	bool World::HasParent(const EntityHandle& child, const EntityHandle& parent) const
	{
		R3_PROF_EVENT();
		EntityHandle thisParent = GetParent(child); 
		if (!IsHandleValid(child) || !IsHandleValid(parent))
		{
			return false;
		}
		if (thisParent == parent || thisParent == child)
		{
			return true;
		}
		else if (IsHandleValid(thisParent))
		{
			return HasParent(parent, thisParent);
		}
		else
		{
			return false;
		}
	}

	void World::GetChildren(const EntityHandle& parent, std::vector<EntityHandle>& results) const
	{
		if (IsHandleValid(parent))
		{
			auto& theEntity = m_allEntities[parent.GetPrivateIndex()];
			results = theEntity.m_children;
		}
	}

	void World::GetAllChildren(const EntityHandle& parent, std::vector<EntityHandle>& results) const
	{
		if (IsHandleValid(parent))
		{
			auto& theEntity = m_allEntities[parent.GetPrivateIndex()];
			for (const auto& child : theEntity.m_children)
			{
				GetAllChildren(child, results);
			}
			results.insert(results.end(), theEntity.m_children.begin(), theEntity.m_children.end());
		}
	}

	std::vector<EntityHandle> World::GetAllChildren(const EntityHandle& parent)
	{
		std::vector<EntityHandle> children;
		GetAllChildren(parent, children);
		return children;
	}

	bool World::SetParent(const EntityHandle& child, const EntityHandle& parent)
	{
		if (IsHandleValid(child))
		{
			auto& theEntity = m_allEntities[child.GetPrivateIndex()];
			if (parent == theEntity.m_parent)
			{
				return true;
			}
			if (IsHandleValid(theEntity.m_parent))		// remove child from the old parent
			{
				auto& theParent = m_allEntities[theEntity.m_parent.GetPrivateIndex()];
				auto foundChild = std::find(theParent.m_children.begin(), theParent.m_children.end(), child);
				assert(foundChild != theParent.m_children.end());
				theParent.m_children.erase(foundChild);
			}
			theEntity.m_parent = parent;
			if (IsHandleValid(parent))		// add child to new parent
			{
				auto& theParent = m_allEntities[parent.GetPrivateIndex()];
				theParent.m_children.push_back(child);
			}
			return true;
		}
		return false;
	}

	EntityHandle World::GetParent(const EntityHandle& child) const
	{
		if (IsHandleValid(child))
		{
			auto& theEntity = m_allEntities[child.GetPrivateIndex()];
			return theEntity.m_parent;
		}
		return EntityHandle();
	}

	void World::RemoveEntity(const EntityHandle& h, bool reserveHandle)
	{
		R3_PROF_EVENT();
		assert(h.GetID() != -1);
		assert(h.GetPrivateIndex() != -1);
		assert(h.GetPrivateIndex() < m_allEntities.size());
		if (IsHandleValid(h))
		{
			auto& theEntity = m_allEntities[h.GetPrivateIndex()];
			theEntity.m_ownedComponentBits = 0;	// we keep the component indices for later, just reset the mask
			m_pendingDelete.push_back({ h, reserveHandle });
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
		assert(resolvedTypeIndex != -1);

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
#ifdef R3_ENTITY_INDICES_IN_VECTOR
		if (m_allEntities[e.GetPrivateIndex()].m_componentIndices.size() < resolvedTypeIndex + 1)
		{
			m_allEntities[e.GetPrivateIndex()].m_componentIndices.resize(resolvedTypeIndex + 1, -1);
		}
#endif
		m_allEntities[e.GetPrivateIndex()].m_componentIndices[resolvedTypeIndex] = newCmpIndex;
	}

	bool World::HasAnyComponents(const EntityHandle& e, uint64_t typeBits) const
	{
		if (IsHandleValid(e))
		{
			const PerEntityData& ped = m_allEntities[e.GetPrivateIndex()];
			return (ped.m_ownedComponentBits & typeBits) != 0;
		}
		return false;
	}

	uint32_t World::GetOwnedComponentCount(const EntityHandle& e)
	{
		if (IsHandleValid(e))
		{
			const PerEntityData& ped = m_allEntities[e.GetPrivateIndex()];
			return std::popcount(ped.m_ownedComponentBits);
		}
		return 0;
	}

	bool World::HasAllComponents(const EntityHandle& e, uint64_t typeBits) const
	{
		if (IsHandleValid(e))
		{
			const PerEntityData& ped = m_allEntities[e.GetPrivateIndex()];
			return (ped.m_ownedComponentBits & typeBits) == typeBits;
		}
		return false;
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

	void World::RemoveComponent(const EntityHandle& e, std::string_view componentTypeName)
	{
		if (IsHandleValid(e))
		{
			const uint32_t typeIndex = ComponentTypeRegistry::GetInstance().GetTypeIndex(componentTypeName);
			if (typeIndex == -1 || typeIndex >= m_allComponents.size())
			{
				return;	// no type registered or we dont have storage for it yet
			}
			PerEntityData& ped = m_allEntities[e.GetPrivateIndex()];
			const auto testMask = (PerEntityData::ComponentBitsetType)1 << typeIndex;
			if ((ped.m_ownedComponentBits & testMask) == testMask && ped.m_componentIndices.size() > typeIndex)
			{
				const auto oldIndex = ped.m_componentIndices[typeIndex];
				ped.m_ownedComponentBits &= ~testMask;	// turn off the type bit
				ped.m_componentIndices[typeIndex] = -1;	// reset before calling Destroy
				m_allComponents[typeIndex]->Destroy(e, oldIndex);
			}
		}
	}

	bool World::AddComponent(const EntityHandle& e, std::string_view componentTypeName)
	{
		if (IsHandleValid(e))
		{
			uint32_t componentTypeIndex = ComponentTypeRegistry::GetInstance().GetTypeIndex(componentTypeName);
			assert(componentTypeIndex != -1);
			if (componentTypeIndex != -1)
			{
				AddComponentInternal(e, componentTypeIndex);
				return true;
			}
			else
			{
				LogError("Failed to add unknown component type '{}' to entity {}. Did you forget to register it?", componentTypeName, e.GetID());
			}
		}
		return false;
	}

	void World::CollectGarbage()
	{
		R3_PROF_EVENT();
		for (const auto& toDelete : m_pendingDelete)
		{
			if (IsHandleValid(toDelete.m_handle))
			{
				EntityHandle nullParent;	// reset the parent entity (removes entity from children)
				SetParent(toDelete.m_handle, nullParent);
				auto& theEntity = m_allEntities[toDelete.m_handle.GetPrivateIndex()];
				// use the entity component indices to destroy the objects
				for (int cmpType = 0; cmpType < theEntity.m_componentIndices.size(); ++cmpType)
				{
					if (theEntity.m_componentIndices[cmpType] != -1)
					{
						m_allComponents[cmpType]->Destroy(toDelete.m_handle, theEntity.m_componentIndices[cmpType]);
					}
				}
				// reset + push the entity to the free or reserved list
				theEntity.m_publicID = -1;
#ifdef R3_ENTITY_INDICES_IN_VECTOR
				theEntity.m_componentIndices.clear();	// clear out the old values but keep the memory around
#else
				theEntity.m_componentIndices.fill(-1);
#endif
				theEntity.m_children.clear();
				if (toDelete.m_reserveHandle)
				{
					assert(m_reservedSlots.find(toDelete.m_handle.GetID()) == m_reservedSlots.end());	// shouldnt be possible, but eh
					m_reservedSlots[toDelete.m_handle.GetID()] = toDelete.m_handle.GetPrivateIndex();
				}
				else
				{
					m_freeEntityIndices.push_back(toDelete.m_handle.GetPrivateIndex());
				}
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