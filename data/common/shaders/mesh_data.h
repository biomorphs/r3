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
	vec4 m_uvOffsetScale;					// uv offset/scale, useful for custom materials
	float m_metallic;						// 0.0 = dielectric, 1 = metallic
	float m_roughness;						// 0 = perfectly smooth, 1 = max roughness
	uint m_albedoTexture;					// -1 = no texture
	uint m_roughnessTexture;
	uint m_metalnessTexture;
	uint m_normalTexture;
	uint m_aoTexture;
};

layout(buffer_reference, std430) readonly buffer MaterialBuffer { 
	StaticMeshMaterial materials[];
};