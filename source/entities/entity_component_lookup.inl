#include "entity_component_lookup.h"
#include <bit>

namespace R3
{
	inline EntityComponentLookup::EntityComponentLookup()
	{
#ifndef R3_ENTITY_INDICES_IN_VECTOR
		m_componentIndices.fill(-1);
#endif
	}

	inline EntityComponentLookup::~EntityComponentLookup()
	{

	}

	inline void R3::EntityComponentLookup::AddComponent(uint32_t typeIndex, uint32_t componentIndex)
	{
		auto newBits = 1ull << typeIndex;
		m_ownedComponentBits |= newBits;
#ifdef R3_ENTITY_INDICES_IN_VECTOR
		if (m_componentIndices.size() < typeIndex + 1)
		{
			m_componentIndices.resize(typeIndex + 1, -1);
		}
#endif
		m_componentIndices[typeIndex] = componentIndex;
	}

	inline uint32_t EntityComponentLookup::RemoveComponent(uint32_t typeIndex)
	{
		uint32_t oldIndex = -1;
		const auto typeMask = 1ull << typeIndex;
		if (ContainAll(typeMask))
		{
			m_ownedComponentBits &= ~typeMask;
			oldIndex = m_componentIndices[typeIndex];
			m_componentIndices[typeIndex] = -1;	
		}
		return oldIndex;
	}

	inline void EntityComponentLookup::Invalidate()
	{
		m_ownedComponentBits = 0;
	}

	inline void EntityComponentLookup::Reset()
	{
#ifdef R3_ENTITY_INDICES_IN_VECTOR
		m_componentIndices.clear();	// clear out the old values but keep the memory around
#else
		m_componentIndices.fill(-1);
#endif
		m_ownedComponentBits = 0;
	}

	inline void EntityComponentLookup::UpdateIndex(uint32_t typeIndex, uint32_t oldIndex, uint32_t newIndex)
	{
#ifdef R3_ENTITY_INDICES_IN_VECTOR
		assert(typeIndex < m_componentIndices.size());
#endif
		assert(m_componentIndices[typeIndex] == oldIndex);
		m_componentIndices[typeIndex] = newIndex;
	}

	inline bool EntityComponentLookup::IsEmpty() const
	{
#ifdef R3_ENTITY_INDICES_IN_VECTOR
		return m_componentIndices.size() == 0 && m_ownedComponentBits == 0;
#else
		return m_ownedComponentBits == 0;
#endif
	}

	inline bool EntityComponentLookup::ContainsComponent(uint32_t typeIndex) const
	{
		return GetComponentIndex(typeIndex) != -1;
	}

	inline bool EntityComponentLookup::ContainAll(uint64_t typeMask) const
	{
		return (m_ownedComponentBits & typeMask) == typeMask;
	}

	inline bool EntityComponentLookup::ContainAny(uint64_t typeMask) const
	{
		return (m_ownedComponentBits & typeMask) != 0;
	}

	inline uint32_t EntityComponentLookup::GetComponentIndex(uint32_t typeIndex) const
	{
		uint32_t cmpIndex = -1;
		const auto testMask = 1ull << typeIndex;
		if ((m_ownedComponentBits & testMask) == testMask)
		{
			assert(m_componentIndices.size() > typeIndex);
			cmpIndex = m_componentIndices[typeIndex];
		}
		return cmpIndex;
	}

	inline uint32_t EntityComponentLookup::GetInvalidatedIndex(uint32_t typeIndex) const
	{
#ifdef R3_ENTITY_INDICES_IN_VECTOR
		if (m_componentIndices.size() > typeIndex)
#endif
		{
			return m_componentIndices[typeIndex];
		}
	}

	inline uint32_t EntityComponentLookup::GetValidComponentCount() const
	{
		return std::popcount(m_ownedComponentBits);
	}
}