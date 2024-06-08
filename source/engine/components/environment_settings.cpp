#include "environment_settings.h"
#include "entities/component_storage.h"


namespace R3
{
	void EnvironmentSettingsComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Sky Colour", m_skyColour);
		s("Sun Direction", m_sunDirection);
		s("Sun Colour", m_sunColour);
		s("Sun Brightness", m_sunBrightness);
		s("Sky Ambient", m_skyAmbientFactor);
		s("Sun Ambient", m_sunAmbientFactor);
	}

	void EnvironmentSettingsComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		i.InspectColour("Sky Colour", m_skyColour, InspectProperty(&EnvironmentSettingsComponent::m_skyColour, e, w));
		i.InspectColour("Sun Colour", m_sunColour, InspectProperty(&EnvironmentSettingsComponent::m_sunColour, e, w));
		i.Inspect("Sun Brightness", m_sunBrightness, InspectProperty(&EnvironmentSettingsComponent::m_sunBrightness, e, w), 0.1f, 0.0f, FLT_MAX);
		i.Inspect("Sun Direction", m_sunDirection, InspectProperty(&EnvironmentSettingsComponent::m_sunDirection, e, w), glm::vec3(-1.0f), glm::vec3(1.0f));
		i.Inspect("Sky Ambient Factor", m_skyAmbientFactor, InspectProperty(&EnvironmentSettingsComponent::m_skyAmbientFactor, e, w), 0.0025f, 0.0f, 1.0f);
		i.Inspect("Sun Ambient Factor", m_sunAmbientFactor, InspectProperty(&EnvironmentSettingsComponent::m_sunAmbientFactor, e, w), 0.0025f, 0.0f, 1.0f);
	}
}