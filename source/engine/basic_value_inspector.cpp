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

	bool BasicValueInspector::Inspect(std::string_view label, float currentValue, std::function<void(float)> setFn, float step, float minv, float maxv)
	{
		float val = currentValue;
		if (ImGui::InputFloat(label.data(), &val, step))
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

	bool BasicValueInspector::Inspect(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn, glm::vec3 minv, glm::vec3 maxv)
	{
		ImGui::Text(label.data());
		ImGui::SameLine(100.0f);	// label width?
		
		std::string inputId = std::string("##") + label.data() + "_value";
		glm::vec3 newValue = currentValue;
		ImGui::PushItemWidth(100.0f);
		ImGui::InputFloat((inputId + "_x").c_str(), &newValue.x);
		ImGui::SameLine();
		ImGui::InputFloat((inputId + "_y").c_str(), &newValue.y);
		ImGui::SameLine();
		ImGui::InputFloat((inputId + "_z").c_str(), &newValue.z);
		ImGui::PopItemWidth();

		if (newValue != currentValue)
		{
			setFn(newValue);
			return true;
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