mkdir shaders_spirv
cd shaders_spirv
mkdir common
cd ..\common\shaders
glslc immediate_render.vert -g -o ..\..\shaders_spirv\common\immediate_render.vert.spv
glslc immediate_render.frag -g -o ..\..\shaders_spirv\common\immediate_render.frag.spv
glslc mesh_render.vert -g -o ..\..\shaders_spirv\common\mesh_render.vert.spv
glslc mesh_render_forward.frag -g -o ..\..\shaders_spirv\common\mesh_render_forward.frag.spv
glslc mesh_render_forward.frag -DUSE_TILED_LIGHTS -g -o ..\..\shaders_spirv\common\mesh_render_forward_tiled.frag.spv
glslc mesh_render_gbuffer.frag -g -o ..\..\shaders_spirv\common\mesh_render_gbuffer.frag.spv
glslc mesh_render_shadow.frag -g -o ..\..\shaders_spirv\common\mesh_render_shadow.frag.spv
glslc mesh_prep_and_cull_instances_compute.comp -g -o ..\..\shaders_spirv\common\mesh_prep_and_cull_instances_compute.comp.spv
glslc tonemap_compute.comp -DUSE_REINHARD_COLOUR -g -o ..\..\shaders_spirv\common\tonemap_reinhard_compute.comp.spv
glslc tonemap_compute.comp -DUSE_REINHARD_LUMINANCE -g -o ..\..\shaders_spirv\common\tonemap_reinhard_luminance_compute.comp.spv
glslc tonemap_compute.comp -DUSE_AGX -g -o ..\..\shaders_spirv\common\tonemap_agx_compute.comp.spv
glslc tonemap_compute.comp -DUSE_AGX -DAGX_LOOK=1 -g -o ..\..\shaders_spirv\common\tonemap_agx_golden_look_compute.comp.spv
glslc tonemap_compute.comp -DUSE_AGX -DAGX_LOOK=2 -g -o ..\..\shaders_spirv\common\tonemap_agx_punchy_look_compute.comp.spv
glslc tonemap_compute.comp -DUSE_UNCHARTED_FILMIC -g -o ..\..\shaders_spirv\common\tonemap_uncharted_compute.comp.spv
glslc tonemap_compute.comp -DUSE_ACES_APPROX -g -o ..\..\shaders_spirv\common\tonemap_aces_approx_compute.comp.spv
glslc tonemap_compute.comp -DUSE_ACES_FITTED -g -o ..\..\shaders_spirv\common\tonemap_aces_fitted_compute.comp.spv
glslc deferred_lighting_compute.comp -g -o ..\..\shaders_spirv\common\deferred_lighting_compute_all_lights.comp.spv
glslc deferred_lighting_compute.comp -DUSE_TILED_LIGHTS -g -o ..\..\shaders_spirv\common\deferred_lighting_compute_tiled.comp.spv
glslc build_light_tile_frustums.comp -g -o ..\..\shaders_spirv\common\build_light_tile_frustums.comp.spv
glslc build_light_tiles_from_frustums.comp -g -o ..\..\shaders_spirv\common\build_light_tiles_from_frustums.comp.spv
glslc light_tile_debug_output.comp -g -o ..\..\shaders_spirv\common\light_tile_debug_output.comp.spv
glslc visualise_depth_texture.comp -g -o ..\..\shaders_spirv\common\visualise_depth_texture.comp.spv

pause

