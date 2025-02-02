// include this in any shaders that draw meshes via MeshRenderer!

#include "mesh_data.h"
#include "lights.h"
#include "tiled_lighting.h"

// all textures passed in one big array (index matches cpu-side texture handle)
layout (set = 0, binding = 0) uniform sampler2D AllTextures[1024];

// Globals buffer sent via push constant
layout(push_constant) uniform constants
{
	mat4 m_projViewTransform;
	vec4 m_cameraWorldSpacePos;
	VertexBuffer m_vertexBuffer;
	LightsBuffer m_lightsBuffer;
	InstancesBuffer m_instances;
	LightTileMetadataBuffer m_lightTileMetadata;	// only used with USE_TILED_LIGHTS in forward pass
} PushConstants;