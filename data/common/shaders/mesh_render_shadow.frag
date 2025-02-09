#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_render_shared.h"
#include "utils.h"

layout(location = 0) in vec2 inUV;
layout(location = 1) in flat uint inInstanceIndex;

void main() {
	MeshInstanceData thisInstance = PushConstants.m_instances.data[inInstanceIndex];
	MeshMaterial myMaterial = thisInstance.m_material.data[0];
	if((myMaterial.m_flags & MESH_MATERIAL_ALPHA_PUNCH_FLAG) != 0 && myMaterial.m_albedoTexture != -1)
	{
		vec4 albedoTex = texture(AllTextures[myMaterial.m_albedoTexture],inUV);
		float finalAlpha = myMaterial.m_albedoOpacity.a * albedoTex.a;
		if(finalAlpha < 0.5)
		{
			discard;
		}
	}
	
	// only output depth
	gl_FragDepth = gl_FragCoord.z;
}