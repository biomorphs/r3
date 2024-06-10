#include "world_editor_import_scene_cmd.h"
#include "editor/world_editor_window.h"
#include "core/profiler.h"
#include "entities/world.h"

namespace R3
{
	WorldEditorImportSceneCmd::WorldEditorImportSceneCmd(WorldEditorWindow* w, std::string path)
		: m_window(w)
		, m_pathToLoad(path)
	{
	}

	EditorCommand::Result WorldEditorImportSceneCmd::Execute()
	{
		R3_PROF_EVENT();
		auto world = m_window->GetWorld();
		m_selectedEntities = m_window->GetSelectedEntities();
		m_newEntities = world->Import(m_pathToLoad);
		m_window->DeselectAll();

		auto newRoot = world->AddEntity();	// parent all new children to this
		world->SetEntityName(newRoot, std::filesystem::path(m_pathToLoad).filename().string());
		for (const auto it : m_newEntities)
		{
			m_window->SelectEntity(it);
			if (world->GetParent(it).GetID() == -1)
			{
				world->SetParent(it, newRoot);
			}
		}
		return Result::Succeeded;
	}

	EditorCommand::Result WorldEditorImportSceneCmd::Undo()
	{
		R3_PROF_EVENT();
		auto world = m_window->GetWorld();
		m_window->DeselectAll();
		for (const auto it : m_newEntities)	// delete the imports
		{
			world->RemoveEntity(it);
		}
		for (const auto it : m_selectedEntities)	// reselect the originals
		{
			m_window->SelectEntity(it);
		}
		return Result::Succeeded;
	}

	EditorCommand::Result WorldEditorImportSceneCmd::Redo()
	{
		return Execute();
	}
}