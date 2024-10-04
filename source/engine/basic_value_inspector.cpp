#include "basic_value_inspector.h"
#include "engine/file_dialogs.h"
#include "engine/tag.h"
#include "entities/entity_handle.h"
#include "entities/systems/entity_system.h"
#include "engine/systems/texture_system.h"
#include "imgui.h"
#include <filesystem>

namespace R3
{
	float c_floatEpsilon = 0.00001f;		// smallest change to a float value we register as a modification

	bool BasicValueInspector::Inspect(std::string label, Tag currentValue, std::function<void(Tag)> setFn)
	{
		char textBuffer[1024 * 16] = { '\0' };
		strcpy_s(textBuffer, currentValue.GetString().c_str());
		if (ImGui::InputText(label.data(), textBuffer, sizeof(textBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			auto newTag = Tag(textBuffer);
			setFn(newTag);
			return true;
		}
		return false;
	}

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
			if (glm::abs(val - currentValue) > c_floatEpsilon)
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
			if (glm::abs(val - currentValue) > c_floatEpsilon)
			{
				setFn(val);
				return true;
			}
		}
		return false;
	}

	bool BasicValueInspector::Inspect(std::string_view label, glm::uvec2 currentValue, std::function<void(glm::uvec2)> setFn, glm::uvec2 minv, glm::uvec2 maxv)
	{
		glm::ivec2 newValue(currentValue);	// warning, uint -> int
		ImGui::InputInt2(label.data(), glm::value_ptr(newValue), ImGuiInputTextFlags_EnterReturnsTrue);
		glm::uvec2 outValue = glm::max(minv, glm::min(glm::uvec2(newValue), maxv));	// warning, int -> uint
		if (outValue != currentValue)
		{
			setFn(outValue);
			return true;
		}
		return false;
	}

	bool BasicValueInspector::Inspect(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn, glm::vec3 minv, glm::vec3 maxv)
	{
		constexpr float errorMargin = 0.000001f;
		float availableWidth = ImGui::GetContentRegionAvail().x;

		std::string inputId = std::string("##") + label.data() + "_value";
		glm::vec3 newValue = currentValue;
		ImGui::InputFloat3(label.data(), glm::value_ptr(newValue), "%f");
		newValue = glm::max(minv, glm::min(newValue, maxv));

		if (glm::distance(newValue, currentValue) >= c_floatEpsilon)
		{
			setFn(newValue);
			return true;
		}
		return false;
	}

	bool BasicValueInspector::Inspect(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn, glm::vec4 minv, glm::vec4 maxv)
	{
		float availableWidth = ImGui::GetContentRegionAvail().x;
		std::string inputId = std::string("##") + label.data() + "_value";
		glm::vec4 newValue = currentValue;
		ImGui::InputFloat4(label.data(), glm::value_ptr(newValue), "%f");
		newValue = glm::max(minv, glm::min(newValue, maxv));

		if (glm::distance(newValue, currentValue) >= c_floatEpsilon)
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

	bool BasicValueInspector::InspectEntity(std::string_view label, Entities::EntityHandle current, Entities::World* w, std::function<void(Entities::EntityHandle)> setFn)
	{
		std::string entityName(w->GetEntityDisplayName(current));
		std::string txt = std::format("{} - {}", label, entityName);
		bool newValSet = false;
		if (ImGui::Button(txt.c_str()))
		{
			m_entitySelectorOpen = true;
		}
		if (m_entitySelectorOpen)
		{
			std::string windowName = std::format("Select {} Entity", label);
			ImGui::OpenPopup(windowName.c_str());
			uint32_t popupFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
			if (ImGui::BeginPopupModal(windowName.data(), nullptr, popupFlags))
			{
				if (ImGui::Button("None"))
				{
					Entities::EntityHandle noEntity;
					setFn(noEntity);
					m_entitySelectorOpen = false;
					newValSet = true;
				}
				std::vector<Entities::EntityHandle> allEntities = w->GetActiveEntities();
				for (const auto& e : allEntities)
				{
					std::string txt = std::format("{}##{}", w->GetEntityDisplayName(e), e.GetID());
					if (ImGui::Button(txt.c_str()))
					{
						setFn(e);
						m_entitySelectorOpen = false;
						newValSet = true;
					}
				}
				ImGui::EndPopup();
			}
		}
		return newValSet;
	}

	bool BasicValueInspector::InspectTexture(std::string_view label, TextureHandle current, std::function<void(TextureHandle)> setFn, glm::ivec2 dims)
	{
		auto textures = Systems::GetSystem<TextureSystem>();
		auto texName = textures->GetTextureName(current);
		auto texImguiSet = textures->GetTextureImguiSet(current);
		bool changeTex = false;
		ImGui::SeparatorText(label.data());
		if (texImguiSet != nullptr)
		{
			ImVec2 size((float)dims.x, (float)dims.y);
			changeTex = ImGui::ImageButton(texImguiSet, size);
			ImGui::Text(texName.data());
		}
		else
		{
			if (current.m_index != -1)
			{
				changeTex = ImGui::Button(std::format("{}##{}", texName.data(), label).c_str());
			}
			else
			{
				changeTex = ImGui::Button(std::format("Select a texture##{}", label).c_str());
			}
		}

		if (changeTex)
		{
			const char* textureFilter = "";
			std::string newPath = FileLoadDialog(texName, textureFilter);
			if (newPath.length() > 0)
			{
				// sanitise path, only files relative to data root are allowed
				auto currentPath = std::filesystem::current_path();
				auto relativePath = std::filesystem::relative(newPath, currentPath);
				auto newHandle = textures->LoadTexture(relativePath.string());
				setFn(newHandle);
				return true;
			}
		}
		return false;
	}
}