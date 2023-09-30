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

	// Base class used to represent a container of component data
	class ComponentStorage
	{
	public:
		ComponentStorage(World* w, uint32_t typeIndex) : m_ownerWorld(w), m_typeIndex(typeIndex) { }
		virtual ~ComponentStorage() {};
		virtual uint32_t Create(const EntityHandle& e) = 0;	// return index into storage
		virtual bool Contains(const EntityHandle& e) = 0;
		virtual void Destroy(const EntityHandle& e) = 0;
		virtual void DestroyAll() = 0;
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
		virtual bool Contains(const EntityHandle& e);
		virtual void Destroy(const EntityHandle& e);
		virtual void DestroyAll();

		ComponentType* Find(uint32_t entityID);	// find component by entity public ID, does not need a valid handle!

		void ForEach(std::function<void(ComponentType&, const EntityHandle&)> fn);

	private:
		std::unordered_map<uint32_t, uint32_t> m_entityToComponent;	// keep? replace?
		std::vector<EntityHandle> m_owners;	// these are entity IDs
		std::vector<ComponentType> m_components;
		int32_t m_iterationDepth = 0;	// this is a safety net to catch if we delete during iteration
		uint64_t m_generation = 1;		// increases every time the existing pointers/storage are changed 
	};

	template<class ComponentType>
	void LinearComponentStorage<ComponentType>::ForEach(std::function<void(ComponentType&, const EntityHandle&)> fn)
	{
		R3_PROF_EVENT();

		// We need to ensure the integrity of the list during iterations
		// You can safely add components during iteration, but you CANNOT delete them!
		++m_iterationDepth;
		assert(m_owners.size() == m_components.size() && m_entityToComponent.size() == m_owners.size());

		// more safety nets, ensure storage doesn't move
		void* storagePtr = m_components.data();

		auto currentActiveComponents = m_components.size();
		for (int c = 0; c < currentActiveComponents; ++c)
		{
			fn(m_components[c], m_owners[c]);
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
		auto foundEntity = m_entityToComponent.find(entityID);
		if (foundEntity == m_entityToComponent.end())
		{
			return nullptr;
		}
		else
		{
			return &m_components[foundEntity->second];
		}
	}

	template<class ComponentType>
	uint32_t LinearComponentStorage<ComponentType>::Create(const EntityHandle& e)
	{
		R3_PROF_EVENT();

		// potential for fast path here by checking indices in entity data
		bool noDuplicate = Find(e.GetID()) == nullptr;
		assert(noDuplicate);
		if (noDuplicate)
		{
			size_t oldCapacity = m_components.capacity();
			if (m_components.size() + 1 >= oldCapacity)
			{
				++m_generation;	// pointers are about to be invalidated
			}
			m_owners.push_back(e);
			m_components.emplace_back(std::move(ComponentType()));
			uint32_t newIndex = static_cast<uint32_t>(m_components.size() - 1);
			m_entityToComponent.insert({ e.GetID(), newIndex});
			assert(m_owners.size() == m_components.size() && m_entityToComponent.size() == m_owners.size());
			return newIndex;
		}
		return -1;
	}

	template<class ComponentType>
	bool LinearComponentStorage<ComponentType>::Contains(const EntityHandle& e)
	{
		// do we care if the handle is valid, or just the ID?
		// potential for fast path by getting index directly from entity data stored in world
		return Find(e.GetID()) != nullptr;
	}

	template<class ComponentType>
	void LinearComponentStorage<ComponentType>::Destroy(const EntityHandle& e)
	{
		R3_PROF_EVENT();

		if (m_iterationDepth > 0)
		{
			LogError("NO! You cannot delete during iteration!");
			assert(!"NO! You cannot delete during iteration!");
			*((int*)0x0) = 3;	// force crash
		}

		// potential fastpath here if handle is valid (it should be!)
		auto foundEntity = m_entityToComponent.find(e.GetID());
		if (foundEntity != m_entityToComponent.end())
		{
			auto currentIndex = foundEntity->second;
			auto oldIndex = static_cast<uint32_t>(m_owners.size() - 1);

			// A component is about to move from the back to the new index,
			// inform the world so the entity data can be updated correctly
			// we do it first to ensure the entity data NEVER has an out-of-bounds index
			if (currentIndex != oldIndex)
			{
				m_ownerWorld->OnComponentMoved(m_owners[m_owners.size() - 1], m_typeIndex, oldIndex, currentIndex);
			}

			std::iter_swap(m_owners.begin() + currentIndex, m_owners.end() - 1);
			m_owners.pop_back();
			if (m_components.begin() + currentIndex != m_components.end() - 1)
			{
				std::iter_swap(m_components.begin() + currentIndex, m_components.end() - 1);
			}
			m_components.pop_back();

			// we may want to be more fancy and only invalidate particular handles, but for now this works
			++m_generation;

			if (currentIndex < m_owners.size())	// why? investigate
			{
				// fix up the lookup for the component we just moved
				auto ownerEntity = m_owners[currentIndex];
				m_entityToComponent[ownerEntity.GetID()] = currentIndex;
			}

			// remove the old lookup
			m_entityToComponent.erase(foundEntity);
		}

		assert(m_owners.size() == m_components.size() && m_entityToComponent.size() == m_owners.size());
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

		m_entityToComponent.clear();
		m_owners.clear();
		m_components.clear();
		++m_generation;
	}
}
}