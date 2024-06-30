set(RenderLib_SourceFiles
	render_target_cache.h
	render_target_cache.cpp
	render_graph.h
	render_graph.cpp
	render_pass_context.h
	render_pass_context.cpp
	render_helpers.h
	render_helpers.cpp
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
	buffer_pool.h
	buffer_pool.cpp
	descriptors.h
	descriptors.cpp
	deletion_queue.h
	deletion_queue.cpp
	writeonly_gpu_buffer.h
	writeonly_gpu_buffer.cpp
	command_buffer_allocator.h
	command_buffer_allocator.cpp
)

add_library(Render ${RenderLib_SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../ FILES ${RenderLib_SourceFiles})	# adds directory structure filters in VS
target_include_directories(Render PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(Render PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Render PRIVATE Core Optick::OptickCore SDL2::SDL2 glm::glm ${Vulkan_LIBRARIES} GPUOpen::VulkanMemoryAllocator imgui::imgui) 