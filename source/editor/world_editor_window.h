#pragma once
#include "editor_window.h"
#include "entities/entity_handle.h"	// tempshit
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
	class WorldEditorWindow : public EditorWindow
	{
	public:
		WorldEditorWindow(std::string worldIdentifier);
		virtual ~WorldEditorWindow();
		virtual std::string_view GetWindowTitle();
		virtual void Update();
	private:
		void DrawSideBarLeft(Entities::World* w);
		void DrawSideBarRight(Entities::World* w);
		float m_sidebarLeftWidth = 200.0f;
		float m_sidebarRightWidth = 200.0f;
		std::string m_titleString;
		std::string m_filePath;	// set if the world was ever saved/loaded
		std::string m_worldIdentifier;	// used to index world in entity system
		std::unique_ptr<EntityListWidget> m_allEntitiesWidget;
		std::unique_ptr<EntityInspectorWidget> m_inspectEntityWidget;
		std::unique_ptr<ValueInspector> m_valueInspector;
		Entities::EntityHandle m_selectedEntity;	// tempshit
	};
}