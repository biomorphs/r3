#include "world_editor_add_component_cmd.h"
#include "editor/world_editor_window.h"
#include "entities/world.h"
#include <format>

namespace R3
{
	std::string_view WorldEditorAddComponentCommand::GetName()
	{
		static std::string nameStr = "Add Component to entity";
		const std::string targetStr = m_targetEntities.size() > 1 ? std::format("{} entities", m_targetEntities.size()) : "entity";
		nameStr = std::format("Add {} to {}", m_componentType, targetStr);
		return nameStr;
	}

	EditorCommand::Result WorldEditorAddComponentCommand::Execute()
	{
		auto world = m_window->GetWorld();
		m_addedToEntities.clear();
		for (int i = 0; i < m_targetEntities.size(); ++i)
		{
			if (!world->HasComponent(m_targetEntities[i], m_componentType))
			{
				if (world->AddComponent(m_targetEntities[i], m_componentType))
				{
					m_addedToEntities.emplace_back(m_targetEntities[i]);
				}
			}
		}
		return Result();
	}

	EditorCommand::Result WorldEditorAddComponentCommand::Undo()
	{
		auto world = m_window->GetWorld();
		for (int i = 0; i < m_addedToEntities.size(); ++i)
		{
			world->RemoveComponent(m_addedToEntities[i], m_componentType);
		}
		return Result();
	}

	EditorCommand::Result WorldEditorAddComponentCommand::Redo()
	{
		return Execute();
	}
}