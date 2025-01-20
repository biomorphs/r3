// include this in any shaders that draw static meshes via StaticMeshRenderer!

#include "mesh_data.h"
#include "lights.h"

struct GlobalConstants { 
	mat4 m_projViewTransform;
	vec4 m_cameraWorldSpacePos;
	VertexBuffer m_vertexBuffer;
	MaterialBuffer m_materialBuffer;
	AllLightsBuffer m_lightsBuffer;
	AllInstancesBuffer m_instancesBuffer;
};

// global constants stored in an array to allow overlapping frames
layout(buffer_reference, std430) readonly buffer GlobalConstantsBuffer
{
	GlobalConstants AllGlobals[];
};

// all textures passed in one big array (index matches cpu-side texture handle)
layout (set = 0, binding = 0) uniform sampler2D allTextures[1024];

// Globals buffer sent via push constant
layout(push_constant) uniform constants
{
	GlobalConstantsBuffer m_globals;
} PushConstants;