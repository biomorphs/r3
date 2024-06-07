
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