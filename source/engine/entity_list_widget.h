#pragma once

namespace R3
{
	namespace Entities
	{
		class World;
		class EntityHandle;
	}

	// Displays the list of entities in a world
	// Supports (multiple) selection, filtering and different viewing modes
	class EntityListWidget
	{
	public:
		// How the entities are layed out (flat list, tree view, etc)
		enum class LayoutMode {
			FlatList,
		};
		struct Options {
			LayoutMode m_layout = LayoutMode::FlatList;
			bool m_showInternalIndex = true;
			bool m_canExpandEntities = true;
		};
		void Update(Entities::World& w);
		Options m_options;
	private:
		void DisplayOptionsBar();
		void DisplayFlatList(Entities::World& w);
		bool DisplaySingleEntity(Entities::World& w, const Entities::EntityHandle& h);
		void DisplayEntityExtended(Entities::World& w, const Entities::EntityHandle& h);	// called if an entity is further expanded
	};
}