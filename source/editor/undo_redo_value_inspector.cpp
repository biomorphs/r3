#include "undo_redo_value_inspector.h"
#include "commands/set_value_cmd.h"
#include "editor/editor_command_list.h"
#include "imgui.h"

namespace R3
{
	constexpr float c_floatEpsilon = 0.00001f;		// smallest change to a float value we register as a modification

	bool UndoRedoInspector::Inspect(std::string_view label, int currentValue, std::function<void(int)> setFn, int step, int minv, int maxv)
	{
		int val = currentValue;
		if (ImGui::InputInt(label.data(), &val, step))
		{
			val = glm::clamp(val, minv, maxv);
			if (glm::abs(val-currentValue) > c_floatEpsilon)
			{
				m_cmds.Push(std::make_unique<SetValueCommand<int>>(label, currentValue, val, setFn));
				return true;
			}
		}
		return false;
	}

	bool UndoRedoInspector::Inspect(std::string_view label, float currentValue, std::function<void(float)> setFn, float step, float minv, float maxv)
	{
		float val = currentValue;
		if (ImGui::InputFloat(label.data(), &val, step))
		{
			val = glm::clamp(val, minv, maxv);
			if (glm::abs(val - currentValue) > c_floatEpsilon)
			{
				m_cmds.Push(std::make_unique<SetValueCommand<float>>(label, currentValue, val, setFn));
				return true;
			}
		}
		return false;
	}

	bool UndoRedoInspector::Inspect(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn, glm::vec3 minv, glm::vec3 maxv)
	{
		constexpr float errorMargin = 0.000001f;
		float availableWidth = ImGui::GetContentRegionAvail().x;

		std::string inputId = std::string("##") + label.data() + "_value";
		glm::vec3 newValue = currentValue;
		ImGui::InputFloat3(label.data(), glm::value_ptr(newValue), "%f");
		newValue = glm::max(minv, glm::min(newValue, maxv));

		if (glm::distance(newValue,currentValue) >= c_floatEpsilon)
		{
			m_cmds.Push(std::make_unique<SetValueCommand<glm::vec3>>(label, currentValue, newValue, setFn));
			return true;
		}
		return false;
	}

	bool UndoRedoInspector::InspectColour(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn)
	{
		glm::vec4 val = currentValue;
		if (ImGui::ColorEdit4(label.data(), glm::value_ptr(val)))
		{
			if (val != currentValue)
			{
				m_cmds.Push(std::make_unique<SetValueCommand<glm::vec4>>(label, currentValue, val, setFn));
				return true;
			}
		}
		return false;
	}
}