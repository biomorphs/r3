#include "lua_script.h"
#include "engine/systems/lua_system.h"
#include <imgui.h>
#include <sol/sol.hpp>

namespace R3
{
	void LuaScriptComponent::RegisterScripts(LuaSystem& l)
	{
		l.RegisterType<LuaScriptComponent>(GetTypeName(),
			"SetFixedUpdateSource", &LuaScriptComponent::SetFixedUpdateSource,
			"SetFixedUpdateEntrypoint", &LuaScriptComponent::SetFixedUpdateEntrypoint,
			"SetVariableUpdateSource", &LuaScriptComponent::SetVariableUpdateSource,
			"SetVariableUpdateEntrypoint", &LuaScriptComponent::SetVariableUpdateEntrypoint,
			"m_needsRecompile", &LuaScriptComponent::m_needsRecompile,
			"m_isActive", &LuaScriptComponent::m_isActive
		);
	}

	void LuaScriptComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("OnFixedUpdate", m_onFixedUpdate);
		s("OnVariableUpdate", m_onVariableUpdate);
		s("IsActive", m_isActive);
		if (s.GetMode() == JsonSerialiser::Read)
		{
			m_needsRecompile = true;
		}
	}

	void LuaScriptComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		ImGui::Text("On Fixed Update");
		i.InspectFile(std::format("Source {}##fixed", m_onFixedUpdate.m_sourcePath), m_onFixedUpdate.m_sourcePath, "lua", InspectProperty(&LuaScriptComponent::SetFixedUpdateSource, e, w));
		i.Inspect("Entry point##fixed", m_onFixedUpdate.m_entryPointName, InspectProperty(&LuaScriptComponent::SetFixedUpdateEntrypoint, e, w));
		ImGui::Separator();
		ImGui::Text("On Variable Update");
		i.InspectFile(std::format("Source {}##variable", m_onVariableUpdate.m_sourcePath), m_onVariableUpdate.m_sourcePath, "lua", InspectProperty(&LuaScriptComponent::SetVariableUpdateSource, e, w));
		i.Inspect("Entry point##variable", m_onVariableUpdate.m_entryPointName, InspectProperty(&LuaScriptComponent::SetVariableUpdateEntrypoint, e, w));
		if (ImGui::Button("Recompile"))
		{
			m_needsRecompile = true;
		}
		i.Inspect("Is Active", m_isActive, InspectProperty(&LuaScriptComponent::m_isActive, e, w));
	}

	void LuaScriptComponent::SetFixedUpdateSource(std::string_view src)
	{
		m_onFixedUpdate.m_sourcePath = src;
	}

	void LuaScriptComponent::SetFixedUpdateEntrypoint(std::string_view src)
	{
		m_onFixedUpdate.m_entryPointName = src;
	}

	void LuaScriptComponent::SetVariableUpdateSource(std::string_view src)
	{
		m_onVariableUpdate.m_sourcePath = src;
	}

	void LuaScriptComponent::SetVariableUpdateEntrypoint(std::string_view src)
	{
		m_onVariableUpdate.m_entryPointName = src;
	}

	void LuaScriptComponent::ScriptData::SerialiseJson(JsonSerialiser& s)
	{
		s("SourcePath", m_sourcePath);
		s("EntryPoint", m_entryPointName);
	}
}
