#pragma once 
#include "entity_handle.h"
#include "component_type_registry.h"
#include <string_view>
#include <vector>
#include <deque>
#include <memory>

namespace R3
{
class JsonSerialiser;
namespace Entities
{
	class ComponentStorage;
	template<class ComponentType> class LinearComponentStorage;
	class World
	{
	public:
		World();
		~World();

		void SetName(std::string_view name) { m_name = name; }
		std::string_view GetName() const { return m_name; }

		// Entity stuff. EntityHandle is essentially an opaque-ish ID
		EntityHandle AddEntity();
		EntityHandle AddEntityFromHandle(const EntityHandle& handleToRestore);	// restore a previously deleted reserved entity handle. only for tools
		EntityHandle GetParent(const EntityHandle& child) const;				// entity parent is purely a logistical thing, nothing in the sim changes unless it specifically acts on children
		bool SetParent(const EntityHandle& child, const EntityHandle& parent);	// returns false if failed (loops, etc)
		bool HasParent(const EntityHandle& child, const EntityHandle& parent) const;
		void GetChildren(const EntityHandle& parent, std::vector<EntityHandle>& results) const;	// immediate children
		void GetAllChildren(const EntityHandle& parent, std::vector<EntityHandle>& results) const;	// all children (recursive)

		// RemoveEntity defers deletion until CollectGarbage() called.
		// reserveHandle = dont add this entity handle to the free list, this slot is reserved until the exact same handle is recreated
		// only useful for editors to recreate deleted entities while preserving references
		void RemoveEntity(const EntityHandle& h, bool reserveHandle = false);	
		bool IsHandleValid(const EntityHandle& h) const;
		std::string GetEntityDisplayName(const EntityHandle& h) const;
		size_t GetEntityDisplayName(const EntityHandle& h, char* nameBuffer, size_t maxLength) const;	// returns size of string written to nameBuffer

		std::vector<EntityHandle> GetActiveEntities(uint32_t startIndex=0, uint32_t endIndex=-1) const;	// slow! only for editor/tools/debugging
		template<class It>	// bool(const EntityHandle& e)
		void ForEachActiveEntity(const It&);	// slow! tools/debug only

		// Entity names, try not to abuse this!
		void SetEntityName(const EntityHandle& h, std::string_view name);
		const std::string_view GetEntityName(const EntityHandle& h);
		EntityHandle GetEntityByName(std::string name);		// slow!

		// Components slow path
		bool AddComponent(const EntityHandle& e, std::string_view componentTypeName);
		bool HasComponent(const EntityHandle& e, std::string_view componentTypeName);
		void RemoveComponent(const EntityHandle& e, std::string_view componentTypeName);

		// fast path
		template<class ComponentType>
		void AddComponent(const EntityHandle& e);
		template<class ComponentType>
		ComponentType* GetComponent(const EntityHandle& e);
		template<class ComponentType>
		ComponentType* GetComponentFast(const EntityHandle& e, uint32_t typeIndex);	// danger! no validation on handle, index assumed to be ok
		bool HasAnyComponents(const EntityHandle& e, uint64_t typeBits) const;		// returns true if entity compoonent bitset matches any of the bits
		bool HasAllComponents(const EntityHandle& e, uint64_t typeBits) const;		// returns true if entity compoonent bitset contains all of the bits
		uint32_t GetOwnedComponentCount(const EntityHandle& e);						// returns how many components an entity owns

		// Called from component storage if a component moves in memory
		void OnComponentMoved(const EntityHandle& owner, uint32_t typeIndex, uint32_t oldIndex, uint32_t newIndex);

		void CollectGarbage();		// destroy all entities pending deletion
		size_t GetPendingDeleteCount() { return m_pendingDelete.size(); }
		size_t GetReservedHandleCount() { return m_reservedSlots.size(); }
		size_t GetActiveEntityCount() { return m_allEntities.size() - m_freeEntityIndices.size() - m_reservedSlots.size(); }

		// Storage accessors
		template<class ComponentType> LinearComponentStorage<ComponentType>* GetStorage();
		template<class ComponentType> LinearComponentStorage<ComponentType>* GetStorageFast(uint32_t typeIndex);	// danger! no validation
		ComponentStorage* GetStorage(std::string_view componentTypeName);	// slowpath

		// Pass a vector of handles to serialise a specific set of entities
		JsonSerialiser SerialiseEntities();
		JsonSerialiser SerialiseEntities(const std::vector<EntityHandle>& e);
		// restoreHandles (optional) - serialise entities while restoring previous handles (expects vector to match json exactly)
		std::vector<EntityHandle> SerialiseEntities(const JsonSerialiser& json, const std::vector<EntityHandle>& restoreHandles = {});	
		void SerialiseComponent(const EntityHandle& e, std::string_view componentType, JsonSerialiser& json);		// helper for serialising individual components
		
		std::vector<EntityHandle> Import(std::string_view path);
		bool Load(std::string_view path);
		bool Save(std::string_view path);
		
	private:
		void SerialiseEntity(const EntityHandle& e, JsonSerialiser& target);	// warning, assumes valid handle
		void AddComponentInternal(const EntityHandle& e, uint32_t resolvedTypeIndex);
		struct PerEntityData
		{
			using ComponentBitsetType = uint64_t;
			uint32_t m_publicID = -1;						// used to publicaly identify an entity in a world
			ComponentBitsetType m_ownedComponentBits = 0;	// each bit represents a component type that this entity owns/contains
			std::vector<uint32_t> m_componentIndices;		// index into component storage per type. take care!
			EntityHandle m_parent;							// if a parent exists, it should have a EntityChildrenComponent!
		};
		struct PendingDeleteEntity 
		{
			EntityHandle m_handle;
			bool m_reserveHandle;
		};

		std::string m_name;
		uint32_t m_entityIDCounter = 0;
		std::vector<PerEntityData> m_allEntities;
		std::unordered_map<uint32_t, uint32_t> m_reservedSlots;	// public ID -> free slot index. stores all reserved slots with their public ID
		std::deque<uint32_t> m_freeEntityIndices;			// free list of entity data
		std::vector<std::unique_ptr<ComponentStorage>> m_allComponents;	// storage for all components
		std::vector<PendingDeleteEntity> m_pendingDelete;	// all entities to be deleted (these handles should still all be valid)
		std::vector<std::string> m_allEntityNames;			// kept off hot data path (m_allEntities)
	};

	template<class It>	// bool(const EntityHandle& e)
	void World::ForEachActiveEntity(const It& it)
	{
		if (m_freeEntityIndices.size() == 0 && m_reservedSlots.size() == 0)	// fast path if all slots are allocated
		{
			for (uint32_t i = 0; i < m_allEntities.size(); ++i)
			{
				if (!it(EntityHandle(m_allEntities[i].m_publicID, i)))
				{
					return;
				}
			}
		}
		else
		{
			for (uint32_t i = 0; i < m_allEntities.size(); ++i)
			{
				const uint32_t& id = m_allEntities[i].m_publicID;
				if (id != -1)
				{
					if (!it(EntityHandle(id, i)))
					{
						return;
					}
				}
			}
		}
	}

	template<class ComponentType>
	void World::AddComponent(const EntityHandle& e)
	{
		if (IsHandleValid(e))
		{
			uint32_t typeIndex = ComponentTypeRegistry::GetTypeIndex<ComponentType>();
			assert(typeIndex != -1);
			if (typeIndex != -1)
			{
				AddComponentInternal(e, typeIndex);
			}
		}
	}

	template<class ComponentType>
	ComponentType* World::GetComponentFast(const EntityHandle& e, uint32_t typeIndex)
	{
		const PerEntityData& ped = m_allEntities[e.GetPrivateIndex()];
		const auto testMask = (PerEntityData::ComponentBitsetType)1 << typeIndex;
		if ((ped.m_ownedComponentBits & testMask) == testMask && ped.m_componentIndices.size() > typeIndex)
		{
			LinearComponentStorage<ComponentType>* storage = GetStorageFast<ComponentType>(typeIndex);
			return storage->GetAtIndex(ped.m_componentIndices[typeIndex]);
		}
		return nullptr;
	}

	template<class ComponentType>
	ComponentType* World::GetComponent(const EntityHandle& e)
	{
		if (IsHandleValid(e))
		{
			const uint32_t typeIndex = ComponentTypeRegistry::GetTypeIndex<ComponentType>();
			const PerEntityData& ped = m_allEntities[e.GetPrivateIndex()];
			const auto testMask = (PerEntityData::ComponentBitsetType)1 << typeIndex;
			if ((ped.m_ownedComponentBits & testMask) == testMask && ped.m_componentIndices.size() > typeIndex)
			{
				LinearComponentStorage<ComponentType>* storage = GetStorage<ComponentType>();
				return storage->GetAtIndex(ped.m_componentIndices[typeIndex]);
			}
		}
		return nullptr;
	}

	template<class ComponentType> 
	LinearComponentStorage<ComponentType>* World::GetStorageFast(uint32_t typeIndex)
	{
		return static_cast<LinearComponentStorage<ComponentType>*>(m_allComponents[typeIndex].get());
	}

	template<class ComponentType>
	LinearComponentStorage<ComponentType>* World::GetStorage()
	{
		uint32_t typeIndex = ComponentTypeRegistry::GetTypeIndex<ComponentType>();
		if (typeIndex != -1 && typeIndex < m_allComponents.size())
		{
			return static_cast<LinearComponentStorage<ComponentType>*>(m_allComponents[typeIndex].get());
		}
		return nullptr;
	}
}
}