struct Pointlight
{
	vec4 m_positionDistance;	// w = distance used for attenuation/culling
	vec4 m_colourBrightness;	// w = brightness
};

layout(buffer_reference, std430) readonly buffer PointlightBuffer { 
	Pointlight data[];
};

struct Spotlight
{
	vec4 m_positionDistance;		// w = distance used for attenuation/culling
	vec4 m_colourOuterAngle;		// colour is premultiplied by brightness, w = outer angle
	vec4 m_directionInnerAngle;		// direction is normalized, w = inner angle
};

layout(buffer_reference, std430) readonly buffer SpotlightBuffer { 
	Spotlight data[];
};

struct ShadowMetadata				// all info about shadow maps
{
	mat4 m_sunShadowCascadeMatrices[4];			// world->light transform for each cascade
	float m_sunShadowCascadeDistances[4];		// view-space distance from near plane for each cascade
	uint m_sunShadowCascadeCount;
	uint m_padding[3];
};

struct LightsData
{
	ShadowMetadata m_shadows;
	vec4 m_sunDirectionBrightness;
	vec4 m_sunColourAmbient;
	vec4 m_skyColourAmbient;
	PointlightBuffer m_allPointlights;
	SpotlightBuffer m_allSpotlights;
	uint m_pointlightCount;	
	uint m_spotlightCount;	
};

layout(buffer_reference, std430) readonly buffer LightsBuffer { 
	LightsData data[];
};

// based on Moving Frostbite to Physically Based Rendering 3.0
// inv. square falloff with radius to force taper to 0
float GetDistanceAttenuation(vec3 pixelToLight, float lightMaxDistance)
{
	float sqDistance = dot(pixelToLight,pixelToLight);
	float invSqFalloff = 1.0 / sqDistance;
	float invSqRadius = 1.0 / (lightMaxDistance * lightMaxDistance);
	
	// ensure the attenuation reaches 0 at the light radius
	// from Moving Frostbite to Physically Based Rendering 3.0
	float factor = sqDistance * invSqRadius;
	float smoothFactor = clamp(1.0 - ( factor * factor ), 0.0, 1.0);
	smoothFactor = smoothFactor * smoothFactor;
	
	return invSqFalloff * smoothFactor;
}

// point lights just use distance attenuation
float GetPointlightAttenuation(Pointlight pl, vec3 pixelToLight)
{
	return GetDistanceAttenuation(pixelToLight, pl.m_positionDistance.w);
}

float GetSpotlightAttenuation(Spotlight sl, vec3 pixelToLight, vec3 pixelToLightDirection)
{
	float attenuation = GetDistanceAttenuation(pixelToLight, sl.m_positionDistance.w);
	float theta = dot(-sl.m_directionInnerAngle.xyz, pixelToLightDirection); 
	float outerAngle = sl.m_colourOuterAngle.w;
	float innerAngle = sl.m_directionInnerAngle.w;
	attenuation *= clamp((theta - outerAngle) / (innerAngle - outerAngle), 0.0, 1.0);
	return attenuation;
}