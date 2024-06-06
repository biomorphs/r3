struct MeshVertex
{
	vec4 m_positionU0;
	vec4 m_normalV0;
	vec4 m_tangentPad;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer { 
	MeshVertex vertices[];
};

struct StaticMeshMaterial
{
	vec4 m_albedoOpacity;
	float m_metallic;						// 0.0 = dielectric, 1 = metallic
	float m_roughness;						// 0 = perfectly smooth, 1 = max roughness
	uint m_albedoTexture;					// -1 = no texture
	uint m_padding;
};

layout(buffer_reference, std430) readonly buffer MaterialBuffer { 
	StaticMeshMaterial materials[];
};