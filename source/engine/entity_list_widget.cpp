#include "entity_list_widget.h"
#include "entities/world.h"
#include "entities/component_type_registry.h"
#include "core/profiler.h"
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
		char entityName[256] = "";
		char displayName[512] = "";
		w.GetEntityDisplayName(h, entityName, sizeof(entityName));
		if (m_options.m_showInternalIndex)
		{
			sprintf_s(displayName, "%s (#%d)", entityName, h.GetPrivateIndex());
		}
		else
		{
			sprintf_s(displayName, "%s", entityName);
		}
		if (m_options.m_canExpandEntities)
		{
			if (ImGui::TreeNode(displayName))
			{
				DisplayEntityExtended(w, h);
				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::Selectable(displayName);
		}
		
		return true;
	}

	bool EntityListWidget::IsFilterActive()
	{
		if (m_options.m_filter == FilterType::ByName && m_filterText.size() > 0)
		{
			return true;
		}
		return false;
	}

	void EntityListWidget::DisplayFlatList(Entities::World& w)
	{
		R3_PROF_EVENT();

		// use a clipper to limit the rows we need to display in a large list
		ImGuiListClipper clipper;
		if (IsFilterActive())
		{
			clipper.Begin(static_cast<int>(m_filteredEntities.size()), ImGui::GetTextLineHeightWithSpacing());
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
				{
					DisplaySingleEntity(w, m_filteredEntities[i]);
				}
			}
			clipper.End();
		}
		else
		{
			clipper.Begin(static_cast<int>(w.GetActiveEntityCount()), ImGui::GetTextLineHeightWithSpacing());
			while (clipper.Step())
			{
				std::vector<Entities::EntityHandle> all = w.GetActiveEntities(clipper.DisplayStart, clipper.DisplayEnd);
				for (int i = 0; i < all.size(); ++i)
				{
					DisplaySingleEntity(w, all[i]);
				}
			}
			clipper.End();
		}
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

	void EntityListWidget::DisplayFilter()
	{
		if (m_options.m_filter == FilterType::ByName && m_filterText.size() < 256)
		{
			char filterBuffer[256] = { '\0' };
			strcpy_s(filterBuffer, m_filterText.c_str());
			if (ImGui::InputTextWithHint("##filter", "Filter by name", filterBuffer, sizeof(filterBuffer)))
			{
				m_filterText = filterBuffer;
			}
		}
	}

	void EntityListWidget::FilterEntities(Entities::World& w)
	{
		R3_PROF_EVENT();
		m_filteredEntities.clear();
		if (m_options.m_filter == FilterType::ByName && !m_filterText.empty())
		{
			char entityName[256] = "";
			auto filterEntity = [&](const Entities::EntityHandle& e) {
				w.GetEntityDisplayName(e, entityName, sizeof(entityName));
				if (strstr(entityName, m_filterText.c_str()) != nullptr)
				{
					m_filteredEntities.emplace_back(e);
				}
				return true;
			};
			w.ForEachActiveEntity(filterEntity);
		}
	}

	void EntityListWidget::Update(Entities::World& w)
	{
		R3_PROF_EVENT();
		FilterEntities(w);
		std::string txt = "Entities in '" + std::string(w.GetName()) + "'";
		if (ImGui::Begin(txt.c_str()))
		{
			DisplayOptionsBar();
			ImGui::SameLine();
			DisplayFilter();

			ImGuiWindowFlags window_flags = 0;
			ImGui::BeginChild("AllEntitiesList", ImGui::GetContentRegionAvail(), true, window_flags);
			if (m_options.m_layout == LayoutMode::FlatList)
			{
				DisplayFlatList(w);
			}
			ImGui::EndChild();
			ImGui::End();
		}
	}
}