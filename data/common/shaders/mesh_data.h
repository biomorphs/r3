struct MeshVertex
{
	vec4 m_positionU0;
	vec4 m_normalV0;
	vec4 m_tangentPad;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer { 
	MeshVertex vertices[];
};