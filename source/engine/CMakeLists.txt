set(EngineLib_SourceFiles
	loaded_model.h
	loaded_model.cpp
	thread_masks.h
	engine_startup.h
	engine_startup.cpp
	systems.h
	systems.cpp
	frame_graph.h
	frame_graph.cpp
	systems/time_system.h
	systems/time_system.cpp
	systems/event_system.h
	systems/event_system.cpp
	systems/input_system.h
	systems/input_system.cpp
	systems/imgui_system.h
	systems/imgui_system.cpp
)

add_library(Engine ${EngineLib_SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../ FILES ${EngineLib_SourceFiles})
target_include_directories(Engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(Engine PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Engine PRIVATE Core Optick::OptickCore SDL2::SDL2 glm::glm assimp::assimp imgui::imgui) 