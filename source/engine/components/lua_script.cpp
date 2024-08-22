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
			"SetPopulateInputsSource", &LuaScriptComponent::SetPopulateInputsSource,
			"SetPopulateInputsEntrypoint", &LuaScriptComponent::SetPopulateInputsEntrypoint,
			"m_needsRecompile", &LuaScriptComponent::m_needsRecompile,
			"m_isActive", &LuaScriptComponent::m_isActive,
			"m_inputParams", &LuaScriptComponent::m_inputParams
		);
	}

	void LuaScriptComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("OnFixedUpdate", m_onFixedUpdate);
		s("OnVariableUpdate", m_onVariableUpdate);
		s("PopulateInputs", m_populateInputs);
		s("IsActive", m_isActive);
		s("InputParams", m_inputParams);
		if (s.GetMode() == JsonSerialiser::Read)
		{
			m_needsRecompile = true;
		}
	}

	void LuaScriptComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		ImGui::Text("Input Params");
		m_inputParams.Inspect(i);
		ImGui::Separator();
		if (ImGui::CollapsingHeader("On Fixed Update"))
		{
			i.InspectFile(std::format("Source {}##fixed", m_onFixedUpdate.m_sourcePath), m_onFixedUpdate.m_sourcePath, "lua", InspectProperty(&LuaScriptComponent::SetFixedUpdateSource, e, w));
			i.Inspect("Entry point##fixed", m_onFixedUpdate.m_entryPointName, InspectProperty(&LuaScriptComponent::SetFixedUpdateEntrypoint, e, w));
		}
		if (ImGui::CollapsingHeader("On Variable Update"))
		{
			i.InspectFile(std::format("Source {}##variable", m_onVariableUpdate.m_sourcePath), m_onVariableUpdate.m_sourcePath, "lua", InspectProperty(&LuaScriptComponent::SetVariableUpdateSource, e, w));
			i.Inspect("Entry point##variable", m_onVariableUpdate.m_entryPointName, InspectProperty(&LuaScriptComponent::SetVariableUpdateEntrypoint, e, w));
		}
		if (ImGui::CollapsingHeader("Populate Inputs"))
		{
			i.InspectFile(std::format("Source {}##inputs", m_populateInputs.m_sourcePath), m_populateInputs.m_sourcePath, "lua", InspectProperty(&LuaScriptComponent::SetPopulateInputsSource, e, w));
			i.Inspect("Entry point##inputs", m_populateInputs.m_entryPointName, InspectProperty(&LuaScriptComponent::SetPopulateInputsEntrypoint, e, w));
		}
		ImGui::Separator();
		i.Inspect("Is Active", m_isActive, InspectProperty(&LuaScriptComponent::m_isActive, e, w));
		ImGui::Separator();
		if (ImGui::Button("Recompile"))
		{
			m_needsRecompile = true;
		}
		if (ImGui::Button("Repopulate Inputs"))
		{
			m_needsInputPopulate = true;
		}
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

	void LuaScriptComponent::SetPopulateInputsSource(std::string_view src)
	{
		m_populateInputs.m_sourcePath = src;
	}

	void LuaScriptComponent::SetPopulateInputsEntrypoint(std::string_view src)
	{
		m_populateInputs.m_entryPointName = src;
	}

	void LuaScriptComponent::ScriptData::SerialiseJson(JsonSerialiser& s)
	{
		s("SourcePath", m_sourcePath);
		s("EntryPoint", m_entryPointName);
	}
}
