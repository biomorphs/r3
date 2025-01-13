mkdir shaders_spirv
cd shaders_spirv
mkdir vk_tutorials
mkdir common
cd ..\shaders\vk_tutorials
glslc fixed_triangle.vert -o ..\..\shaders_spirv\vk_tutorials\fixed_triangle.vert.spv
glslc fixed_triangle.frag -o ..\..\shaders_spirv\vk_tutorials\fixed_triangle.frag.spv
glslc triangle_from_buffers.vert -o ..\..\shaders_spirv\vk_tutorials\triangle_from_buffers.vert.spv
glslc triangle_from_buffers.frag -o ..\..\shaders_spirv\vk_tutorials\triangle_from_buffers.frag.spv
glslc triangle_from_buffers_with_push_constants.vert -o ..\..\shaders_spirv\vk_tutorials\triangle_from_buffers_with_push_constants.vert.spv
cd ..\..\common\shaders
glslc immediate_render.vert -o ..\..\shaders_spirv\common\immediate_render.vert.spv
glslc immediate_render.frag -o ..\..\shaders_spirv\common\immediate_render.frag.spv
glslc static_mesh.vert -o ..\..\shaders_spirv\common\static_mesh.vert.spv
glslc static_mesh_forward.frag -o ..\..\shaders_spirv\common\static_mesh_forward.frag.spv

glslc tonemap_compute.comp -o ..\..\shaders_spirv\common\tonemap_compute.comp.spv

pause

