#include "entity_inspector_widget.h"
#include "engine/value_inspector.h"
#include "engine/imgui_menubar_helper.h"
#include "engine/systems/imgui_system.h"	// for fonts
#include "core/profiler.h"
#include "entities/world.h"
#include "entities/entity_handle.h"
#include "entities/component_type_registry.h"
#include "imgui.h"

namespace R3
{
	void EntityInspectorWidget::UpdateEntityContextMenu(std::string_view name, const Entities::EntityHandle& h, Entities::World& w)
	{
		Entities::ComponentTypeRegistry& cti = Entities::ComponentTypeRegistry::GetInstance();
		MenuBar contextMenu;
		auto& addComponent = contextMenu.GetSubmenu("Add Component");
		for (int cmpTypeIndex = 0; cmpTypeIndex < cti.AllTypes().size(); ++cmpTypeIndex)
		{
			if (!w.HasComponent(h, cti.AllTypes()[cmpTypeIndex].m_name))
			{
				addComponent.AddItem(cti.AllTypes()[cmpTypeIndex].m_name, [&h, &w, &cti, cmpTypeIndex]() {
					w.AddComponent(h, cti.AllTypes()[cmpTypeIndex].m_name);
				});
			}
		}
		contextMenu.DisplayContextMenu(false, name.data());
	}

	bool EntityInspectorWidget::ShowEntityHeader(std::string_view name, const Entities::EntityHandle& h, Entities::World& w)
	{
		Systems::GetSystem<ImGuiSystem>()->PushLargeFont();
		ImGui::PushID(name.data());	// Imgui::Text needs a ID for context menu to work
		ImGui::Text(name.data());
		ImGui::PopID();
		ImGui::PopFont();
		UpdateEntityContextMenu(name, h, w);
		return true;
	}

	void EntityInspectorWidget::Update(const Entities::EntityHandle& h, Entities::World& w, ValueInspector& v, bool embedAsChild)
	{
		R3_PROF_EVENT();
		std::string entityName = ">Invalid handle!<";
		if (w.IsHandleValid(h))
		{
			entityName = w.GetEntityDisplayName(h);
		}

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
		bool isOpen = embedAsChild ? ImGui::BeginChild(entityName.c_str(), {0,actualChildSize }, true) : ImGui::Begin(entityName.c_str());
		if (isOpen && w.IsHandleValid(h))
		{
			if (ShowEntityHeader(entityName, h, w))
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
		if (ImGui::CollapsingHeader(cmpTypeData.m_name.c_str()))
		{
			const auto& inspectFn = cti.AllTypes()[cmpTypeIndex].m_inspectFn;
			if (inspectFn)
			{
				inspectFn(h, w, v);
			}
		}
	}
}