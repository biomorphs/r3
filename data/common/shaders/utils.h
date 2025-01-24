
const float c_gamma = 2.2;

vec4 SRGBToLinear(vec4 v)
{
	return vec4(pow(v.rgb, vec3(c_gamma)), v.a);
}

vec3 LinearToSRGB(vec3 v)
{
	return pow(v.rgb, vec3(1.0 / c_gamma));
}

mat3 CalculateTBN(mat4 modelMat, vec3 tangent, vec3 normal)
{
	// Gram-shmidt from https://learnopengl.com
	vec3 T = normalize(vec3(modelMat * vec4(tangent, 0.0)));
	vec3 N = normalize(vec3(modelMat * vec4(normal, 0.0)));

	// re-orthogonalize T with respect to N
	T = normalize(T - dot(T, N) * N);
	// then retrieve perpendicular vector B with the cross product of T and N
	vec3 B = cross(N, T);

	return mat3(T, B, N); 
}

// performs normal mapping using a 2 channel view-space normal map
vec3 GetWorldspaceNormal(vec3 worldSpaceNormal, uint normalMapTexture, mat3 tbn, vec2 texCoords)
{
	vec3 normal = normalize(worldSpaceNormal);
	if(normalMapTexture != -1)	// normal mapping
	{
		normal.xy = texture(AllTextures[normalMapTexture],texCoords).xy;
		normal.xy = normal.xy * 2.0 - 1.0;		// convert to -1,1
		normal.z = sqrt(1.0 - (normal.x*normal.x) - (normal.y*normal.y));	// infer 3rd channel from x,y
		normal = normalize(tbn * normal);
	}
	return normal;
}

vec2 ParallaxMappingBasic(uint heightTexId, vec2 texCoords, vec3 viewDir, float paralaxAmount)
{ 
    float height =  1.0 - texture(AllTextures[heightTexId],texCoords).r;
    vec2 p = viewDir.xy / viewDir.z * (height * paralaxAmount);
    return texCoords - p;    
} 

vec2 ParallaxMappingSteep(uint heightTexId, vec2 texCoords, vec3 viewDir, float paralaxAmount)
{ 
     // number of depth layers
    const float numLayers = 10;
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * paralaxAmount; 
    vec2 deltaTexCoords = P / numLayers;
	// get initial values
	vec2  currentTexCoords     = texCoords;
	float currentDepthMapValue =  1.0 - texture(AllTextures[heightTexId],currentTexCoords).r;
	
	while(currentLayerDepth < currentDepthMapValue)
	{
		// shift texture coordinates along direction of P
		currentTexCoords -= deltaTexCoords;
		// get depthmap value at current texture coordinates
		currentDepthMapValue = 1.0 - texture(AllTextures[heightTexId],currentTexCoords).r;
		// get depth of next layer
		currentLayerDepth += layerDepth;  
	}
	return currentTexCoords;
} 

vec2 ParallaxOcclusionMapping(uint heightTexId, vec2 texCoords, vec3 viewDir, float paralaxAmount)
{ 
    const float minLayers = 8.0;
	const float maxLayers = 32.0;
	float numLayers = mix(maxLayers, minLayers, max(dot(vec3(0.0, 0.0, 1.0), viewDir), 0.0));
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * paralaxAmount; 
    vec2 deltaTexCoords = P / numLayers;
	// get initial values
	vec2  currentTexCoords     = texCoords;
	float currentDepthMapValue =  1.0 - texture(AllTextures[heightTexId],currentTexCoords).r;
	
	while(currentLayerDepth < currentDepthMapValue)
	{
		// shift texture coordinates along direction of P
		currentTexCoords -= deltaTexCoords;
		// get depthmap value at current texture coordinates
		currentDepthMapValue = 1.0 - texture(AllTextures[heightTexId],currentTexCoords).r;
		// get depth of next layer
		currentLayerDepth += layerDepth;  
	}
	
	// get texture coordinates before collision (reverse operations)
	vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

	// get depth after and before collision for linear interpolation
	float afterDepth  = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = 1.0 - texture(AllTextures[heightTexId], prevTexCoords).r - currentLayerDepth + layerDepth;
	 
	// interpolation of texture coordinates
	float weight = afterDepth / (afterDepth - beforeDepth);
	vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

	return finalTexCoords;
} 

float ParallaxSoftShadowMultiplier(uint heightTexId, vec3 fragToLightTangentSpace, vec2 initialTexCoord, float initialHeight, float paralaxAmount)
{
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
		vec2 texStep = paralaxAmount * fragToLightTangentSpace.xy / fragToLightTangentSpace.z / numLayers;

		// current parameters
		float currentLayerHeight = initialHeight - layerHeight;
		vec2 currentTextureCoords = initialTexCoord + texStep;
		float heightFromTexture = 1.0 - texture(AllTextures[heightTexId], currentTextureCoords).r;
		int stepIndex = 1;

		// while point is below depth 0.0 )
		while(currentLayerHeight > 0)
		{
			// if point is under the surface
			if(heightFromTexture < currentLayerHeight)
			{
				// calculate partial shadowing factor
				numSamplesUnderSurface += 1;
				float newShadowMultiplier = (currentLayerHeight - heightFromTexture) * (1.0 - stepIndex / numLayers);
				shadowMultiplier = max(shadowMultiplier, newShadowMultiplier);
			}

			// offset to the next layer
			stepIndex += 1;
			currentLayerHeight -= layerHeight;
			currentTextureCoords += texStep;
			heightFromTexture = 1.0 - texture(AllTextures[heightTexId], currentTextureCoords).r;
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