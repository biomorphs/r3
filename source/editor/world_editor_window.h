#pragma once
#include "editor_window.h"
#include "entities/entity_handle.h"
#include <string>
#include <memory>

namespace R3
{
	namespace Entities
	{
		class World;
	}
	class ValueInspector;
	class EntityListWidget;
	class EntityInspectorWidget;
	class EditorCommandList;
	class EditorCommand;
	class WorldEditorTool;
	class WorldEditorWindow : public EditorWindow
	{
	public:
		WorldEditorWindow(std::string worldIdentifier, std::string filePath="");
		virtual ~WorldEditorWindow();
		virtual std::string_view GetWindowTitle();
		virtual void Update();
		virtual CloseStatus PrepareToClose();
		virtual void OnFocusGained();

		void PushCommand(std::unique_ptr<EditorCommand>&&);

		bool SaveWorld(std::string_view path);
		Entities::World* GetWorld() const;

		const std::vector<Entities::EntityHandle>& GetSelectedEntities() { return m_selectedEntities; }
		void SelectEntities(const std::vector<Entities::EntityHandle>& h);
		void SelectEntity(const Entities::EntityHandle& h);
		void DeselectEntity(const Entities::EntityHandle& h);
		void DeselectAll();
		void SelectAll();
		void DeleteSelected();
	private:
		void UpdateSelectParentWindow();
		void DrawSelected();
		void UpdateMainContextMenu();
		void UpdateMainMenu();
		void DrawSideBarLeft(Entities::World* w);
		void DrawSideBarRight(Entities::World* w);
		void DrawToolbar();
		void CreateTools();
		void ActivateTool(int toolIndex);
		float m_sidebarLeftWidth = 200.0f;
		float m_sidebarRightWidth = 200.0f;
		bool m_showCommandsWindow = false;
		bool m_isInspectingEntityWithStaticMesh = false;
		std::string m_titleString;
		std::string m_filePath;	// set if the world was ever saved/loaded
		std::string m_worldIdentifier;	// used to index world in entity system
		uint32_t m_activeTool = -1;		// references m_tools
		std::vector<std::unique_ptr<WorldEditorTool>> m_tools;
		std::unique_ptr<EntityListWidget> m_allEntitiesWidget;
		std::unique_ptr<EntityInspectorWidget> m_inspectEntityWidget;
		std::unique_ptr<ValueInspector> m_valueInspector;
		std::unique_ptr<EditorCommandList> m_cmds;
		std::vector<Entities::EntityHandle> m_selectedEntities;
		bool m_isSelectParentActive = false;	// if active, a 'select parent entity' window is displayed
	};
}