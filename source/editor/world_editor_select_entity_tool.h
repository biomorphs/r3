#pragma once 

#include "world_editor_tool.h"

namespace R3
{
	class WorldEditorWindow;
	class WorldEditorSelectEntityTool : public WorldEditorTool
	{
	public:
		WorldEditorSelectEntityTool(WorldEditorWindow* w);
		virtual std::string_view GetToolbarButtonText() { return "S"; };
		virtual std::string_view GetTooltipText() { return "Select Entities"; }
		virtual bool Update(Entities::World&, EditorCommandList&);
	private:
		WorldEditorWindow* m_window = nullptr;
	};
}