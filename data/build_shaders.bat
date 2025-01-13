mkdir shaders_spirv
cd shaders_spirv
mkdir vk_tutorials
mkdir common
cd ..\shaders\vk_tutorials
glslc fixed_triangle.vert -g -o ..\..\shaders_spirv\vk_tutorials\fixed_triangle.vert.spv
glslc fixed_triangle.frag -g -o ..\..\shaders_spirv\vk_tutorials\fixed_triangle.frag.spv
glslc triangle_from_buffers.vert -g -o ..\..\shaders_spirv\vk_tutorials\triangle_from_buffers.vert.spv
glslc triangle_from_buffers.frag -g -o ..\..\shaders_spirv\vk_tutorials\triangle_from_buffers.frag.spv
glslc triangle_from_buffers_with_push_constants.vert -g -o ..\..\shaders_spirv\vk_tutorials\triangle_from_buffers_with_push_constants.vert.spv
cd ..\..\common\shaders
glslc immediate_render.vert -g -o ..\..\shaders_spirv\common\immediate_render.vert.spv
glslc immediate_render.frag -g -o ..\..\shaders_spirv\common\immediate_render.frag.spv
glslc static_mesh.vert -g -o ..\..\shaders_spirv\common\static_mesh.vert.spv
glslc static_mesh_forward.frag -g -o ..\..\shaders_spirv\common\static_mesh_forward.frag.spv
glslc static_mesh_gbuffer.frag -g -o ..\..\shaders_spirv\common\static_mesh_gbuffer.frag.spv
glslc tonemap_compute.comp -g -o ..\..\shaders_spirv\common\tonemap_compute.comp.spv
glslc deferred_lighting_compute.comp -g -o ..\..\shaders_spirv\common\deferred_lighting_compute.comp.spv
pause

