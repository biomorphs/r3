#pragma once 
#include <vector>
#include <array>

// Vector uses less memory but much slower iteration due to cache misses
// #define R3_ENTITY_INDICES_IN_VECTOR

namespace R3
{
	// Contains a lookup table of component indices for a single entity + bitset to determine which components exist
	// The bitset is invalidated when an entity is removed (to stop subsequent lookups). However the indices are NOT invalidated until garbage collection
	// This allows us to defer component destruction until a safe point in the frame
	class EntityComponentLookup
	{
	public:
		EntityComponentLookup();
		~EntityComponentLookup();

		void AddComponent(uint32_t typeIndex, uint32_t componentIndex);
		uint32_t RemoveComponent(uint32_t typeIndex);	// remove the component of specified type (bitset + index), returns the old index
		void Invalidate();	// resets the bitfield but does not touch the stored indices (used when deleting entities)
		void Reset();		// clear bitset + all indices
		void UpdateIndex(uint32_t typeIndex, uint32_t oldIndex, uint32_t newIndex);	// called when a component moves, validates previous state

		bool IsEmpty() const;									// returns true if no components exist for this entity
		bool ContainsComponent(uint32_t typeIndex) const;		// returns true if a specific valid component exists (bits + index must be valid)
		bool ContainAll(uint64_t typeMask) const;				// returns true if all the component types exist (uses a bitmask for each type)
		bool ContainAny(uint64_t typeMask) const;				// returns true if any component types exist (uses a bitmask for each type)

		uint32_t GetComponentIndex(uint32_t typeIndex) const;	// get the storage index of a component for a particular type, or -1 if none exists. Only returns valid lookups
		uint32_t GetInvalidatedIndex(uint32_t typeIndex) const;	// get the index for a type even when the bitset is not set
		uint32_t GetValidComponentCount() const;				// get the number of components stored

	private:
		using ComponentBitsetType = uint64_t;
		ComponentBitsetType m_ownedComponentBits = 0;

#ifdef R3_ENTITY_INDICES_IN_VECTOR
		std::vector<uint32_t> m_componentIndices;		// index into component storage per type. take care!
#else
		std::array<uint32_t, 64> m_componentIndices;	// testing
#endif
	};
}

#include "entity_component_lookup.inl"