#include "world_editor_select_entity_tool.h"
#include "world_editor_window.h"
#include "editor_utils.h"
#include "editor_command_list.h"
#include "commands/world_editor_select_entities_cmd.h"
#include "engine/systems/static_mesh_system.h"
#include "engine/systems/input_system.h"
#include "entities/world.h"
#include "core/profiler.h"

namespace R3
{
	WorldEditorSelectEntityTool::WorldEditorSelectEntityTool(WorldEditorWindow* w)
		: m_window(w)
	{
	}

	bool WorldEditorSelectEntityTool::Update(Entities::World& w, EditorCommandList& cmds)
	{
		R3_PROF_EVENT();
		auto staticMeshes = Systems::GetSystem<StaticMeshSystem>();
		auto inputSystem = Systems::GetSystem<InputSystem>();
		static bool buttonWasDown = false;
		bool isButtonDown = inputSystem->GetMouseState().m_buttonState & LeftButton;
		Entities::EntityHandle hitEntity;
		if (isButtonDown || buttonWasDown)
		{
			glm::vec3 selectionRayStart, selectionRayEnd;
			MouseCursorToWorldspaceRay(100000.0f, selectionRayStart, selectionRayEnd);		// fire a ray out into the world
			hitEntity = staticMeshes->FindClosestActiveEntityIntersectingRay(selectionRayStart, selectionRayEnd);
		}
		if (hitEntity.GetID() != -1)
		{
			DrawEntityBounds(w, hitEntity, { 0,1,1,1 });
		}
		if (isButtonDown)
		{
			buttonWasDown = true;
		}
		else if (buttonWasDown)	// just released
		{
			buttonWasDown = false;
			if (hitEntity.GetID() != -1)
			{
				auto selectCmd = std::make_unique<WorldEditorSelectEntitiesCommand>(m_window);
				selectCmd->m_appendToSelection = inputSystem->GetKeyboardState().m_keyPressed[KEY_LCTRL];
				const auto& currentSelected = m_window->GetSelectedEntities();
				bool alreadySelected = std::find(currentSelected.begin(), currentSelected.end(), hitEntity) != currentSelected.end();
				if (alreadySelected)
				{
					selectCmd->m_toDeselect.push_back(hitEntity);
				}
				else
				{
					selectCmd->m_toSelect.push_back(hitEntity);
				}
				cmds.Push(std::move(selectCmd));
			}
		}
		return true;
	}
}