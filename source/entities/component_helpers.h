#pragma once

// Pretty much all components will be implementing serialisation and inspection
#include "entities/world.h"
#include "engine/serialiser.h"
#include "engine/value_inspector.h"

// Some useful utilities for implementing components
// These are all optional, but handy!
namespace R3
{
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
}