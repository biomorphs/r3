#pragma once

#include "entities/component_helpers.h"
#include "engine/utils/blackboard.h"
#include <sol/sol.hpp>

namespace R3
{
	class LuaScriptComponent
	{
	public:
		static std::string_view GetTypeName() { return "LuaScript"; }
		static void RegisterScripts(class LuaSystem&);
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		void SetFixedUpdateSource(std::string_view src);
		void SetFixedUpdateEntrypoint(std::string_view src);

		void SetVariableUpdateSource(std::string_view src);
		void SetVariableUpdateEntrypoint(std::string_view src);

		void SetPopulateInputsSource(std::string_view src);
		void SetPopulateInputsEntrypoint(std::string_view src);

		struct ScriptData
		{
			void SerialiseJson(JsonSerialiser& s);
			std::string m_sourcePath;
			std::string m_entryPointName;	// function(EntityHandle owner)
			sol::protected_function m_fn;	// compiled function
		};
		Blackboard m_inputParams;			// pass inputs to script via blackboard
		ScriptData m_populateInputs;		// lets scripts declare input params by modifying blackboard
		ScriptData m_onFixedUpdate;
		ScriptData m_onVariableUpdate;
		bool m_needsRecompile = false;
		bool m_needsInputPopulate = false;
		bool m_isActive = false;
	};
}