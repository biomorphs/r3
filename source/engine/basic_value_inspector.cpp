#include "basic_value_inspector.h"
#include "engine/file_dialogs.h"
#include "entities/entity_handle.h"
#include "entities/systems/entity_system.h"
#include "engine/systems/texture_system.h"
#include "imgui.h"
#include <filesystem>

namespace R3
{
	bool BasicValueInspector::Inspect(std::string label, bool currentValue, std::function<void(bool)> setFn)
	{
		bool newVal = currentValue;
		if (ImGui::Checkbox(label.data(), &newVal))
		{
			setFn(newVal);
			return true;
		}
		return false;
	}

	bool BasicValueInspector::Inspect(std::string_view label, std::string_view currentValue, std::function<void(std::string)> setFn)
	{
		char textBuffer[1024 * 16] = { '\0' };
		strcpy_s(textBuffer, currentValue.data());
		if (ImGui::InputText(label.data(), textBuffer, sizeof(textBuffer)))
		{
			setFn(textBuffer);
			return true;
		}
		return false;
	}

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

	bool BasicValueInspector::Inspect(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn, glm::vec4 minv, glm::vec4 maxv)
	{
		ImGui::Text(label.data());
		ImGui::SameLine(100.0f);	// label width?

		std::string inputId = std::string("##") + label.data() + "_value";
		glm::vec4 newValue = currentValue;
		ImGui::PushItemWidth(100.0f);
		ImGui::InputFloat((inputId + "_x").c_str(), &newValue.x);
		ImGui::SameLine();
		ImGui::InputFloat((inputId + "_y").c_str(), &newValue.y);
		ImGui::SameLine();
		ImGui::InputFloat((inputId + "_z").c_str(), &newValue.z);
		ImGui::PopItemWidth();
		ImGui::InputFloat((inputId + "_w").c_str(), &newValue.w);
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

	bool BasicValueInspector::InspectColour(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn)
	{
		glm::vec3 val = currentValue;
		if (ImGui::ColorEdit3(label.data(), glm::value_ptr(val)))
		{
			if (val != currentValue)
			{
				setFn(val);
				return true;
			}
		}
		return false;
	}

	bool BasicValueInspector::InspectFile(std::string_view label, std::string_view path, std::string_view filter, std::function<void(std::string_view)> setFn)
	{
		std::string txt = std::format("{} - {}", label, path);
		if (ImGui::Button(txt.c_str()))
		{
			std::string newPath = FileLoadDialog(path, filter);
			if (newPath.length() > 0)
			{
				// sanitise path, only files relative to data root are allowed
				auto currentPath = std::filesystem::current_path();
				auto relativePath = std::filesystem::relative(newPath, currentPath);
				setFn(relativePath.string());
				return true;
			}
		}
		return false;
	}

	bool BasicValueInspector::InspectEntity(std::string_view label, Entities::EntityHandle current, std::function<void(Entities::EntityHandle)> setFn)
	{
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		std::string entityName(entities->GetName());
		std::string txt = std::format("{} - {}", label, entityName);
		if (ImGui::Button(txt.c_str()))
		{

		}
		return false;
	}

	bool BasicValueInspector::InspectTexture(std::string_view label, TextureHandle current, std::function<void(TextureHandle)> setFn, glm::ivec2 dims)
	{
		assert("Todo");
		return false;
	}
}