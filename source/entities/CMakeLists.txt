set(EntitiesLib_SourceFiles
	systems/entity_system.h
	systems/entity_system.cpp
	component_type_registry.h
	component_type_registry.cpp
	component_storage.h
	component_storage.cpp
	component_helpers.h
	entity_handle.h
	entity_handle.cpp
	world.h
	world.cpp
	queries.h
	queries.inl
)

add_library(Entities ${EntitiesLib_SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../ FILES ${EntitiesLib_SourceFiles})
target_include_directories(Entities PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_link_libraries(Entities PRIVATE Core Optick::OptickCore glm::glm imgui::imgui nlohmann_json::nlohmann_json) 