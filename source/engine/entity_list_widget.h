#pragma once
#include "entities/entity_handle.h"
#include <string>
#include <vector>
#include <functional>

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
		// Selection Callbacks
		using OnSelectedFn = std::function<void(const Entities::EntityHandle&, bool)>;	// handle, append to selection
		using OnDeselectedFn = std::function<void(const Entities::EntityHandle&)>;

		// How the entities are layed out (flat list, tree view, etc)
		enum class FilterType {
			ByName,ByComponent
		};
		struct Options {
			FilterType m_filter = FilterType::ByName;
			bool m_showInternalIndex = true;
			bool m_canExpandEntities = false;
			bool m_showOptionsButton = true;
			OnSelectedFn m_onSelected;
			OnDeselectedFn m_onDeselected;
		};
		void Update(Entities::World& w, const std::vector<Entities::EntityHandle>& selectedEntities, bool embedAsChild = false);
		Options m_options;
	private:
		bool IsFilterActive();
		void FilterEntities(Entities::World& w);
		void DisplayFilterContextMenu();
		void DisplayFilter();
		void DisplayOptionsBar();
		void DisplayFlatList(Entities::World& w, const std::vector<Entities::EntityHandle>& selectedEntities);
		bool DisplaySingleEntity(Entities::World& w, const Entities::EntityHandle& h, bool isSelected);
		void DisplayEntityExtended(Entities::World& w, const Entities::EntityHandle& h, bool isSelected);	// called if an entity is further expanded
		std::string m_filterText = "";
		uint64_t m_filterTypes = 0;	// mask of component types to filter
		std::vector<Entities::EntityHandle> m_filteredEntities;
	};
}