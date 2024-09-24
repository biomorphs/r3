set(DungeonsOfArrrgh_SourceFiles
	world_grid_component.h
	world_grid_component.cpp
	world_grid_position.h
	world_grid_position.cpp
	dungeons_of_arrrgh.h
	dungeons_of_arrrgh.cpp
	vision_component.h
	vision_component.cpp
	monster_component.h
	monster_component.cpp
	inspectable_component.h
	inspectable_component.cpp
	blocks_tile_component.h
	blocks_tile_component.cpp
	base_actor_stats_component.h
	base_actor_stats_component.cpp
	inventory_component.h
	inventory_component.cpp
	item_component.h
	item_component.cpp
)

add_library(DungeonsOfArrrgh ${DungeonsOfArrrgh_SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../ FILES ${DungeonsOfArrrgh_SourceFiles})
target_include_directories(DungeonsOfArrrgh PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(DungeonsOfArrrgh PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(DungeonsOfArrrgh PRIVATE Core Engine Optick::OptickCore glm::glm)