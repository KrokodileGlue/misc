/*
 * A very simple and short fractal generator that does some antialiasing.
 * I will probably add some commandline parameters and clean this thing
 * up, it was written for a few quick images.
 * It's written more as a shader than a real piece of software.
 */

#include <iostream>
#include <fstream>

#include "glm/glm.hpp"

using namespace glm;

#define AA 9
#define NUM_ITERATIONS 1024
#define WIDTH 1024
#define HEIGHT 1024

float map(float x, float in_min, float in_max, float out_min, float out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

vec3 render_point(int x, int y)
{
	vec2 c = vec2(x, y) / vec2(WIDTH * AA, HEIGHT * AA) * 2.0f - 1.0f;
	c.x *= (float)WIDTH / (float)HEIGHT;

	// c *= 1.25;

	c += vec2(-1.5, 1.7);
	c *= 0.05;

	// c *= 0.03;
	// c += vec2(-0.744, 0.186);

	vec2 set = vec2(-0.6, -0.45);

	float l = 0.0;
	vec2 z = vec2(c);

	for (int n = 0; n < NUM_ITERATIONS; n++)
	{
		z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + set;
		if (dot(z, z) > 128.0)
			break;
		l += 1.0;
	}

	if (l == (float)NUM_ITERATIONS) return vec3(0.0);

	float sl = l - log2(log2(dot(z, z))) + 4.0;
	float al = smoothstep(-0.1, 0.0, 0.0);
	l = mix(l, sl, al);

	vec3 col = vec3(0.0);

	col.r = 0.5f + 0.5f * cos(3.0 + l * 0.15 + -0.98803162409286178998774890729446);
	col.g = 0.5f + 0.5f * cos(3.0 + l * 0.15 + 0.15425144988758405071866214661421);
	col.b = 0.5f + 0.5f * cos(3.0 + l * 0.15 + 0.25);

	col = col * col;
	col -= 0.5; col *= -1.0; col += 0.5;

	col *= 255.0;
	col = abs(col);

	return col;
}

int main(int argc, char** argv)
{
	std::ofstream output;
	output.open("output11.ppm");
	output << "P3\n" << WIDTH << " " << HEIGHT << "\n255\n";

	for (int y = 0; y < HEIGHT; y++) {
		int x;
		for (x = 0; x < WIDTH; x++) {
			vec3 color = vec3(0.0);
			for (int i = 0; i < AA; i++) {
				for (int j = 0; j < AA; j++) {
					color += render_point(x * AA + i, y * AA + j);
				}
			}

			color /= AA * AA;
			output << (int)color.r << " " << (int)color.g << " " << (int)color.b << "\n";
		}

		std::cout << ((double)(y * WIDTH + x) / (double)(WIDTH * HEIGHT))*100.0f << "%" << std::endl;
	}

	output.close();

	return EXIT_SUCCESS;
}