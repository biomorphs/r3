#include "mesh_data.h"
#include "lights.h"

struct GlobalConstants { 
	mat4 m_projViewTransform;
	vec4 m_cameraWorldSpacePos;
	VertexBuffer m_vertexBuffer;
	MaterialBuffer m_materialBuffer;
	PointlightBuffer m_pointlightBuffer;
	uint m_firstPointLightOffset;
	uint m_pointLightCount;
};

// global constants stored in an array to allow overlapping frames
layout(buffer_reference, std430) readonly buffer GlobalConstantsBuffer { 
	GlobalConstants m_allGlobals[];
};

// transform and globals buffer addr/index sent per mesh
layout(push_constant) uniform constants
{
	mat4 m_instanceTransform;
	GlobalConstantsBuffer m_globals;
	int m_globalIndex;		// index into m_allGlobals
	int m_materialIndex;	// index into m_materials
} PushConstants;

// all textures passed in one big array (index matches handle index)
layout (set = 0, binding = 0) uniform sampler2D allTextures[1024];