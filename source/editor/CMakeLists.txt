set(EditorLib_SourceFiles
	world_info_widget.h
	world_info_widget.cpp
	editor_window.h
	world_editor_window.h
	world_editor_window.cpp
	editor_command.h
	editor_command_list.h
	editor_command_list.cpp
	undo_redo_value_inspector.h
	undo_redo_value_inspector.cpp
	editor_utils.h
	editor_utils.cpp
	world_editor_tool.h
	world_editor_select_entity_tool.h
	world_editor_select_entity_tool.cpp
	world_editor_move_entities_tool.h
	world_editor_move_entities_tool.cpp
	world_editor_transform_widget.h
	world_editor_transform_widget.cpp
	systems/editor_system.h
	systems/editor_system.cpp
	commands/world_editor_set_entity_positions_cmd.h
	commands/world_editor_set_entity_positions_cmd.cpp
	commands/world_editor_save_cmd.h
	commands/world_editor_save_cmd.cpp
	commands/world_editor_select_entities_cmd.h
	commands/world_editor_select_entities_cmd.cpp
	commands/world_editor_add_empty_entity_cmd.h
	commands/world_editor_add_empty_entity_cmd.cpp
	commands/world_editor_delete_entities_cmd.h
	commands/world_editor_delete_entities_cmd.cpp
	commands/world_editor_add_component_cmd.h
	commands/world_editor_add_component_cmd.cpp
	commands/world_editor_delete_component_cmd.h
	commands/world_editor_delete_component_cmd.cpp
	commands/world_editor_clone_entities_cmd.h
	commands/world_editor_clone_entities_cmd.cpp
	commands/world_editor_add_entity_from_mesh_cmd.h
	commands/world_editor_add_entity_from_mesh_cmd.cpp
	commands/world_editor_set_entity_parent_cmd.h
	commands/world_editor_set_entity_parent_cmd.cpp
	commands/set_value_cmd.h
)

add_library(Editor ${EditorLib_SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../ FILES ${EditorLib_SourceFiles})	# adds directory structure filters in VS
target_include_directories(Editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_link_libraries(Editor PRIVATE imgui::imgui Optick::OptickCore) 