#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_render_shared.h"
#include "utils.h"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec3 inWorldspaceNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in flat uint inInstanceIndex;
layout(location = 4) in mat3 inTBN;

void main() {
	MeshInstanceData thisInstance = PushConstants.m_instances.data[inInstanceIndex];
	MeshMaterial myMaterial = thisInstance.m_material.data[0];
	float finalAlpha = myMaterial.m_albedoOpacity.a;
	if(myMaterial.m_albedoTexture != -1)
	{
		vec4 albedoTex = texture(AllTextures[myMaterial.m_albedoTexture],inUV);
		finalAlpha = finalAlpha * albedoTex.a;
	}
	
	if(finalAlpha < 0.25)	// punch-through alpha
	{
		discard;
	}
	
	// only output depth
	gl_FragDepth = gl_FragCoord.z;
}