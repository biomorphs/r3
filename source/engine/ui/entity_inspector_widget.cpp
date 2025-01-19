#include "entity_inspector_widget.h"
#include "engine/ui/value_inspector.h"
#include "engine/ui/imgui_menubar_helper.h"
#include "engine/systems/imgui_system.h"	// for fonts
#include "core/profiler.h"
#include "entities/world.h"
#include "entities/entity_handle.h"
#include "entities/component_type_registry.h"
#include <imgui.h>
#include <format>

namespace R3
{
	void EntityInspectorWidget::UpdateEntityContextMenu(const Entities::EntityHandle& h, Entities::World& w)
	{
		Entities::ComponentTypeRegistry& cti = Entities::ComponentTypeRegistry::GetInstance();
		MenuBar contextMenu;
		auto& addComponent = contextMenu.GetSubmenu("Add Component");
		for (int cmpTypeIndex = 0; cmpTypeIndex < cti.AllTypes().size(); ++cmpTypeIndex)
		{
			if (!w.HasComponent(h, cti.AllTypes()[cmpTypeIndex].m_name))
			{
				addComponent.AddItem(cti.AllTypes()[cmpTypeIndex].m_name, [&h, &w, &cti, cmpTypeIndex, this]() {
					m_onAddComponent(h, cti.AllTypes()[cmpTypeIndex].m_name);
				});
			}
		}
		contextMenu.DisplayContextMenu(false);
	}

	bool EntityInspectorWidget::ShowEntityHeader(const Entities::EntityHandle& h, Entities::World& w)
	{
		std::string oldName(w.GetEntityName(h));
		char textBuffer[1024] = { '\0' };
		strcpy_s(textBuffer, oldName.c_str());
		if (ImGui::InputText(std::format("Name##EntName{}", h.GetID()).c_str(), textBuffer, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			m_onSetEntityName(h, oldName, textBuffer);
		}
		if (m_onAddComponent)
		{
			UpdateEntityContextMenu(h, w);
		}
		return true;
	}

	void EntityInspectorWidget::Update(const Entities::EntityHandle& h, Entities::World& w, ValueInspector& v, bool embedAsChild)
	{
		R3_PROF_EVENT();
		std::string displayName = w.GetEntityDisplayName(h);
		// A limitation of imgui is fixed sized child windows. 
		// This implements a collapsable window by storing the size of the contents from the previous frame
		float actualChildSize = 0.0f;
		auto foundSize = m_entityIdToWindowHeight.find(h.GetID());	// use the previously stored size if we have one
		if (foundSize == m_entityIdToWindowHeight.end())
		{
			const float minSize = ImGui::GetTextLineHeightWithSpacing() * 1.75f;
			actualChildSize = glm::max(minSize, w.GetOwnedComponentCount(h) * ImGui::GetTextLineHeightWithSpacing() * 8.0f);
		}
		else
		{
			actualChildSize = foundSize->second;
		}
		bool isOpen = embedAsChild ? ImGui::BeginChild(displayName.c_str(), {0,actualChildSize }, true) : ImGui::Begin(displayName.c_str());
		if (isOpen && w.IsHandleValid(h))
		{
			if (ShowEntityHeader(h, w))
			{
				Entities::ComponentTypeRegistry& cti = Entities::ComponentTypeRegistry::GetInstance();
				for (int cmpTypeIndex = 0; cmpTypeIndex < cti.AllTypes().size(); ++cmpTypeIndex)
				{
					if (w.HasComponent(h, cti.AllTypes()[cmpTypeIndex].m_name))
					{
						DisplayComponent(h, w, v, cmpTypeIndex);
					}
				}
			}
		}
		if (embedAsChild)
		{
			// store the max y for the next frame (child will resize to contents of previous frame)
			m_entityIdToWindowHeight[h.GetID()] = ImGui::GetCursorPos().y + 4.0f;	
			ImGui::EndChild();
		}
		else
		{
			ImGui::End();
		}
	}

	void EntityInspectorWidget::DisplayComponent(const Entities::EntityHandle& h, Entities::World& w, ValueInspector& v, int cmpTypeIndex)
	{
		Entities::ComponentTypeRegistry& cti = Entities::ComponentTypeRegistry::GetInstance();
		auto& cmpTypeData = cti.AllTypes()[cmpTypeIndex];
		bool headerOpen = ImGui::CollapsingHeader(cmpTypeData.m_name.c_str());
		if (m_onRemoveComponent)
		{
			MenuBar contextMenu;
			std::string txt = "Remove " + cmpTypeData.m_name;
			contextMenu.AddItem(txt, [&, this]() {
				m_onRemoveComponent(h, cmpTypeData.m_name);
			});
			contextMenu.DisplayContextMenu(false, cmpTypeData.m_name.c_str());
		}
		if (headerOpen)
		{
			const auto& inspectFn = cti.AllTypes()[cmpTypeIndex].m_inspectFn;
			if (inspectFn)
			{
				inspectFn(h, w, v);
			}
		}
	}
}