
const float c_gamma = 2.2;

vec4 SRGBToLinear(vec4 v)
{
	return vec4(pow(v.rgb, vec3(c_gamma)), v.a);
}

vec3 LinearToSRGB(vec3 v)
{
	return pow(v.rgb, vec3(1.0 / c_gamma));
}