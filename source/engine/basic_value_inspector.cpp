#include "basic_value_inspector.h"
#include "imgui.h"

namespace R3
{
	bool BasicValueInspector::Inspect(std::string_view label, int currentValue, std::function<void(int)> setFn, int step, int minv, int maxv)
	{
		int val = currentValue;
		if (ImGui::InputInt(label.data(), &val, step))
		{
			val = glm::clamp(val, minv, maxv);
			if (val != currentValue)
			{
				setFn(val);
				return true;
			}
		}
		return false;
	}

	bool BasicValueInspector::InspectColour(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn)
	{
		glm::vec4 val = currentValue;
		if (ImGui::ColorEdit4(label.data(), glm::value_ptr(val)))
		{
			if (val != currentValue)
			{
				setFn(val);
				return true;
			}
		}
		return false;
	}
}