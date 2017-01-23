#define M_PI 3.1415926
#define SCALE 5.0
#define SPEED 2.0
#define XOFF 3.0
#define YOFF 2.0

vec2 standard(vec2 z)
{
	z *= SCALE;
	z -= SCALE / 2.0;
	return z;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 uv = fragCoord.xy / iResolution.xy;
	uv = standard(uv);
	vec2 oguv = uv;

	uv.x *= iResolution.x / iResolution.y;
	uv = exp(log(uv)/2.0);

	float z = sqrt(1.0 - (uv.x * uv.x) - (uv.y * uv.y));
	float a = 1.0 / (z * tan((M_PI / 2.0) * 0.5));
	//uv = (uv.xy - 0.5) * 2.0 * a;

	vec2 unmodifieduv = uv;
	//vec2 mouse = vec2(sin(iGlobalTime*SPEED)*(1.0/SCALE)*4.0, cos(iGlobalTime*SPEED)*(1.0/SCALE)*4.0);
	//vec2 mouse = vec2((standard(iMouse.xy / iResolution.xy) * SCALE) * (1.0/SCALE));
	//mouse.x *= iResolution.x / iResolution.y;

	float the = -iGlobalTime;
	mat2 rotat = mat2(cos(the), -sin(the), sin(the), cos(the));
	uv *= rotat;

	float theta = 0.0;
	mat2 rot = mat2(cos(theta), -sin(theta), sin(theta), cos(theta));

	vec2 mouse = vec2(sin(iGlobalTime*SPEED)*(1.0/SCALE)*(SCALE+XOFF), cos(iGlobalTime*SPEED)*(1.0/SCALE)*(SCALE+YOFF));
	mouse *= rot;

	theta += M_PI / 2.0;
	rot = mat2(cos(theta), -sin(theta), sin(theta), cos(theta));
	vec2 mouse2 = mouse;
	mouse2 *= rot;

	theta += M_PI / 2.0;
	rot = mat2(cos(theta), -sin(theta), sin(theta), cos(theta));
	vec2 mouse3 = mouse;
	mouse3 *= rot;

	theta += M_PI / 2.0;
	rot = mat2(cos(theta), -sin(theta), sin(theta), cos(theta));
	vec2 mouse4 = mouse;
	mouse4 *= rot;

	float temp = length(uv - mouse);
	temp = pow(temp, 1.5);
	float temp2 = length(uv - mouse2);
	temp2 = pow(temp2, 1.5);
	float temp3 = length(uv - mouse3);
	temp3 = pow(temp3, 1.5);
	float temp4 = length(uv - mouse4);
	temp4 = pow(temp4, 1.5);
	float col = temp * temp2 * temp3 * temp4;

	float vignette = ((length(unmodifieduv)-0.5)*-1.0)+0.5;
	vignette += 0.5;

	col *= vignette;
	vec3 res = vec3(col, col*3.0, col/2.0);
	res -= 0.5; res *= -1.0; res += 0.5;
	res = pow(res, vec3(-0.8));

	/* if (fract(fragCoord.x / 2.0) <= 0.25) {
	res.gb = vec2(0.0);
	} */
	res.gb *= float(fract(fragCoord.x / 2.0) <= 0.25);

	res.r *= sin((fragCoord.x / iResolution.x));

	float vig = (((length(oguv)/2.0)-0.5)*-1.0)+0.5;
	vig += 0.5;
	res *= vig;

	fragColor = vec4(res , 1.0);
}