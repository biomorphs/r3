#pragma once
#include "editor_window.h"
#include "entities/entity_handle.h"
#include <string>
#include <vector>
#include <memory>

namespace R3
{
	namespace Entities
	{
		class World;
	}
	class EntityListWidget;
	class EntityInspectorWidget;
	class WorldRunnerWindow : public EditorWindow
	{
	public:
		WorldRunnerWindow(std::string worldIdentifier, std::string filePath = "");
		virtual ~WorldRunnerWindow();
		virtual std::string_view GetWindowTitle();
		virtual void Update();
		virtual CloseStatus PrepareToClose();
		virtual void OnFocusGained();
		virtual void OnFocusLost();

	private:
		void UpdateMainMenu();
		Entities::World* GetWorld();
		std::string m_sceneFile = "";
		std::string m_worldID = "";
		// debugging
		std::unique_ptr<EntityListWidget> m_allEntitiesWidget;
		std::unique_ptr<EntityInspectorWidget> m_inspectEntityWidget;
		std::vector<Entities::EntityHandle> m_selectedEntities;
		bool m_showInspector = false;
	};
}