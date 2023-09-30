#pragma once 
#include "entity_handle.h"
#include "component_type_registry.h"
#include <string_view>
#include <vector>
#include <deque>
#include <memory>
#include <unordered_map>
#include <functional>

namespace R3
{
namespace Entities
{
	class ComponentStorage;
	class World
	{
	public:
		World();
		~World();

		void SetName(std::string_view name) { m_name = name; }
		std::string_view GetName() const { return m_name; }

		EntityHandle AddEntity();
		void RemoveEntity(const EntityHandle& h);	// defers actual deletion until CollectGarbage() called
		bool IsHandleValid(const EntityHandle& h) const;

		// slow path
		void AddComponent(EntityHandle e, std::string_view componentTypeName);

		void CollectGarbage();						// destroy all entities pending deletion
		size_t GetPendingDeleteCount() { return m_pendingDelete.size(); }

		size_t GetActiveEntityCount() { return m_allEntities.size() - m_freeEntityIndices.size(); }

		// Called from component storage if a component moves in memory
		void OnComponentMoved(const EntityHandle& owner, uint32_t typeIndex, uint32_t oldIndex, uint32_t newIndex);

		// Storage accessors

	private:
		
		std::string m_name;
		struct PerEntityData
		{
			using ComponentBitsetType = uint64_t;
			uint32_t m_publicID = -1;						// used to publicaly identify an entity in a world
			ComponentBitsetType m_ownedComponentBits = 0;	// each bit represents a component type that this entity owns/contains
			std::vector<uint32_t> m_componentIndices;		// index into component storage per type. take care!
		};
		uint32_t m_entityIDCounter = 0;
		std::unordered_map<uint32_t, uint32_t> m_entityIdToIndex;	// used for faster lookups when only ID is known
		std::vector<PerEntityData> m_allEntities;
		std::deque<uint32_t> m_freeEntityIndices;			// free list of entity data
		std::vector<std::unique_ptr<ComponentStorage>> m_allComponents;	// storage for all components
		std::vector<EntityHandle> m_pendingDelete;	// all entities to be deleted (these handles should still all be valid)
	};
}
}