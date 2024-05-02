#pragma once

#include <string_view>

namespace R3
{
	namespace Entities
	{
		class World;
	}
	class EditorCommandList;

	// A tool in the world editor
	class WorldEditorTool
	{
	public:
		virtual ~WorldEditorTool() {}
		virtual std::string_view GetToolbarButtonText() { return "???"; };
		virtual std::string_view GetTooltipText() { return ""; }
		virtual bool OnActivated(Entities::World&, EditorCommandList&) { return true; }		// return true if the tool can activate at this time
		virtual bool OnDeactivated(Entities::World&, EditorCommandList&) { return true; }	// return true if the tool can deactivate at this time
		virtual bool Update(Entities::World&, EditorCommandList&) { return true; }			// returns true if the tool should continue to be active
	};
}