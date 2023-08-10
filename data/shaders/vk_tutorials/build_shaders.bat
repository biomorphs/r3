cd ..\..\
mkdir shaders_spirv
cd shaders_spirv
mkdir vk_tutorials
cd ..\shaders\vk_tutorials
glslc fixed_triangle.vert -o ..\..\shaders_spirv\vk_tutorials\fixed_triangle.vert.spv
glslc fixed_triangle.frag -o ..\..\shaders_spirv\vk_tutorials\fixed_triangle.frag.spv
glslc triangle_from_buffers.vert -o ..\..\shaders_spirv\vk_tutorials\triangle_from_buffers.vert.spv
glslc triangle_from_buffers.frag -o ..\..\shaders_spirv\vk_tutorials\triangle_from_buffers.frag.spv
glslc triangle_from_buffers_with_push_constants.vert -o ..\..\shaders_spirv\vk_tutorials\triangle_from_buffers_with_push_constants.vert.spv
pause

