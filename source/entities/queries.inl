#pragma once

#include "component_type_registry.h"
#include "component_storage.h"
#include "entity_handle.h"
#include "world.h"

namespace R3
{
namespace Entities
{
namespace Queries
{
	template<class ComponentType, class It>
	void ForEach(World* w, const It& fn)
	{
		R3_PROF_EVENT();
		LinearComponentStorage<ComponentType>* storage = w->GetStorage<ComponentType>();
		if (storage != nullptr)
		{
			storage->ForEach(fn);
		}
	}

	template<class ComponentType1, class ComponentType2, class It>
	void ForEach(World* w, const It& fn)
	{
		R3_PROF_EVENT();
		// choose which component type to iterate on based on smallest total count
		if (w->GetStorage<ComponentType1>()->GetTotalCount() < w->GetStorage<ComponentType2>()->GetTotalCount())
		{
			const uint32_t type2Index = ComponentTypeRegistry::GetTypeIndex<ComponentType2>();
			auto forEachCmp1 = [w, fn, type2Index](const EntityHandle& e, ComponentType1& cmp1)
			{
				ComponentType2* cmp2 = w->GetComponentFast<ComponentType2>(e, type2Index);
				if (cmp2)
				{
					return fn(e, cmp1, *cmp2);
				}
				return true;
			};
			ForEach<ComponentType1>(w, forEachCmp1);
		}
		else
		{
			const uint32_t type1Index = ComponentTypeRegistry::GetTypeIndex<ComponentType1>();
			auto forEachCmp2 = [w, fn, type1Index](const EntityHandle& e, ComponentType2& cmp2)
			{
				ComponentType1* cmp1 = w->GetComponentFast<ComponentType1>(e, type1Index);
				if (cmp1)
				{
					return fn(e, *cmp1, cmp2);
				}
				return true;
			};
			ForEach<ComponentType2>(w, forEachCmp2);
		}
	}
}
}
}