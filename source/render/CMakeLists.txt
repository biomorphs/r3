set(RenderLib_SourceFiles
	render_system.h
	render_system.cpp
	window.h
	window.cpp
	vulkan_memory_allocator_impl.cpp
	vulkan_helpers.h
	vulkan_helpers.cpp
	pipeline_builder.h
	pipeline_builder.cpp
	device.h
	device.cpp
	swap_chain.h
	swap_chain.cpp
	immediate_renderer.h
	immediate_renderer.cpp
	camera.h
	camera.inl
)

add_library(Render ${RenderLib_SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../ FILES ${RenderLib_SourceFiles})	# adds directory structure filters in VS
target_include_directories(Render PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(Render PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Render PRIVATE Core Optick::OptickCore SDL2::SDL2 glm::glm ${Vulkan_LIBRARIES} GPUOpen::VulkanMemoryAllocator imgui::imgui) 