#pragma once 

#include "world_editor_tool.h"

namespace R3
{
	class WorldEditorWindow;
	class WorldEditorSelectEntityTool : public WorldEditorTool
	{
	public:
		WorldEditorSelectEntityTool(WorldEditorWindow* w);
	private:
		WorldEditorWindow* m_window = nullptr;
	};
}