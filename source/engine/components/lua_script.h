#pragma once

#include "entities/component_helpers.h"
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

		struct CompiledFunction;
		struct ScriptData
		{
			void SerialiseJson(JsonSerialiser& s);
			std::string m_sourcePath;
			std::string m_entryPointName;	// function(EntityHandle owner)
			sol::protected_function m_fn;	// compiled function
		};
		ScriptData m_onFixedUpdate;
		ScriptData m_onVariableUpdate;
		bool m_needsRecompile = false;
		bool m_isActive = false;
	};
}