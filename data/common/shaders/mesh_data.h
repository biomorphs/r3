#extension GL_EXT_shader_16bit_storage : require

struct MeshVertexPosUV
{
	vec3 m_position;
	float16_t m_uv[2];
};

struct MeshVertexNormTangent
{
	float16_t m_normal[3];
	float16_t m_tangent[3];
};

layout(buffer_reference, std430) readonly buffer VertexPosUVBuffer { 
	MeshVertexPosUV data[];
};

layout(buffer_reference, std430) readonly buffer VertexNormTangentBuffer { 
	MeshVertexNormTangent data[];
};

struct MeshMaterial
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
	uint m_flags;
};

#define MESH_MATERIAL_ALPHA_PUNCH_FLAG 1

layout(buffer_reference, std430) readonly buffer MaterialBuffer { 
	MeshMaterial data[];
};

struct MeshPart					// Describes a single mesh part
{
	mat4 m_transform;					// relative to the model
	vec4 m_boundsMin;					// mesh space bounds
	vec4 m_boundsMax;
	uint m_indexStartOffset;			// offset into index data
	uint m_indexCount;					//
	uint m_materialIndex;				// index into materials array
	uint m_vertexDataOffset;			// used when generating draw calls
};

layout(buffer_reference, std430) readonly buffer MeshPartsBuffer
{
	MeshPart data[];
};

// per-instance data used when drawing a mesh
struct MeshInstanceData 
{
	mat4 m_transform;		// final model matrix of the part
	MaterialBuffer m_material;	// materal buffer address passed per-instance, only use m_material.materials[0]!
};

//all instance data passed via storage buffer (use gl_InstanceIndex to get the current index)
layout(buffer_reference, std430) readonly buffer InstancesBuffer
{
	MeshInstanceData data[];
};