#include <cmath>
#include <valarray>
#include <iostream>
#include <sstream>
#include <string>
#include <ncurses.h>
#include <signal.h>
#include <sys/ioctl.h>

using namespace std;

const valarray<float> SKY_COLOR = {120.0f, 190.0f, 250.0f};

uint8_t* screenBuffer; // The buffer should be 6x the size of the screen for 3 color channels across a foreground and background color
struct winsize terminalSize;

struct RayHitInfo {
	valarray<float> color;
	valarray<float> pos;
	valarray<float> dir;
	valarray<float> normal;
	float nextIntensity;
	bool hitSky;
};

struct Sphere {
	valarray<float> pos;
	valarray<float> color;
};


inline int coordsToScreenBufferIndex(int i, int j) {
	return (i * terminalSize.ws_col + j) * 6;
}

inline float magnitude(valarray<float> vec) {
	return sqrt(vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]);
}

inline valarray<float> normalize(valarray<float> vec) {
	float mag = magnitude(vec);
	vec = {
		vec[0] / mag,
		vec[1] / mag,
		vec[2] / mag
	};
	return vec;
}

inline float dot(valarray<float> vec1, valarray<float> vec2) {
	return vec1[0] * vec2[0] + vec1[1] * vec2[1] + vec1[2] * vec2[2];
}

inline float distance(valarray<float> vec1, valarray<float> vec2) {
	return magnitude(vec1 - vec2);
}

void onExit(int s) {
	// Reset terminal colors and formatting
	std::cout << "\e[0m\e[H\e[J\e[?25h" << endl;

	// Clear the onExit signal to prevent possible recursion
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = NULL;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);

	endwin();
	exit(1);
}

RayHitInfo raycast(RayHitInfo lastRay) {
	// Copy ray info
	RayHitInfo ray;
	ray.pos = lastRay.pos;
	ray.dir = lastRay.dir;
	ray.color = SKY_COLOR;
	ray.hitSky = true;
	ray.nextIntensity = 0.0;

	// Set up scene
	Sphere sphere;
	sphere.pos = {0, 2, 8};
	sphere.color = {250.0f, 0.0f, 0.0f};

	float minDist = 9999.0;
	for (float i = 0.0; i < 65536.0; i += minDist) {
		// Distance from sphere
		float dist = distance(ray.pos, sphere.pos) - 1.0;
		float bias = i / 512.0; // Higher bias the further away the ray is
		minDist = dist; // Used for checking how far the ray is allowed to go

		if (dist <= bias) { // Draw sphere
			ray.color = sphere.color;
			ray.normal = normalize(ray.pos - sphere.pos);

			ray.dir = ray.dir - 2 * ray.normal * dot(ray.dir, ray.normal);

			ray.pos = sphere.pos + (ray.pos - sphere.pos);
			ray.nextIntensity = lastRay.nextIntensity * 0.5;
			ray.hitSky = false;
			break;
		}

		dist = ray.pos[1]; // Floor distance
		minDist = min(dist, minDist);

		if (dist <= bias) { // Draw floor
			// Should the given tile be black or white?
			bool tileBrightness = (((int) ray.pos[0]) + ((int) ray.pos[2])) % 2 == 0;
			tileBrightness = ray.pos[0] < 0 ? !tileBrightness : tileBrightness;
			tileBrightness = ray.pos[2] < 0 ? !tileBrightness : tileBrightness;

			if (tileBrightness) {
				ray.color = {255.0, 255.0, 255.0};
			} else {
				ray.color = {0.0, 0.0, 0.0};
			}

			ray.hitSky = false;
			ray.normal = {0.0f, 1.0f, 0.0f};
			break;
		}

		// Move ray
		ray.pos += ray.dir * minDist * 1.01;
	}

	if (ray.hitSky) {
		ray.color *= max(1.0, 1.5 - distance(normalize(ray.pos), normalize({1.0f, 1.0f, 0.0f})));
	}

	return ray;
}

float getSunLight(valarray<float> pos, valarray<float> normal) {
	valarray<float> sunVec = {1.0f, 1.0f, 0.0f};

	valarray<float> adjust = {0.02f, 0.02f, 0.02f};

	RayHitInfo sunRay;
	sunRay.pos = pos + normal * adjust + sunVec * adjust;
	sunRay.dir = sunVec;
	sunRay.nextIntensity = -1.0;
	sunRay = raycast(sunRay);

	return sunRay.hitSky ? clamp(pow((dot(normal, sunVec)), 1.25), 0.5, 2.0) : 0.5;
}

valarray<float> getPixel(int x, int y, valarray<float> pos, valarray<float> rot) {
	RayHitInfo ray;
	
	// Get position for ray
	ray.pos = pos;

	// Get direction for ray, correcting for aspect ratio too
	ray.dir = normalize({
		(float(x) / float(terminalSize.ws_col * 2)) * 1.0f - 0.5f,
		(float(y) / float(terminalSize.ws_row * 2)) * 1.0f - 0.5f,
		1.0
	});
	ray.dir[1] = -ray.dir[1];

	// Correct for aspect ratio
	ray.dir[0] *= float(terminalSize.ws_col / 2.0) / float(terminalSize.ws_row);

	// Rotate direction from camera rotation
	ray.dir = normalize({
		ray.dir[0],
		ray.dir[1] * cos(rot[1]) - ray.dir[2] * sin(rot[1]),
		ray.dir[1] * sin(rot[1]) + ray.dir[2] * cos(rot[1])
	});
	ray.dir = normalize({
		ray.dir[0] * cos(rot[0]) - ray.dir[2] * sin(rot[0]),
		ray.dir[1],
		ray.dir[0] * sin(rot[0]) + ray.dir[2] * cos(rot[0])
	});

	ray.color = SKY_COLOR;
	ray.nextIntensity = 1.0;

	// Cast the ray
	valarray<float> color = {0, 0, 0};
	float nextIntensity = 1.0;
	while (ray.nextIntensity > 0) {
		ray = raycast(ray);
		color += ray.color * (nextIntensity - ray.nextIntensity);
		nextIntensity = ray.nextIntensity;

		if (ray.nextIntensity > 0) {
			ray.pos += ray.dir * 0.1;
		}
	}

	if (!ray.hitSky) {
		float sunLightAmount = getSunLight(ray.pos, ray.normal);
		color *= sunLightAmount;
	}

	return color;
}

int main() {
	// Setup the onExit signal to properly close the program
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = onExit;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);

	// Get the size of the terminal
	ioctl(0, TIOCGWINSZ, &terminalSize);

	// The buffer should be 6x the size of the screen for 3 color channels across a foreground and background color
	uint8_t _screenBuffer[terminalSize.ws_row * terminalSize.ws_col * 3 * 2];
	screenBuffer = _screenBuffer;
	
	WINDOW *win = initscr();
	keypad(stdscr, TRUE);
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
	mouseinterval(0);
	nodelay(win, false);

	valarray<float> camera = {0.0f, 2.0f, 0.0f};
	valarray<float> rotation = {0.0f, 0.0f};
	valarray<int> mouseDown = {0, 0};

	// Main loop
	while (1) {
		stringstream stringBuffer;

		// Sets cursor position to the top-left-most position and makes the cursor not blink for betting looking text rendering
		// Also makes the foreground color pure white and the background color pure black for consistency
		stringBuffer << "\e[H\e[?25l\e[48;2;0;0;0m\e[38;2;255;255;255m"; 

		for (int y = 0; y < terminalSize.ws_row; y++) {
			for (int x = 0; x < terminalSize.ws_col; x++) {
				valarray<float> colorTop = getPixel(x * 2, y * 2, camera, rotation);
				valarray<float> colorBottom = getPixel(x * 2, y * 2 + 1, camera, rotation);

				int r0 = min(255, int(colorTop[0]));
				int g0 = min(255, int(colorTop[1]));
				int b0 = min(255, int(colorTop[2]));
				int r1 = min(255, int(colorBottom[0]));
				int g1 = min(255, int(colorBottom[1]));
				int b1 = min(255, int(colorBottom[2]));

				// Construct and print the ANSI code to color the foreground and background, then print a unicode character to make two pixels
				stringBuffer << "\e[48;2;" << r0 << ";" << g0 << ";" << b0 << "m\e[38;2;" << r1 << ";" << g1 << ";" << b1 << "m▄";
			}
		}
		// Reset color
		stringBuffer << "\e[0m";
		std::cout << stringBuffer.str();

		valarray<float> movement = {0.0f, 0.0f, 0.0f};
		int ch = wgetch(win);
		if (ch == 119) {
			movement[2]++;
		} else if (ch == 115) {
			movement[2]--;
		} else if (ch == 97) {
			movement[0]--;
		} else if (ch == 100) {
			movement[0]++;
		} else if (ch == KEY_MOUSE) {
			MEVENT event;
			if (getmouse(&event) == OK) {
				if (event.bstate & BUTTON1_PRESSED) {
					mouseDown = {event.x, event.y};
				} else if (event.bstate & BUTTON1_RELEASED) {
					rotation[0] += (float(terminalSize.ws_col) / float(terminalSize.ws_row * 2)) * float(event.x - mouseDown[0]) / float(terminalSize.ws_col);
					rotation[1] += float(mouseDown[1] - event.y) / float(terminalSize.ws_row);
				}
			}
		}

		if (movement[0] != 0 || movement[2] != 0) {
			movement = {
				movement[0] * cos(rotation[0]) - movement[2] * sin(rotation[0]),
				0.0,
				movement[0] * sin(rotation[0]) + movement[2] * cos(rotation[0])
			};
			camera += movement;
		}
	}
}