struct GlobalConstants { 
	mat4 m_projViewTransform;
	vec4 m_cameraWorldSpacePos;
	VertexBuffer m_vertexBuffer;
	MaterialBuffer m_materialBuffer;
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