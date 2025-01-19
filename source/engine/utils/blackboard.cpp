#include "blackboard.h"
#include "engine/serialiser.h"
#include "engine/systems/lua_system.h"
#include <imgui.h>

namespace R3
{
	void Blackboard::RegisterScripts(LuaSystem& s)
	{
		s.RegisterType<Blackboard>("Blackboard",
			sol::constructors<Blackboard()>(),
			"Reset", &Blackboard::Reset,
			"AddIntVec2", &Blackboard::AddIntVec2,
			"GetIntVec2", &Blackboard::GetIntVec2,
			"AddInt", &Blackboard::AddInt,
			"GetInt", &Blackboard::GetInt,
			"AddFloat", &Blackboard::AddFloat,
			"GetFloat", &Blackboard::GetFloat
		);
	}

	void Blackboard::SerialiseJson(JsonSerialiser& s)
	{
		s("IntVec2s", m_intVec2s);
		s("Ints", m_ints);
		s("Floats", m_floats);
	}

	void Blackboard::Inspect(ValueInspector&)
	{
		for (auto& r : m_intVec2s)
		{
			std::string_view name = r.first;
			glm::ivec2 val = r.second;
			if (ImGui::InputInt2(name.data(), glm::value_ptr(val)))
			{
				r.second = val;
			}
		}
		for (auto& r : m_ints)
		{
			std::string_view name = r.first;
			int val = r.second;
			if (ImGui::InputInt(name.data(), &val, 1, 5))
			{
				r.second = val;
			}
		}
		for (auto& r : m_floats)
		{
			std::string_view name = r.first;
			float val = r.second;
			if (ImGui::InputFloat(name.data(), &val, 0.1f, 1.0f))
			{
				r.second = val;
			}
		}
	}

	void Blackboard::Reset()
	{
		m_intVec2s.clear();
		m_ints.clear();
		m_floats.clear();
	}
}