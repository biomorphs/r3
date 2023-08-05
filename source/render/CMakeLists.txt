set(RenderLib_SourceFiles
	render_system.h
	render_system.cpp
	window.h
	window.cpp
)

add_library(Render ${RenderLib_SourceFiles})
target_include_directories(Render PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(Render PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Render PRIVATE Core fmt::fmt Optick::OptickCore SDL2::SDL2 glm::glm) 