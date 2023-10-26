#include "environment_settings.h"
#include "entities/component_storage.h"


namespace R3
{
	void EnvironmentSettingsComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Clear Colour", m_clearColour);
	}

	void EnvironmentSettingsComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		i.InspectColour("Clear Colour", m_clearColour, InspectProperty(&EnvironmentSettingsComponent::m_clearColour, e, w));
	}
}