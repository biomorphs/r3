#version 450
#extension GL_EXT_buffer_reference : require

#include "static_mesh_shared.h"
#include "utils.h"

layout(location = 0) out vec3 outWorldspacePos;
layout(location = 1) out vec3 outWorldspaceNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out flat uint outMaterialIndex;
layout(location = 4) out mat3 outTBN;


void main() {
	GlobalConstants globals = PushConstants.m_globals.AllGlobals[0];
	PerInstanceData thisInstance = globals.m_instancesBuffer.AllInstances[gl_InstanceIndex];
	StaticMeshMaterial myMaterial = globals.m_materialBuffer.materials[thisInstance.m_materialIndex];
	MeshVertex v = globals.m_vertexBuffer.vertices[gl_VertexIndex];
	vec4 worldSpacePosition = thisInstance.m_transform * vec4(v.m_positionU0.xyz, 1);
	outWorldspacePos = worldSpacePosition.xyz;
	outWorldspaceNormal = normalize(mat3(transpose(inverse(thisInstance.m_transform))) * v.m_normalV0.xyz);
	outUV = myMaterial.m_uvOffsetScale.xy + vec2(v.m_positionU0.w * myMaterial.m_uvOffsetScale.z, v.m_normalV0.w * myMaterial.m_uvOffsetScale.w);
	outMaterialIndex = thisInstance.m_materialIndex;
	outTBN = CalculateTBN(thisInstance.m_transform, v.m_tangentPad.xyz, v.m_normalV0.xyz);
	gl_Position = globals.m_projViewTransform * worldSpacePosition;	// clip space
}