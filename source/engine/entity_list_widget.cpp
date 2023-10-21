#include "entity_list_widget.h"
#include "entities/world.h"
#include "entities/component_type_registry.h"
#include "imgui.h"
#include <format>

namespace R3
{
	void EntityListWidget::DisplayEntityExtended(Entities::World& w, const Entities::EntityHandle& h)
	{
		const auto& allCmpTypes = Entities::ComponentTypeRegistry::GetInstance().AllTypes();
		for (int i = 0; i < allCmpTypes.size(); ++i)
		{
			if (w.HasComponent(h, allCmpTypes[i].m_name))
			{
				ImGui::Text(allCmpTypes[i].m_name.c_str());
			}
		}
	}

	bool EntityListWidget::DisplaySingleEntity(Entities::World& w, const Entities::EntityHandle& h)
	{
		std::string entityName = w.GetEntityDisplayName(h);
		if (m_options.m_showInternalIndex)
		{
			entityName = std::format("{} (#{})", entityName, h.GetPrivateIndex());
		}
		if (m_options.m_canExpandEntities)
		{
			if (ImGui::TreeNode(entityName.c_str()))
			{
				DisplayEntityExtended(w, h);
				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::Selectable(entityName.c_str());
		}
		
		return true;
	}

	void EntityListWidget::DisplayFlatList(Entities::World& w)
	{
		// use a clipper to limit the rows we need to display in a large list
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(w.GetActiveEntityCount()), ImGui::GetTextLineHeightWithSpacing());
		while (clipper.Step())
		{
			std::vector<Entities::EntityHandle> allEntities = w.GetActiveEntities(clipper.DisplayStart, clipper.DisplayEnd);
			for (int i = 0; i < allEntities.size(); ++i)
			{
				DisplaySingleEntity(w, allEntities[i]);
			}
		}
		clipper.End();
	}

	void EntityListWidget::DisplayOptionsBar()
	{
		if(ImGui::Button("Options"))
		{
			ImGui::OpenPopup("EntityListWidgetOptions");
		}
		if (ImGui::BeginPopup("EntityListWidgetOptions"))
		{
			ImGui::Checkbox("Expandable", &m_options.m_canExpandEntities);
			ImGui::SameLine();
			ImGui::Checkbox("Show Indices", &m_options.m_showInternalIndex);
			ImGui::EndPopup();
		}
	}

	void EntityListWidget::Update(Entities::World& w)
	{
		std::string txt = "Entities in '" + std::string(w.GetName()) + "'";
		if (ImGui::Begin(txt.c_str()))
		{
			DisplayOptionsBar();

			ImGuiWindowFlags window_flags = 0;
			ImGui::BeginChild("AllEntitiesList", ImGui::GetContentRegionAvail(), false, window_flags);
			if (m_options.m_layout == LayoutMode::FlatList)
			{
				DisplayFlatList(w);
			}
			ImGui::EndChild();
			ImGui::End();
		}
	}
}