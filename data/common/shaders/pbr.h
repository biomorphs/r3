
struct PBRMaterial {
	vec3 m_albedo;
	float m_metallic;
	float m_roughness;
	float m_ao;
	float m_ambientMulti;
};

// Normal distribution function approximates area of surface aligned towards half vector given a roughness
float NormalDistributionGGX(vec3 normal, vec3 worldToCamera, float roughness);

// Geometry function approximates area of surface where micro-facets overlap, causing occlusion
float GeometrySmithDirect(vec3 normal, vec3 worldToCamera, vec3 lightToPixel, float roughness);

// Fresnel shlick method 
vec3 FresnelSchlick(float nDotL, vec3 F0);

const float PI = 3.14159265359;

vec3 PBRDirectLighting(
	vec3 worldToCamera, 
	vec3 worldPos, 
	vec3 worldNormal, 
	vec3 lightPosWorldSpace,
	vec3 lightColour,
	PBRMaterial material)
{
	vec3 lightToPixel = normalize(lightPosWorldSpace - worldPos);
    vec3 halfVec = normalize(worldToCamera + lightToPixel);
	float nDotL = max(dot(lightToPixel,worldNormal),0.0);
	 
	 // F0 = surface's response at normal incidence at a 0 degree angle as if looking directly onto a surface
	vec3 F0 = vec3(0.04); 				// most dialettics (non-metals) are around this value
    F0 = mix(F0, material.m_albedo, material.m_metallic);	// should not be calculated per light!
	
	// per light from here
	float distance = length(lightPosWorldSpace - worldPos);
	float attenuation = 1.0 / (distance * distance);
	vec3 lightRadiance = lightColour * attenuation;
	
	// cook-torrence BRDF
	float ndf = NormalDistributionGGX(worldNormal, worldToCamera, material.m_roughness);
	float geomTerm = GeometrySmithDirect(worldNormal, worldToCamera, lightToPixel, material.m_roughness);
	vec3 fresnel = FresnelSchlick(nDotL, F0);
	
	// specular amount comes from fresnel value. diffuse amount calculated from it
	vec3 specularAmount = fresnel;
	vec3 refractedAmount = vec3(1) - specularAmount;
	refractedAmount = refractedAmount * (1.0 - material.m_metallic);	// metallic materials dont refract light

	// specular calc
	vec3 numerator    = ndf * geomTerm * fresnel;
	float denominator = 4.0 * max(dot(worldNormal, worldToCamera), 0.0) * max(dot(worldNormal, lightToPixel), 0.0) + 0.0001;
	vec3 specular     = numerator / denominator;  
	
	vec3 radianceOut = (refractedAmount * material.m_albedo / PI + specular) * lightRadiance * nDotL;
	
	vec3 ambient = material.m_ambientMulti * material.m_albedo * material.m_ao;
	
	return radianceOut + ambient;
}

// https://learnopengl.com/PBR/Theory
// Trowbridge-Reitz GGX normal distribution function
float NormalDistributionGGX(vec3 normal, vec3 worldToCamera, float roughness)
{
    float a2     = roughness*roughness;
    float NdotH  = max(dot(normal, worldToCamera), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float nom    = a2;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom        = PI * denom * denom;
	
    return nom / denom;
}

// The Fresnel-Schlick approximation expects a F0 parameter which is known as the surface reflection at zero incidence or how much the surface reflects if looking directly at the surface
vec3 FresnelSchlick(float nDotL, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - nDotL, 5.0);
}

//Similar to the NDF, the Geometry function takes a material's roughness parameter as input with rougher surfaces having a higher probability of overshadowing microfacets
// a combination of the GGX and Schlick-Beckmann approximation known as Schlick-GGX
// note how roughness is remapped to a 'k' value
// Direct and IBL use different mappings
float GeometrySchlickGGX(float NdotV, float k)
{
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return nom / denom;
}
  
float GeometrySmithDirect(vec3 normal, vec3 worldToCamera, vec3 lightToPixel, float roughness)
{
	float k = ((roughness + 1.0) * (roughness + 1.0)) / 8.0;	// direct lighting mapping
    float NdotV = max(dot(normal, worldToCamera), 0.0);
    float NdotL = max(dot(normal, lightToPixel), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);
	
    return ggx1 * ggx2;
}