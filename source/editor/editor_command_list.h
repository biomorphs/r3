#pragma once 
#include <deque>
#include <memory>

namespace R3
{
	// Handles execution and undo/redo stack of commands for an editor
	class EditorCommand;
	class EditorCommandList
	{
	public:
		EditorCommandList();
		~EditorCommandList();

		bool ShowWidget(bool embedAsChild=false);

		void Push(std::unique_ptr<EditorCommand>&& cmdPtr);
		bool HasWork();
		void RunNext();

		bool CanRedo();
		void Redo();
		bool CanUndo();
		void Undo();

	private:
		std::unique_ptr<EditorCommand> m_undoCommand;
		std::unique_ptr<EditorCommand> m_currentCommand;
		std::unique_ptr<EditorCommand> m_redoCommand;
		std::deque<std::unique_ptr<EditorCommand>> m_redoStack;
		std::deque<std::unique_ptr<EditorCommand>> m_commandsToRun;
		std::deque<std::unique_ptr<EditorCommand>> m_undoStack;
	};
}