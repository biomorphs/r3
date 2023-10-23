#include "editor_command_list.h"
#include "editor_command.h"
#include "core/profiler.h"
#include "core/log.h"
#include "imgui.h"

namespace R3
{
	EditorCommandList::EditorCommandList()
	{
	}

	EditorCommandList::~EditorCommandList()
	{
	}

	bool EditorCommandList::HasWork()
	{
		return m_undoCommand != nullptr || m_redoCommand != nullptr || m_currentCommand != nullptr;
	}

	bool EditorCommandList::CanRedo()
	{
		return m_redoStack.size() > 0 && !HasWork();
	}

	void EditorCommandList::Redo()
	{
		if (CanRedo())
		{
			m_redoCommand = std::move(m_redoStack.at(m_redoStack.size() - 1));
			m_redoStack.pop_back();
		}
	}

	void EditorCommandList::Undo()
	{
		if (CanUndo())
		{
			m_undoCommand = std::move(m_undoStack.at(m_undoStack.size() - 1));
			m_undoStack.pop_back();
		}
	}

	bool EditorCommandList::CanUndo()
	{
		return m_undoStack.size() > 0 && !HasWork();
	}

	bool EditorCommandList::ShowWidget(bool embedAsChild)
	{
		bool shouldKeepOpen = true;
		bool isOpen = embedAsChild ? ImGui::BeginChild("CommandListWidget", { 0,0 }, true)
			: ImGui::Begin("Editor Commands", &shouldKeepOpen);
		if (isOpen)
		{
			if (ImGui::TreeNodeEx("Current Commands", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const auto& cmd : m_commandsToRun)
				{
					ImGui::Text(cmd->GetName().data());
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Undo Stack", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const auto& cmd : m_undoStack)
				{
					ImGui::Text(cmd->GetName().data());
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Redo Commands", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const auto& cmd : m_redoStack)
				{
					ImGui::Text(cmd->GetName().data());
				}
				ImGui::TreePop();
			}
			if (m_currentCommand != nullptr)
			{
				std::string txt = std::format("Currently running '{}'", m_currentCommand->GetName());
				ImGui::Text(txt.c_str());
			}
		}
		if (embedAsChild)
		{
			ImGui::EndChild();
		}
		else
		{
			ImGui::End();
		}
		return shouldKeepOpen;
	}

	void EditorCommandList::Push(std::unique_ptr<EditorCommand>&& cmdPtr)
	{
		m_commandsToRun.emplace_back(std::move(cmdPtr));
	}

	void EditorCommandList::RunNext()
	{
		R3_PROF_EVENT();
		if (m_undoCommand != nullptr)
		{
			EditorCommand::Result undoResult = m_undoCommand->Undo();
			if (undoResult == EditorCommand::Result::Succeeded)
			{
				m_redoStack.push_back(std::move(m_undoCommand));
			}
			else if (undoResult == EditorCommand::Result::Failed)
			{
				m_undoCommand = nullptr;
			}
		}
		else if (m_redoCommand != nullptr)
		{
			EditorCommand::Result redoResult = m_redoCommand->Redo();
			if (redoResult == EditorCommand::Result::Succeeded)
			{
				m_undoStack.push_back(std::move(m_redoCommand));
			}
			else if (redoResult == EditorCommand::Result::Failed)
			{
				m_redoCommand = nullptr;
			}
		}
		else
		{
			if (m_commandsToRun.size() > 0 && m_currentCommand == nullptr)
			{
				m_currentCommand = std::move(m_commandsToRun[0]);
				m_commandsToRun.pop_front();
				m_redoStack.clear();
			}

			if (m_currentCommand != nullptr)
			{
				EditorCommand::Result result = m_currentCommand->Execute();
				if (result == EditorCommand::Result::Succeeded)
				{
					if (m_currentCommand->CanUndoRedo())
					{
						m_undoStack.push_back(std::move(m_currentCommand));
					}
					else
					{
						m_undoStack.clear();
					}
					m_currentCommand = nullptr;
				}
				else if (result == EditorCommand::Result::Failed)
				{
					LogWarn("A command failed to execute!{}", m_currentCommand->GetName());
					m_currentCommand = nullptr;
				}
			}
		}
	}
}