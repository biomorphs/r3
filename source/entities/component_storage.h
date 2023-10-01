#pragma once

#include "entity_handle.h"
#include "world.h"
#include "core/profiler.h"
#include "core/log.h"
#include <vector>
#include <unordered_map>
#include <cassert>
#include <algorithm>

namespace R3
{
namespace Entities
{
	class World;

	// Base class used just so we can have an array of these things
	class ComponentStorage
	{
	public:
		ComponentStorage(World* w, uint32_t typeIndex) : m_ownerWorld(w), m_typeIndex(typeIndex) { }
		virtual ~ComponentStorage() {};

		// Base API does not know anything about the underlying component type
		virtual uint32_t Create(const EntityHandle& e) = 0;	// return index into storage
		virtual void Destroy(const EntityHandle& e, uint32_t index) = 0;	// you must know the index to destroy a component (for speed)
		virtual void DestroyAll() = 0;
		virtual void Serialise(const EntityHandle& e, uint32_t index, JsonSerialiser& s) = 0;
	protected:
		World* m_ownerWorld = nullptr;
		uint32_t m_typeIndex = -1;
	};

	// a linear array of packed components
	template<class ComponentType>
	class LinearComponentStorage : public ComponentStorage
	{
	public:
		LinearComponentStorage(World* w, uint32_t typeIndex) : ComponentStorage(w, typeIndex) {}
		virtual uint32_t Create(const EntityHandle& e);
		virtual void Destroy(const EntityHandle& e, uint32_t index);
		virtual void DestroyAll();
		virtual void Serialise(const EntityHandle& e, uint32_t index, JsonSerialiser& s);

		// Fastpath API
		ComponentType* GetAtIndex(uint32_t index);	// fastest path, direct random access, but no safety! not even a bounds check in release

		// Slowpath API, only use for debugging
		ComponentType* Find(uint32_t entityID);		// find component by entity public ID, does not need a valid handle!

		// It = bool(const EntityHandle& e, ComponentType& cmp)
		// Returns early if the iterator returns false
		template<class It>
		void ForEach(const It& fn);

	private:
		std::vector<EntityHandle> m_owners;	// these are entity IDs
		std::vector<ComponentType> m_components;
		int32_t m_iterationDepth = 0;	// this is a safety net to catch if we delete during iteration
		uint64_t m_generation = 1;		// increases every time the existing pointers/storage are changed 
	};

	template<class ComponentType>
	void LinearComponentStorage<ComponentType>::Serialise(const EntityHandle& e, uint32_t index, JsonSerialiser& s)
	{
		if (index == -1 || index >= m_owners.size() || e.GetID() == -1 || e.GetPrivateIndex() == -1)
		{
			return;
		}

		// ensure the handle + index match up
		assert(m_owners[index] == e);
		if (m_owners[index] == e)
		{
			s(ComponentType::GetTypeName(), *GetAtIndex(index));
		}
	}

	template<class ComponentType>
	ComponentType* LinearComponentStorage<ComponentType>::GetAtIndex(uint32_t index)
	{
		assert(index < m_components.size());
		return &m_components[index];
	}

	template<class ComponentType>
	template<class It>
	void LinearComponentStorage<ComponentType>::ForEach(const It& fn)
	{
		R3_PROF_EVENT();

		// We need to ensure the integrity of the list during iterations
		// You can safely add components during iteration, but you CANNOT delete them!
		++m_iterationDepth;
		assert(m_owners.size() == m_components.size());

		// more safety nets, ensure storage doesn't move
		void* storagePtr = m_components.data();

		auto currentActiveComponents = m_components.size();
		for (int c = 0; c < currentActiveComponents; ++c)
		{
			if (!fn(m_owners[c], m_components[c]))
				break;
		}

		if (storagePtr != m_components.data())
		{
			LogError("NO! Storage ptr was changed during iteration!");
			assert(!"NO! Storage ptr was changed during iteration!");
			*((int*)0x0) = 3;	// force crash
		}

		--m_iterationDepth;
	}

	template<class ComponentType>
	ComponentType* LinearComponentStorage<ComponentType>::Find(uint32_t entityID)
	{
		auto found = std::find_if(m_owners.begin(), m_owners.end(), [entityID](const EntityHandle& e) {
			return e.GetID() == entityID;
		});
		if (found == m_owners.end())
		{
			return nullptr;
		}
		else
		{
			const auto index = std::distance(m_owners.begin(), found);
			return &m_components[index];
		}
	}

	template<class ComponentType>
	uint32_t LinearComponentStorage<ComponentType>::Create(const EntityHandle& e)
	{
		R3_PROF_EVENT();

		// assert(Find(e.GetID()) == nullptr);		// safety check for duplicates (very slow)
		size_t oldCapacity = m_components.capacity();
		if (m_components.size() + 1 >= oldCapacity)
		{
			++m_generation;	// pointers are about to be invalidated
		}
		m_owners.push_back(e);
		m_components.emplace_back(std::move(ComponentType()));
		assert(m_owners.size() == m_components.size());
		
		return static_cast<uint32_t>(m_components.size() - 1);
	}

	template<class ComponentType>
	void LinearComponentStorage<ComponentType>::Destroy(const EntityHandle& e, uint32_t index)
	{
		R3_PROF_EVENT();
		if (index == -1 || index >= m_owners.size() || e.GetID() == -1 || e.GetPrivateIndex() == -1)
		{
			return;
		}

		if (m_iterationDepth > 0)
		{
			LogError("NO! You cannot delete during iteration!");
			assert(!"NO! You cannot delete during iteration!");
			*((int*)0x0) = 3;	// force crash
		}

		// ensure the handle + index match up
		if(m_owners[index] == e)
		{
			auto oldIndex = static_cast<uint32_t>(m_owners.size() - 1);
			if (index != oldIndex)
			{
				// A component is about to move from the back to this index,
				// inform the world so the entity data can be updated correctly
				// we do it first to ensure the entity data NEVER has an out-of-bounds index
				m_ownerWorld->OnComponentMoved(m_owners[oldIndex], m_typeIndex, oldIndex, index);
				std::iter_swap(m_owners.begin() + index, m_owners.end() - 1);
				std::iter_swap(m_components.begin() + index, m_components.end() - 1);
			}
			m_owners.pop_back();
			m_components.pop_back();

			// we may want to be more fancy and only invalidate particular handles, but for now this works
			++m_generation;
		}
	}

	template<class ComponentType>
	void LinearComponentStorage<ComponentType>::DestroyAll()
	{
		R3_PROF_EVENT();

		if (m_iterationDepth > 0)
		{
			LogError("NO! You cannot delete during iteration!");
			assert(!"NO! You cannot delete during iteration!");
			*((int*)0x0) = 3;	// force crash
		}

		m_owners.clear();
		m_components.clear();
		++m_generation;
	}
}
}