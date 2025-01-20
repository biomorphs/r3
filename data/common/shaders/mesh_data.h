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

struct StaticMeshPart					// Describes a single mesh part
{
	mat4 m_transform;					// relative to the model
	vec4 m_boundsMin;					// mesh space bounds
	vec4 m_boundsMax;
	uint m_indexStartOffset;			// offset into index data
	uint m_indexCount;					//
	uint m_materialIndex;				// index into materials array
	uint m_vertexDataOffset;			// used when generating draw calls
};

layout(buffer_reference, std430) readonly buffer AllStaticMeshPartsBuffer
{
	StaticMeshPart parts[];
};

// per-instance data used when drawing a mesh
struct PerInstanceData {
	mat4 m_transform;		// final model matrix of the part
	uint m_materialIndex;	// references MaterialBuffer.materials
};

//all instance data passed via storage buffer (use gl_InstanceIndex to get the current index)
layout(buffer_reference, std430) readonly buffer AllInstancesBuffer
{
	PerInstanceData AllInstances[];
};