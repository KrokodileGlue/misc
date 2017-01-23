#define NUM_ITERATIONS 24

float map(vec3 p) {
	vec3 q = fract(p) * 2.0 - 1.0;

	return length(q) - 0.25;
}

float trace(vec3 o, vec3 r) {
	float t = 0.0;
	for (int i = 0; i < NUM_ITERATIONS; i++) {
		vec3 p = o + r * t;
		float d = map(p);
		t += d * 0.5;
	}
	return t;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
	vec2 uv = fragCoord.xy / iResolution.xy;
	uv = uv * 2.0 - 1.0;
	vec2 unmodifieduv = uv;
	uv.x *= iResolution.x / iResolution.y;

	vec3 r = normalize(vec3(uv, sin((iGlobalTime-20.0))/8.0));
	vec3 o = vec3(0.0, 0.0, iGlobalTime);

	//float theta = iGlobalTime / 2.0;
	float theta = 3.1415926 / 4.0;
	r.xy *= mat2(cos(theta), -sin(theta), sin(theta), cos(theta));

	float t = trace(o, r);
	t -= 2.0;
	float fog = 1.0 / (1.0 + t * t * 0.1);
	float vignette = ((length(unmodifieduv)-0.5)*-1.0)+0.5;
	vignette += 0.5; fog *= vignette;
	vec3 col = vec3(fog/2.0, sin(fog), fog*2.0);
	//col *= 0.75;

	fragColor = vec4(col * col * col, 1.0);
}