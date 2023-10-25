#include "world_editor_delete_component_cmd.h"
#include "editor/world_editor_window.h"
#include "entities/world.h"
#include "engine/serialiser.h"
#include <format>

namespace R3
{
	std::string_view WorldEditorDeleteComponentCommand::GetName()
	{
		static std::string nameStr = "Delete component from entity";
		const std::string targetStr = m_targetEntities.size() > 1 ? std::format("{} entities", m_targetEntities.size()) : "entity";
		nameStr = std::format("Delete {} from {}", m_componentType, targetStr);
		return nameStr;
	}

	EditorCommand::Result WorldEditorDeleteComponentCommand::Execute()
	{
		auto world = m_window->GetWorld();
		m_deletedFromEntities.clear();
		m_serialisedComponents.clear();
		JsonSerialiser writeCmpData(JsonSerialiser::Write);
		for (int i = 0; i < m_targetEntities.size(); i++)
		{
			if (world->HasComponent(m_targetEntities[i], m_componentType))
			{
				writeCmpData.GetJson().clear();
				world->SerialiseComponent(m_targetEntities[i], m_componentType, writeCmpData);
				m_deletedFromEntities.push_back(m_targetEntities[i]);
				m_serialisedComponents.push_back(writeCmpData.GetJson().dump());	// store the component data for later
				world->RemoveComponent(m_targetEntities[i], m_componentType);
			}
		}
		return Result::Succeeded;
	}

	EditorCommand::Result WorldEditorDeleteComponentCommand::Undo()
	{
		auto world = m_window->GetWorld();
		JsonSerialiser cmpReadData(JsonSerialiser::Read);
		for (int d = 0; d < m_deletedFromEntities.size(); ++d)
		{
			if (world->AddComponent(m_deletedFromEntities[d], m_componentType))
			{
				cmpReadData.LoadFromString(m_serialisedComponents[d]);
				world->SerialiseComponent(m_deletedFromEntities[d], m_componentType, cmpReadData);	// restore the data
			}
		}
		return Result::Succeeded;
	}

	EditorCommand::Result WorldEditorDeleteComponentCommand::Redo()
	{
		return Execute();
	}
}