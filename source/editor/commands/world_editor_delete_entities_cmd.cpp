#include "world_editor_delete_entities_cmd.h"
#include "editor/world_editor_window.h"
#include "engine/systems/imgui_system.h"
#include "entities/world.h"
#include "external/Fork-awesome/IconsForkAwesome.h"
#include "imgui.h"

namespace R3
{
	WorldEditorDeleteEntitiesCmd::WorldEditorDeleteEntitiesCmd(WorldEditorWindow* w)
		: m_window(w)
	{
	}

	EditorCommand::Result WorldEditorDeleteEntitiesCmd::Execute()
	{
		EditorCommand::Result result = Result::Waiting;
		auto imSys = Systems::GetSystem<ImGuiSystem>();
	    const char* popupTitle = (const char*)(ICON_FK_TRASH " Deleting entities can not be undone! " ICON_FK_TRASH);
		if (!m_openedPopup)
		{
			ImGui::OpenPopup(popupTitle);
			m_openedPopup = true;
		}
		if (ImGui::BeginPopupModal(popupTitle))
		{
			imSys->PushLargeFont();
			ImGui::Text("!!! Warning !!!");
			ImGui::PopFont();
			imSys->PushBoldFont();
			ImGui::Text("Deleting entities cannot be undone! Are you sure?");
			ImGui::PopFont();
			if (ImGui::Button("Yes, delete them!"))
			{
				m_window->DeleteSelected();
				ImGui::CloseCurrentPopup();
				result = Result::Succeeded;
			}
			ImGui::SameLine();
			if (ImGui::Button("No!"))
			{
				ImGui::CloseCurrentPopup();
				result = Result::Failed;
			}
			ImGui::EndPopup();
		}

		return result;
	}
}