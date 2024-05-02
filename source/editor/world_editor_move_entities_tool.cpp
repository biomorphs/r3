#include "world_editor_move_entities_tool.h"
#include "world_editor_window.h"
#include "world_editor_transform_widget.h"
#include "entities/world.h"
#include "core/profiler.h"

namespace R3
{
	WorldEditorMoveEntitiesTool::WorldEditorMoveEntitiesTool(WorldEditorWindow* w)
		: m_window(w)
	{
		m_transformWidget = std::make_unique<WorldEditorTransformWidget>(w);
	}

	bool WorldEditorMoveEntitiesTool::Update(Entities::World& w, EditorCommandList& cmds)
	{
		R3_PROF_EVENT();
		m_transformWidget->Update(m_window->GetSelectedEntities());
		return true;
	}
}