#define PI 3.14159265359
#define TAU 6.28318530718

float polygon(vec2 st, float sides, float radius) {
	float a = atan(st.x,st.y) + PI;
	float r = TAU / sides;
	float d = cos(floor(0.5 + a / r) * r - a) * length(st);
	return smoothstep(radius - 0.002, radius + 0.002, d);
}

float stepUpDown(float begin, float end, float t) {
	return step(begin, t) - step(end, t);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
	vec2 vignetteuv = (fragCoord.xy / iResolution.xy - 0.5) * -1.0 * .25;
	vec2 uv = fragCoord.xy / iResolution.xy - 0.5;
	uv.x *= iResolution.x / iResolution.y;
	uv.y += sin(iGlobalTime*0.25) * 0.025;
	
	vec3 cola = vec3(0.);
	vec3 colb = vec3(1.);
	float angle = PI / 4.0;
	uv *= mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
	vec3 col = vec3(0.);
	
	float d = polygon(uv, 4., 0.3);
	col += colb * d;
	d -= 0.5; d *= -1.0; d += 0.5;
	col += cola * d;
	col *= (length(vignetteuv) - 0.5) * -1.0 + 0.5;
	
	fragColor = vec4(col * col * col, 1.0);
}