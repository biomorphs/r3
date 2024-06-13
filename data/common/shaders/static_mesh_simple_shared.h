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

// global constants stored in an array to allow overlapping frames
layout(set = 0, binding = 0) uniform GlobalConstantsBuffer
{
	GlobalConstants AllGlobals[3];
};

// transform and globals buffer addr/index sent per mesh
layout(push_constant) uniform constants
{
	mat4 m_instanceTransform;
	int m_materialIndex;	// index into m_materials
	int m_globalIndex;		// index into m_allGlobals
} PushConstants;

// all textures passed in one big array (index matches handle index)
layout (set = 1, binding = 0) uniform sampler2D allTextures[1024];