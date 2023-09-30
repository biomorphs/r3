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
				LinearComponentStorage<ComponentType>* storage = w->GetStorage<ComponentType>();
				if (storage != nullptr)
				{
					storage->ForEach(fn);
				}
			}

			template<class ComponentType1, class ComponentType2, class It>
			void ForEach(World* w, const It& fn)
			{
				auto forEachCmp1 = [w, fn](const EntityHandle& e, ComponentType1& cmp1)
				{
					ComponentType2* cmp2 = w->GetComponent<ComponentType2>(e);
					if (cmp2)
					{
						return fn(e, cmp1, *cmp2);
					}
					return true;
				};
				ForEach<ComponentType1>(w, forEachCmp1);
			}
		}
	}
}