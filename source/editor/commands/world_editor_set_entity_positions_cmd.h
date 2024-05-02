#pragma once

#include "entities/entity_handle.h"
#include "core/glm_headers.h"
#include "editor/editor_command.h"
#include <vector>

namespace R3
{
	class WorldEditorWindow;
	class WorldEditorSetEntityPositionsCommand : public EditorCommand
	{
	public:
		WorldEditorSetEntityPositionsCommand(WorldEditorWindow* w);
		virtual std::string_view GetName() { return "Move Entities"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();
		void AddEntity(Entities::EntityHandle h, glm::vec3 oldPos, glm::vec3 newPos) { m_entities.push_back({ h, oldPos, newPos }); }

	private:
		struct EntityPos
		{
			Entities::EntityHandle m_entity;
			glm::vec3 m_oldPos;
			glm::vec3 m_newPos;
		};
		std::vector<EntityPos> m_entities;
		WorldEditorWindow* m_window;
	};
}