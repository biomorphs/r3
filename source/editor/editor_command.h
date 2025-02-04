#pragma once
#include <string_view>
#include <string>

namespace R3
{
	// classic command pattern for undo/redo
	// EditorCommandList handles execution
	class EditorCommand
	{
	public:
		virtual ~EditorCommand() {}
		enum class Result
		{
			Succeeded,	// the command finished executing and succeeded
			Failed,		// the command finished executing and failed 
			Waiting,	// the command needs to wait for something, try again later
		};
		virtual std::string_view GetName() = 0;	// for debugging
		virtual bool CanUndoRedo() = 0;	// return true if undo/redo supported
		virtual Result Execute() = 0;
		virtual Result Undo() { return Result::Failed; }
		virtual Result Redo() { return Result::Failed; }
	};
}