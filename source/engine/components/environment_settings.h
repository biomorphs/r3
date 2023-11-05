#pragma once

#include "entities/component_helpers.h"
#include "core/glm_headers.h"

namespace R3
{
	// Contains global environment state
	class EnvironmentSettingsComponent
	{
	public:
		static std::string_view GetTypeName() { return "EnvironmentSettings"; }
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		glm::vec4 m_clearColour;	// controls clear colour of main render pass
	};
}