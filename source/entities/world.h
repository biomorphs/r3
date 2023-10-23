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

		// Entity stuff
		EntityHandle AddEntity();
		void RemoveEntity(const EntityHandle& h);	// defers actual deletion until CollectGarbage() called
		bool IsHandleValid(const EntityHandle& h) const;
		std::vector<EntityHandle> GetActiveEntities(uint32_t startIndex=0, uint32_t endIndex=-1) const;	// slow! only for editor/tools/debugging
		size_t GetEntityDisplayName(const EntityHandle& h, char* nameBuffer, size_t maxLength) const;	// returns size of string written to nameBuffer
		std::string GetEntityDisplayName(const EntityHandle& h) const;
		template<class It>	// bool(const EntityHandle& e)
		void ForEachActiveEntity(const It&);	// slow! tools/debug only

		// Components slow path
		void AddComponent(const EntityHandle& e, std::string_view componentTypeName);
		bool HasComponent(const EntityHandle& e, std::string_view componentTypeName);

		// fast path
		template<class ComponentType>
		void AddComponent(const EntityHandle& e);
		template<class ComponentType>
		ComponentType* GetComponent(const EntityHandle& e);
		template<class ComponentType>
		ComponentType* GetComponentFast(const EntityHandle& e, uint32_t typeIndex);	// danger! no validation on handle, index assumed to be ok
		bool HasAnyComponents(const EntityHandle& e, uint64_t typeBits) const;		// returns true if entity compoonent bitset matches any of the bits
		bool HasAllComponents(const EntityHandle& e, uint64_t typeBits) const;		// returns true if entity compoonent bitset contains all of the bits

		// Called from component storage if a component moves in memory
		void OnComponentMoved(const EntityHandle& owner, uint32_t typeIndex, uint32_t oldIndex, uint32_t newIndex);

		void CollectGarbage();		// destroy all entities pending deletion
		size_t GetPendingDeleteCount() { return m_pendingDelete.size(); }
		size_t GetActiveEntityCount() { return m_allEntities.size() - m_freeEntityIndices.size(); }

		// Storage accessors
		template<class ComponentType> LinearComponentStorage<ComponentType>* GetStorage();
		template<class ComponentType> LinearComponentStorage<ComponentType>* GetStorageFast(uint32_t typeIndex);	// danger! no validation
		ComponentStorage* GetStorage(std::string_view componentTypeName);	// slowpath

		// Pass a vector of handles to serialise a specific set of entities
		JsonSerialiser SerialiseEntities(std::vector<EntityHandle> e = {});
		bool Save(std::string_view path);
		
	private:
		void AddComponentInternal(const EntityHandle& e, uint32_t resolvedTypeIndex);
		struct PerEntityData
		{
			using ComponentBitsetType = uint64_t;
			uint32_t m_publicID = -1;						// used to publicaly identify an entity in a world
			ComponentBitsetType m_ownedComponentBits = 0;	// each bit represents a component type that this entity owns/contains
			std::vector<uint32_t> m_componentIndices;		// index into component storage per type. take care!
		};

		std::string m_name;
		uint32_t m_entityIDCounter = 0;
		std::unordered_map<uint32_t, uint32_t> m_entityIdToIndex;	// used for faster lookups when only ID is known
		std::vector<PerEntityData> m_allEntities;
		std::deque<uint32_t> m_freeEntityIndices;			// free list of entity data
		std::vector<std::unique_ptr<ComponentStorage>> m_allComponents;	// storage for all components
		std::vector<EntityHandle> m_pendingDelete;	// all entities to be deleted (these handles should still all be valid)
	};

	template<class It>	// bool(const EntityHandle& e)
	void World::ForEachActiveEntity(const It& it)
	{
		if (m_freeEntityIndices.size() == 0)	// fast path if all slots are allocated
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
			AddComponentInternal(e, typeIndex);
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