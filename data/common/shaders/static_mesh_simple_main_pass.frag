#version 450
#extension GL_EXT_buffer_reference : require

#include "static_mesh_simple_shared.h"
#include "pbr.h"
#include "utils.h"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec3 inWorldspaceNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in flat uint inMaterialIndex;
layout(location = 4) in mat3 inTBN;
layout(location = 0) out vec4 outColour;

float ParalaxHeight = 0.1;

vec2 ParallaxMappingBasic(uint heightTexId, vec2 texCoords, vec3 viewDir)
{ 
    float height =  1.0 - texture(allTextures[heightTexId],texCoords).r;
    vec2 p = viewDir.xy / viewDir.z * (height * ParalaxHeight);
    return texCoords - p;    
} 

vec2 ParallaxMappingSteep(uint heightTexId, vec2 texCoords, vec3 viewDir)
{ 
     // number of depth layers
    const float numLayers = 10;
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * ParalaxHeight; 
    vec2 deltaTexCoords = P / numLayers;
	// get initial values
	vec2  currentTexCoords     = texCoords;
	float currentDepthMapValue =  1.0 - texture(allTextures[heightTexId],currentTexCoords).r;
	
	while(currentLayerDepth < currentDepthMapValue)
	{
		// shift texture coordinates along direction of P
		currentTexCoords -= deltaTexCoords;
		// get depthmap value at current texture coordinates
		currentDepthMapValue = 1.0 - texture(allTextures[heightTexId],currentTexCoords).r;
		// get depth of next layer
		currentLayerDepth += layerDepth;  
	}
	return currentTexCoords;
} 

vec2 ParallaxOcclusionMapping(uint heightTexId, vec2 texCoords, vec3 viewDir)
{ 
     // number of depth layers
    const float numLayers = 10;
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * ParalaxHeight; 
    vec2 deltaTexCoords = P / numLayers;
	// get initial values
	vec2  currentTexCoords     = texCoords;
	float currentDepthMapValue =  1.0 - texture(allTextures[heightTexId],currentTexCoords).r;
	
	while(currentLayerDepth < currentDepthMapValue)
	{
		// shift texture coordinates along direction of P
		currentTexCoords -= deltaTexCoords;
		// get depthmap value at current texture coordinates
		currentDepthMapValue = 1.0 - texture(allTextures[heightTexId],currentTexCoords).r;
		// get depth of next layer
		currentLayerDepth += layerDepth;  
	}
	
	// get texture coordinates before collision (reverse operations)
	vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

	// get depth after and before collision for linear interpolation
	float afterDepth  = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = 1.0 - texture(allTextures[heightTexId], prevTexCoords).r - currentLayerDepth + layerDepth;
	 
	// interpolation of texture coordinates
	float weight = afterDepth / (afterDepth - beforeDepth);
	vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

	return finalTexCoords;
} 

float ParallaxSoftShadowMultiplier(uint heightTexId, vec3 fragToLightTangentSpace, vec2 initialTexCoord, float initialHeight)
{
	float shadowParalaxHeight = 0.1;
   float shadowMultiplier = 1;
   const float minLayers = 15;
   const float maxLayers = 30;

   // calculate lighting only for surface oriented to the light source
   if(dot(vec3(0, 0, 1), fragToLightTangentSpace) > 0)
   {
      // calculate initial parameters
      float numSamplesUnderSurface = 0;
      shadowMultiplier = 0;
      float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0, 0, 1), fragToLightTangentSpace)));
      float layerHeight = initialHeight / numLayers;
      vec2 texStep = shadowParalaxHeight * fragToLightTangentSpace.xy / fragToLightTangentSpace.z / numLayers;

      // current parameters
      float currentLayerHeight = initialHeight - layerHeight;
      vec2 currentTextureCoords = initialTexCoord + texStep;
      float heightFromTexture = 1.0 - texture(allTextures[heightTexId], currentTextureCoords).r;
      int stepIndex = 1;

      // while point is below depth 0.0 )
      while(currentLayerHeight > 0)
      {
         // if point is under the surface
         if(heightFromTexture < currentLayerHeight)
         {
            // calculate partial shadowing factor
            numSamplesUnderSurface += 1;
            float newShadowMultiplier = (currentLayerHeight - heightFromTexture) *
                                             (1.0 - stepIndex / numLayers);
            shadowMultiplier = max(shadowMultiplier, newShadowMultiplier);
         }

         // offset to the next layer
         stepIndex += 1;
         currentLayerHeight -= layerHeight;
         currentTextureCoords += texStep;
         heightFromTexture = 1.0 - texture(allTextures[heightTexId], currentTextureCoords).r;
      }

      // Shadowing factor should be 1 if there were no points under the surface
      if(numSamplesUnderSurface < 1)
      {
         shadowMultiplier = 1;
      }
      else
      {
         shadowMultiplier = 1.0 - shadowMultiplier;
      }
   }
   return shadowMultiplier;
} 

void main() {
	GlobalConstants globals = AllGlobals[PushConstants.m_globalIndex];
	vec3 worldPos = inWorldSpacePos;
	vec3 fragPosTbn = inTBN * inWorldSpacePos;
	vec3 normal = normalize(inWorldspaceNormal);
	vec3 viewDir = normalize(globals.m_cameraWorldSpacePos.xyz - worldPos);
	StaticMeshMaterial myMaterial = globals.m_materialBuffer.materials[inMaterialIndex];
	
	PBRMaterial mat;
	float finalAlpha = myMaterial.m_albedoOpacity.a;
	vec2 texCoords = inUV;
	if(myMaterial.m_heightmapTexture != -1)	// paralax map changes uvs, so must be first
	{
		vec3 viewPosTbn = inTBN * globals.m_cameraWorldSpacePos.xyz;
		texCoords = ParallaxOcclusionMapping(myMaterial.m_heightmapTexture, inUV, normalize(viewPosTbn - fragPosTbn));
		if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0)
		{
			discard;
		}
	}
	if(myMaterial.m_albedoTexture != -1)
	{
		vec4 albedoTex = texture(allTextures[myMaterial.m_albedoTexture],texCoords);
		mat.m_albedo = myMaterial.m_albedoOpacity.xyz * SRGBToLinear(albedoTex).xyz;
		finalAlpha = finalAlpha * albedoTex.a;
	}
	else
	{
		mat.m_albedo = myMaterial.m_albedoOpacity.xyz;
	}
	
	// early discard for punch-through alpha 
	if(finalAlpha < 0.25)
	{
		discard;
	}
	
	if(myMaterial.m_roughnessTexture != -1)
	{
		mat.m_roughness = texture(allTextures[myMaterial.m_roughnessTexture],texCoords).y;	// assuming combined rough/metalness
	}
	else
	{
		mat.m_roughness = myMaterial.m_roughness;
	}
	if(myMaterial.m_metalnessTexture != -1)
	{
		mat.m_metallic = texture(allTextures[myMaterial.m_metalnessTexture],texCoords).z;	// assuming combined rough/metalness
	}
	else
	{
		mat.m_metallic = myMaterial.m_metallic;
	}
	if(myMaterial.m_aoTexture != -1)
	{
		mat.m_ao = texture(allTextures[myMaterial.m_aoTexture],texCoords).x;
	}
	else
	{
		mat.m_ao = 1.0;
	}
	if(myMaterial.m_normalTexture != -1)	// normal mapping
	{
		normal = texture(allTextures[myMaterial.m_normalTexture],texCoords).xyz;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(inTBN * normal);
	}
	
	// Apply sun direct light
	vec3 sunRadiance = globals.m_sunColourAmbient.xyz * globals.m_sunDirectionBrightness.w;
	vec3 directLight = PBRDirectLighting(mat, viewDir, -globals.m_sunDirectionBrightness.xyz, normal, sunRadiance, 1.0);
	
	// Apply point lights (direct)
	uint pointLightCount = globals.m_pointLightCount;
	for(uint p=0;p<pointLightCount;++p)
	{
		Pointlight pl = globals.m_pointlightBuffer.lights[globals.m_firstPointLightOffset + p];
		vec3 lightRadiance = pl.m_colourBrightness.xyz * pl.m_colourBrightness.w;
		float attenuation = GetPointlightAttenuation(pl, worldPos);
		vec3 lightToPixel = normalize(pl.m_positionDistance.xyz - worldPos);
		
		float shadowMul = 1.0;
		if(myMaterial.m_heightmapTexture != -1)
		{
			vec3 lightToPixelTangentSpace = normalize((inTBN * pl.m_positionDistance.xyz) - fragPosTbn);
			float initialHeight = 1.0 - texture(allTextures[myMaterial.m_heightmapTexture],texCoords).r;
			shadowMul = ParallaxSoftShadowMultiplier(myMaterial.m_heightmapTexture, lightToPixelTangentSpace, inUV, initialHeight);
		}
		directLight += shadowMul * PBRDirectLighting(mat, viewDir, lightToPixel, normal, lightRadiance, attenuation);
	}
	
	// Ambient is a hack, tries to combine sky + sun colour somehow
	vec3 ambient = PBRGetAmbientLighting(mat, 
		globals.m_sunColourAmbient.xyz, 
		globals.m_sunColourAmbient.w,
		globals.m_skyColourAmbient.xyz,
		globals.m_skyColourAmbient.w
	);
	
	vec3 finalLight = ambient + directLight;	
	
	// tonemap using reinhard operator for now 
	// should be a separate fullscreen pass
    finalLight = finalLight / (finalLight + vec3(1.0));
	
#ifdef R3_CONVERT_OUTPUT_TO_SRGB
	finalLight = LinearToSRGB(finalLight);
#endif

	outColour = vec4(finalLight,finalAlpha);
}