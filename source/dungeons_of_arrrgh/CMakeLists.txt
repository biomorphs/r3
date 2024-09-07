set(DungeonsOfArrrgh_SourceFiles
	world_grid_component.h
	world_grid_component.cpp
	dungeons_of_arrrgh.h
	dungeons_of_arrrgh.cpp
	vision_component.h
	vision_component.cpp
)

add_library(DungeonsOfArrrgh ${DungeonsOfArrrgh_SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../ FILES ${DungeonsOfArrrgh_SourceFiles})
target_include_directories(DungeonsOfArrrgh PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(DungeonsOfArrrgh PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(DungeonsOfArrrgh PRIVATE Core Engine Optick::OptickCore glm::glm)