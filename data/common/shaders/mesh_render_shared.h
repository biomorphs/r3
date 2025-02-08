// include this in any shaders that draw meshes via MeshRenderer!

#include "mesh_data.h"
#include "lights.h"
#include "tiled_lighting.h"

// all textures passed in one big array (index matches cpu-side texture handle)
layout (set = 0, binding = 0) uniform sampler2D AllTextures[1024];

// Too big for push constants, sadface
struct Globals
{
	mat4 m_projViewTransform;
	mat4 m_worldToViewTransform;
	vec4 m_cameraWorldSpacePos;
	VertexBuffer m_vertexBuffer;
	LightsBuffer m_lightsBuffer;
	LightTileMetadataBuffer m_lightTileMetadata;	// only used with USE_TILED_LIGHTS in forward pass
};


layout(buffer_reference, std430) readonly buffer GlobalsBuffer { 
	Globals data[];
};

// Globals buffer sent via push constant
layout(push_constant) uniform constants
{
	GlobalsBuffer m_globals;
	InstancesBuffer m_instances;
} PushConstants;