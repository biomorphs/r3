#pragma once

#include <string_view>

namespace R3
{
	// A tool in the world editor
	class WorldEditorTool
	{
	public:
		virtual ~WorldEditorTool() {}
		virtual std::string_view GetToolbarButtonText() { return "???"; };
		virtual std::string_view GetTooltipText() { return ""; }
		virtual bool OnActivated() { return true; }		// return true if the tool can activate at this time
		virtual bool OnDeactivated() { return true; }	// return true if the tool can deactivate at this time
		virtual bool Update() { return true; }			// returns true if the tool should continue to be active
	};
}