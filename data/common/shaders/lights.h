struct Pointlight
{
	vec4 m_positionDistance;	// w = distance used for attenuation/culling
	vec4 m_colourBrightness;	// w = brightness
	float m_attenuationFactor;	// adjusts falloff curve
	float m_padding[3];
};

layout(buffer_reference, std430) readonly buffer PointlightBuffer { 
	Pointlight lights[];
};

float GetPointlightAttenuationNatural(Pointlight pl, vec3 worldPos)
{
	float distance = length(pl.m_positionDistance.xyz - worldPos);			
	float attenuation = 1.0 / (distance * distance);
	return attenuation;
}

float GetPointlightAttenuation(Pointlight pl, vec3 worldPos)
{
	float distance = length(pl.m_positionDistance.xyz - worldPos);			
	float attenuation = pow(smoothstep(pl.m_positionDistance.w, 0, distance),pl.m_attenuationFactor);
	return attenuation;
}