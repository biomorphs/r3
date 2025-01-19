#pragma once

// Pretty much all components will be implementing serialisation and inspection
#include "entities/world.h"
#include "entities/component_storage.h"
#include "engine/serialiser.h"
#include "engine/ui/value_inspector.h"

// Some useful utilities for implementing components
// These are all optional, but handy!
namespace R3
{
	class LuaSystem;

	// Inspector helpers
	// these return a lambda that can be used with ValueInspector
	// hides the boilerplate of getting the component ptr through the world each time
	// (enabled undo/redo on components that move around in memory)

	// takes a pointer to member as a parameter, this member will be set when the lambda is called
	template<class ComponentType, class PropertyType>
	auto InspectProperty(PropertyType(ComponentType::* valuePtr), const Entities::EntityHandle& e, Entities::World* w)
	{
		return [e, w, valuePtr](PropertyType newValue)
		{
			auto foundCmp = w->GetComponent<ComponentType>(e);
			if (foundCmp)
			{
				foundCmp->*valuePtr = newValue;
			}
		};
	}

	// takes a pointer to member *function* as a parameter, this will be called when the value is set
	template<class ComponentType, class PropertyType>
	auto InspectProperty(void(ComponentType::* fn)(PropertyType), const Entities::EntityHandle& e, Entities::World* w)
	{
		return [e, w, fn](PropertyType newValue)
		{
			auto foundCmp = w->GetComponent<ComponentType>(e);
			if (foundCmp)
			{
				(foundCmp->*fn)(newValue);
			}
			};
	}

	// wraps a lambda fn that is called with signature void(const PropertyType&, EntityHandle, World)
	// useful as a 'passthrough' for complex components (i.e. lists of stuff need custom inspector, see StaticMeshMaterialsComponent)
	template<class ComponentType, class PropertyType>
	using CustomComponentInspector = std::function<void(const Entities::EntityHandle&, ComponentType&, Entities::World*, PropertyType)>;

	template<class ComponentType, class PropertyType>
	auto InspectComponentCustom(const Entities::EntityHandle& e, Entities::World* w, CustomComponentInspector<ComponentType, PropertyType> callFn)
	{
		return[e, w, callFn](PropertyType newValue)
		{
			auto foundCmp = w->GetComponent<ComponentType>(e);
			if (foundCmp)
			{
				callFn(e, *foundCmp, w, newValue);
			}
		};
	}
}