#include "mesh_data.h"
#include "lights.h"

struct PerInstanceData {
	mat4 m_transform;
	uint m_materialIndex;	// points into m_materialBuffer
};

//all instance data passed via storage buffer (use gl_InstanceIndex to get the current index)
layout(buffer_reference, std430) readonly buffer AllInstancesBuffer
{
	PerInstanceData AllInstances[];
};

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