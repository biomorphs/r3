#include "mesh_data.h"
#include "lights.h"

struct GlobalConstants { 
	mat4 m_projViewTransform;
	vec4 m_cameraWorldSpacePos;
	vec4 m_sunColourAmbient;
	vec4 m_sunDirectionBrightness;
	vec4 m_skyColourAmbient;
	VertexBuffer m_vertexBuffer;
	MaterialBuffer m_materialBuffer;
	PointlightBuffer m_pointlightBuffer;
	uint m_firstPointLightOffset;
	uint m_pointLightCount;
};

struct PerInstanceData {
	mat4 m_transform;
	uint m_materialIndex;	// points into m_materialBuffer
};

// global constants stored in an array to allow overlapping frames
layout(set = 0, binding = 0) uniform GlobalConstantsBuffer
{
	GlobalConstants AllGlobals[4];
};

//all instance data passed in global set via storage buffer (use gl_InstanceIndex to get the current index)
layout(std140,set = 0, binding = 1) readonly buffer AllInstancesBuffer{

	PerInstanceData AllInstances[];
};

// all textures passed in one big array (index matches handle index)
layout (set = 1, binding = 0) uniform sampler2D allTextures[1024];

// index into globals sent via push constant
// don't know a better way of doing this yet, but its cheap
layout(push_constant) uniform constants
{
	uint m_globalIndex;		// index into m_allGlobals
	uint m_padding;
} PushConstants;