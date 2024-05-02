struct Pointlight
{
	vec4 m_positionDistance;	// w = distance used for attenuation/culling
	vec4 m_colourBrightness;	// w = brightness
};

layout(buffer_reference, std430) readonly buffer PointlightBuffer { 
	Pointlight lights[];
};

// based on Moving Frostbite to Physically Based Rendering 3.0
// inv. square falloff with radius to force taper to 0
float GetPointlightAttenuation(Pointlight pl, vec3 worldPos)
{
	float distance = length(pl.m_positionDistance.xyz - worldPos);			
	float sqDistance = max(0.01, distance * distance);	// avoid divide by zero, also can set a size for the light? (min radius at full brightness)
	float invSqFalloff = 1.0 / sqDistance;
	float invSqRadius = 1.0 / (pl.m_positionDistance.w * pl.m_positionDistance.w);
	
	// ensure the attenuation reaches 0 at the light radius
	// from Moving Frostbite to Physically Based Rendering 3.0
	float factor = sqDistance * invSqRadius;
	float smoothFactor = clamp(1.0 - ( factor * factor ), 0.0, 1.0);
	smoothFactor = smoothFactor * smoothFactor;
	
	return invSqFalloff * smoothFactor;
}