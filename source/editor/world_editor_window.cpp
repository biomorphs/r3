#include "world_editor_window.h"
#include "world_info_widget.h"
#include "engine/systems.h"
#include "engine/entity_list_widget.h"
#include "entities/systems/entity_system.h"
#include "render/render_system.h"
#include "imgui.h"
#include <format>

namespace R3
{
	WorldEditorWindow::WorldEditorWindow(std::string worldIdentifier)
		: m_worldIdentifier(worldIdentifier)
	{
		m_allEntitiesWidget = std::make_unique<EntityListWidget>();
		m_allEntitiesWidget->m_options.m_canExpandEntities = false;
		m_allEntitiesWidget->m_options.m_showInternalIndex = false;
	}

	WorldEditorWindow::~WorldEditorWindow()
	{
	}

	void WorldEditorWindow::DrawSideBarLeft(Entities::World* w)
	{
		uint32_t sidebarFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove;
		auto windowFullExtents = Systems::GetSystem<RenderSystem>()->GetWindowExtents();
		ImGui::SetNextWindowPos({ 0,ImGui::GetTextLineHeightWithSpacing() * 3 });
		ImGui::SetNextWindowSize(ImVec2(m_sidebarLeftWidth, windowFullExtents.y - ImGui::GetTextLineHeightWithSpacing() * 3));
		if (ImGui::Begin("EntitiesSidebarLeft", nullptr, sidebarFlags))
		{
			if (w)
			{
				WorldInfoWidget wi;
				wi.Update(*w, true);
				m_allEntitiesWidget->Update(*w, true);
			}
		}
		float newWidth = ImGui::GetWindowWidth();
		ImGui::End();
		m_sidebarLeftWidth = glm::max(newWidth, windowFullExtents.x * 0.05f);
		m_sidebarLeftWidth = glm::min(m_sidebarLeftWidth, windowFullExtents.x * 0.45f);
	}

	std::string_view WorldEditorWindow::GetWindowTitle()
	{
		if (m_filePath.empty())
		{
			m_titleString = std::format("World '{}'", m_worldIdentifier);
		}
		else
		{
			m_titleString = std::format("World '{}' ({})", m_worldIdentifier, m_filePath.c_str());
		}
		
		return m_titleString;
	}

	void WorldEditorWindow::Update()
	{
		auto* entities = Systems::GetSystem<Entities::EntitySystem>();
		if (entities)
		{
			Entities::World* thisWorld = entities->GetWorld(m_worldIdentifier);
			DrawSideBarLeft(thisWorld);
		}
	}
}