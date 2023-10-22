#pragma once
#include "editor_window.h"
#include <string>
#include <memory>

namespace R3
{
	namespace Entities
	{
		class World;
	}
	class EntityListWidget;
	class WorldEditorWindow : public EditorWindow
	{
	public:
		WorldEditorWindow(std::string worldIdentifier);
		virtual ~WorldEditorWindow();
		virtual std::string_view GetWindowTitle();
		virtual void Update();
	private:
		void DrawSideBarLeft(Entities::World* w);
		float m_sidebarLeftWidth = 200.0f;
		std::string m_titleString;
		std::string m_filePath;	// set if the world was ever saved/loaded
		std::string m_worldIdentifier;	// used to index world in entity system
		std::unique_ptr<EntityListWidget> m_allEntitiesWidget;
	};
}