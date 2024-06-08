#pragma once

#include "entities/component_helpers.h"
#include "core/glm_headers.h"

namespace R3
{
	// Contains global (well, per world) environment settings
	class EnvironmentSettingsComponent
	{
	public:
		static std::string_view GetTypeName() { return "EnvironmentSettings"; }
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		glm::vec3 m_skyColour = {};
		glm::vec3 m_sunDirection{ -.1,-1,-.1 };	// normalise before use
		glm::vec3 m_sunColour = {1,1,1};
		float m_sunBrightness = 1.0f;
		float m_skyAmbientFactor = 0.005f;		// ambient from the sky 
		float m_sunAmbientFactor = 0.008f;		// ambient from the sun
	};
}