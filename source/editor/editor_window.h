#pragma once 
#include <string_view>

namespace R3
{
	// The editor contains multiple editor windows, with one being active at once
	// For example, multiple worlds can be edited in different windows
	class EditorWindow
	{
	public:
		enum class CloseStatus {
			ReadyToClose,	// window can now be closed
			NotReady,		// don't close yet, ask again next frame
			Cancel			// cancel closing this window
		};
		virtual ~EditorWindow() {}
		virtual std::string_view GetWindowTitle() { return "Editor Window"; }
		virtual CloseStatus PrepareToClose() { return CloseStatus::ReadyToClose; }
		virtual void OnFocusGained() {};	// called once when the window is made active
		virtual void OnFocusLost() {};		// called once when the window is no longer active
		virtual void Update() {};			// called each frame if the window is active
	};
}