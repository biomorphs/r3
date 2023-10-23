#include "environment_settings.h"
#include "entities/component_storage.h"
#include "engine/serialiser.h"
#include "engine/value_inspector.h"

namespace R3
{
	void EnvironmentSettingsComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Clear Colour", m_clearColour);
	}

	void EnvironmentSettingsComponent::Inspect(const Entities::EntityHandle& e, Entities::World& w, ValueInspector& i)
	{
		Entities::World* worldPtr = &w;
		i.InspectColour("Clear Colour", m_clearColour, [e, worldPtr](glm::vec4 v) {
			worldPtr->GetComponent<EnvironmentSettingsComponent>(e)->m_clearColour = v;
		});
	}
}