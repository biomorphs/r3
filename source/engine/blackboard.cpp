#include "blackboard.h"
#include "serialiser.h"
#include "systems/lua_system.h"
#include <imgui.h>

namespace R3
{
	void Blackboard::RegisterScripts(LuaSystem& s)
	{
		s.RegisterType<Blackboard>("Blackboard",
			sol::constructors<Blackboard()>(),
			"Reset", &Blackboard::Reset,
			"TryAddIntVec2", &Blackboard::TryAddIntVec2,
			"GetIntVec2", &Blackboard::GetIntVec2
		);
	}

	void Blackboard::SerialiseJson(JsonSerialiser& s)
	{
		s("IntVec2s", m_intVec2s);
	}

	void Blackboard::Inspect(ValueInspector&)
	{
		for (auto& r : m_intVec2s)
		{
			std::string_view name = r.first;
			glm::ivec2 val = r.second;
			if (ImGui::InputInt2(name.data(), glm::value_ptr(val), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				r.second = val;
			}
		}
	}

	void Blackboard::Reset()
	{
		m_intVec2s.clear();
	}
}