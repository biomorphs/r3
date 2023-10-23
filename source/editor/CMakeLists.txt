set(EditorLib_SourceFiles
	world_info_widget.h
	world_info_widget.cpp
	editor_window.h
	world_editor_window.h
	world_editor_window.cpp
	editor_command.h
	editor_command_list.h
	editor_command_list.cpp
	systems/editor_system.h
	systems/editor_system.cpp
	commands/world_editor_save_cmd.h
	commands/world_editor_save_cmd.cpp
)

add_library(Editor ${EditorLib_SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../ FILES ${EditorLib_SourceFiles})	# adds directory structure filters in VS
target_include_directories(Editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_link_libraries(Editor PRIVATE imgui::imgui Optick::OptickCore) 