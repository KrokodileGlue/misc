#define PI 3.1415926
#define CRISPNESS 32
#define SPEED 0.25

float box(vec3 p)
{
	vec3 q = fract(p) * 2.0 - 1.0;
	vec3 d = abs(q) - vec3(0.25);
	return min(max(d.x,max(d.y,d.z)),0.0) + length(max(d,0.0));
}

float trace(vec3 o, vec3 r) {
	float t = 0.0;
	for (int i = 0; i < CRISPNESS; i++) {
		vec3 p = o + r * t;
		float d = box(p);
		t += d * 0.5;
	}
	return t;
}

float fog_at(vec2 uv)
{
	vec3 r = normalize(vec3(uv, 1.0));
	float theta = PI / 4.0;
	r.xy *= mat2(cos(theta), -sin(theta), sin(theta), cos(theta));
	vec3 o = vec3(0.0, 0.0, iGlobalTime); 
	float t = trace(o, r);
	t -= 2.0;
	float fog = 1.0 / (1.0 + t * t * 0.1);

	return fog;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
	vec2 uv = fragCoord.xy / iResolution.xy - 0.5;
	vec3 col;
	float vignette = ((length(uv)-0.5)*-1.0)+0.5;
	vignette += 0.5;

	uv.x *= iResolution.x / iResolution.y;

	float fog = fog_at(uv);
	fog *= vignette;
	fog *= 0.5;
	vec3 cubes = vec3(fog/2.0, fog*2.0, sin(fog));

	vec3 gradient = vec3(uv + 0.5, 0.5);

	if (uv.x < 0.25 && uv.x > -0.25) {
		fog = fog_at(uv * mat2(cos(iGlobalTime * SPEED),
			-sin(iGlobalTime * SPEED),
			sin(iGlobalTime * SPEED),
			cos(iGlobalTime * SPEED)));
		fog *= vignette;
		fog *= 0.5;
	/* vec3 cubes = vec3(fog / 2.0, fog * 3.0, sin(fog));
	col = cubes + length(uv * 1.25) * gradient; */
		vec3 cubes = vec3(fog / 2.0, fog * 3.0, sin(fog));
		vec2 inverseuv = vec2(uv.x, uv.y - 1.0 * -1.0 + 1.0);
		cubes *= vec3(inverseuv + 0.25, 1.0);
		cubes *= 0.5;
		col = cubes + length(uv * 1.25) * gradient;
	} else {
		col = gradient + cubes * length(uv);
	}

	fragColor = vec4(col * col * col, 1.0);
}