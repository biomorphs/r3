#pragma once
#include "entities/entity_handle.h"
#include <string>
#include <vector>

namespace R3
{
	namespace Entities
	{
		class World;
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
		enum class FilterType {
			ByName,
		};
		struct Options {
			LayoutMode m_layout = LayoutMode::FlatList;
			FilterType m_filter = FilterType::ByName;
			bool m_showInternalIndex = true;
			bool m_canExpandEntities = true;
		};
		void Update(Entities::World& w);
		Options m_options;
	private:
		bool IsFilterActive();
		void FilterEntities(Entities::World& w);
		void DisplayFilter();
		void DisplayOptionsBar();
		void DisplayFlatList(Entities::World& w);
		bool DisplaySingleEntity(Entities::World& w, const Entities::EntityHandle& h);
		void DisplayEntityExtended(Entities::World& w, const Entities::EntityHandle& h);	// called if an entity is further expanded
		std::string m_filterText = "";
		std::vector<Entities::EntityHandle> m_filteredEntities;
	};
}