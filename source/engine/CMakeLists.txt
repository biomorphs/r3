set(EngineLib_SourceFiles
	async.h
	async.cpp
	callback_array.h
	model_data.h
	model_data.cpp
	model_data_handle.h
	model_data_handle.cpp
	engine_startup.h
	engine_startup.cpp
	systems.h
	systems.cpp
	frame_graph.h
	frame_graph.cpp
	serialiser.h
	serialiser.cpp
	entity_list_widget.h
	entity_list_widget.cpp
	imgui_menubar_helper.h
	imgui_menubar_helper.cpp
	value_inspector.h
	entity_inspector_widget.h
	entity_inspector_widget.cpp
	basic_value_inspector.h
	basic_value_inspector.cpp
	file_dialogs.h
	file_dialogs.cpp
	frustum.h
	frustum.cpp
	flycam.h
	flycam.cpp
	register_engine_components.h
	register_engine_components.cpp
	systems/time_system.h
	systems/time_system.cpp
	systems/event_system.h
	systems/event_system.cpp
	systems/input_system.h
	systems/input_system.cpp
	systems/imgui_system.h
	systems/imgui_system.cpp
	systems/camera_system.h
	systems/camera_system.cpp
	systems/job_system.h
	systems/job_system.cpp
	systems/model_data_system.h
	systems/model_data_system.cpp
	systems/static_mesh_system.h
	systems/static_mesh_system.cpp
	components/environment_settings.h
	components/environment_settings.cpp
	components/transform.h
	components/transform.cpp
	components/camera.h
	components/camera.cpp
	components/static_mesh.h
	components/static_mesh.cpp
)

add_library(Engine ${EngineLib_SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../ FILES ${EngineLib_SourceFiles})
target_include_directories(Engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(Engine PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Engine PRIVATE Core Optick::OptickCore SDL2::SDL2 glm::glm assimp::assimp imgui::imgui unofficial::nativefiledialog::nfd) 