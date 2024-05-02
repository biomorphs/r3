#pragma once 

#include "world_editor_tool.h"
#include <memory>

namespace R3
{
	class WorldEditorWindow;
	class WorldEditorTransformWidget;
	class WorldEditorMoveEntitiesTool : public WorldEditorTool
	{
	public:
		WorldEditorMoveEntitiesTool(WorldEditorWindow* w);
		virtual std::string_view GetToolbarButtonText() { return "M"; };
		virtual std::string_view GetTooltipText() { return "Move Entities"; }
		virtual bool Update(Entities::World&, EditorCommandList&);
	private:
		WorldEditorWindow* m_window = nullptr;
		std::unique_ptr<WorldEditorTransformWidget> m_transformWidget;
	};
}