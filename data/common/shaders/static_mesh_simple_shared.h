#include "mesh_data.h"
#include "lights.h"

struct GlobalConstants { 
	mat4 m_projViewTransform;
	vec4 m_cameraWorldSpacePos;
	VertexBuffer m_vertexBuffer;
	MaterialBuffer m_materialBuffer;
	AllLightsBuffer m_lightsBuffer;
};

struct PerInstanceData {
	mat4 m_transform;
	uint m_materialIndex;	// points into m_materialBuffer
};

// global constants stored in an array to allow overlapping frames
layout(buffer_reference, std430) readonly buffer GlobalConstantsBuffer
{
	GlobalConstants AllGlobals[];
};

//all instance data passed in global set via storage buffer (use gl_InstanceIndex to get the current index)
layout(std140,set = 0, binding = 0) readonly buffer AllInstancesBuffer
{
	PerInstanceData AllInstances[];
};

// all textures passed in one big array (index matches handle index)
layout (set = 1, binding = 0) uniform sampler2D allTextures[1024];

// index into globals sent via push constant
// don't know a better way of doing this yet, but its cheap
layout(push_constant) uniform constants
{
	GlobalConstantsBuffer m_globals;
} PushConstants;