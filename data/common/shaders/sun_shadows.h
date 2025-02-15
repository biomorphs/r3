// We are using hardware PCF along with our own PCF sampling
// For this to work all shadow samplers must have linear filtering and compare op less 

#define USE_HW_PCF
#define USE_SW_PCF			// use our own additional PCF 
#define MY_PCF_SAMPLES 3	// our own pcf in addition to hw

// shadow maps passed in texture arrays in set 1
#ifndef USE_HW_PCF
	layout (set = 1, binding = 0) uniform sampler2D SunShadowMaps[4];
#else
	layout (set = 1, binding = 0) uniform sampler2DShadow SunShadowMaps[4];
#endif

float CalculateSunShadow(ShadowMetadata shadowMetadata, mat4 worldToView, vec3 worldPos)
{
	// figure out which cascade to use based on view-space depth value
	float shadowValue = 1.0;
	vec3 viewSpacePosition = vec3(worldToView * vec4(worldPos,1.0));
	float viewDepth = -viewSpacePosition.z;
	int cascadeCount = int(shadowMetadata.m_sunShadowCascadeCount);
	int cascadeToSample = -1;
	for(int i=0;i<cascadeCount;i++)
	{
		float thisCascadeDistance = shadowMetadata.m_sunShadowCascadeDistances[i];
		if(viewDepth > thisCascadeDistance)
		{
			cascadeToSample = i;
		}
	}
	
	if(cascadeToSample != -1)
	{
		vec4 positionLightSpace = shadowMetadata.m_sunShadowCascadeMatrices[cascadeToSample] * vec4(worldPos,1.0); 
		vec3 projCoords = positionLightSpace.xyz / positionLightSpace.w;
		projCoords.xy = projCoords.xy * 0.5 + 0.5;		// transform from ndc space to UV space
		
#ifdef USE_SW_PCF
		float pcfScale = (1.0f / textureSize(SunShadowMaps[cascadeToSample], 0).x) * 1.0f;
		float pcfValue = 0.0f;
		for(int x=0;x<MY_PCF_SAMPLES;++x)
		{
			for(int y=0;y<MY_PCF_SAMPLES;++y)
			{
				float pcfOffsetX = pcfScale * (x - (MY_PCF_SAMPLES / 2));
				float pcfOffsetY = pcfScale * (y - (MY_PCF_SAMPLES / 2));
#ifdef USE_HW_PCF
				pcfValue += texture(SunShadowMaps[cascadeToSample], projCoords + vec3(pcfOffsetX, pcfOffsetY, 0)).r;
#else
				float distance = texture(SunShadowMaps[cascadeToSample], projCoords.xy + vec2(pcfOffsetX, pcfOffsetY)).r;
				pcfValue += distance > 0.0 && distance < projCoords.z ? 0.0 : 1.0;
#endif	// USE_HW_PCF
			}
		}
		shadowValue = pcfValue / (MY_PCF_SAMPLES * MY_PCF_SAMPLES);
#else
#ifdef USE_HW_PCF
	shadowValue = texture(SunShadowMaps[cascadeToSample], projCoords).r;
#else
	float distance = texture(SunShadowMaps[cascadeToSample], projCoords.xy).r;
	shadowValue = distance > 0.0 && distance < projCoords.z ? 0.0 : 1.0;
#endif	// USE_HW_PCF
#endif	// USE_SW_PCF
	}
	
	return shadowValue;
}