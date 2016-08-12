/*
 * main.c
 *
 *  Created on: Jul 19, 2016
 *      Author: root
 */

#include "smem.h"
#include <stdio.h>
#include <stdlib.h>
#define GLEW_STATIC
#include <GL/glew.h>
#include "gui.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
#include <errno.h>
#include "render.h"
#include <unistd.h>
#include <fcntl.h>
#include "globals.h"
#include <time.h>
#include "xstring.h"
#include "physics2.h"

void main_preinit() {
	windowTitle = "Waveform Engine";
	frameLimit = 60;
	tps = 60;
}

struct physics2_ctx* pctx;

void test_render(float partialTick) {
	glViewport(0, 0, width, height);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0., width * zoom, height * zoom, 0., 1000., 3000.);
	//glOrtho(0., width, height, 0., 1000., 3000.);
	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0., 0., -2000.);
	//gui
	float wzoom = width * zoom;
	float hzoom = height * zoom;
	glTranslatef(-camX + wzoom / 2., -camY + hzoom / 2., 0.);
	glColor4f(1., 0., 0., 1.);
	physics2_drawAllShapes(pctx, camX - wzoom / 2., camY - hzoom / 2., camX + wzoom / 2., camY + hzoom / 2., partialTick);
}

union physics2_shape wall1;
union physics2_shape wall2;
union physics2_shape wall3;
union physics2_shape wall4;

void tsim() {
	physics2_simulate(pctx);
	//wall1.poly->ploc.x = wall1.poly->loc.x = width / 2.;
	//wall1.poly->ploc.y = wall1.poly->loc.y = -50;
	//wall1.poly->width = width;
	wall1.poly->rot = 0.;
	wall1.poly->rps = 0.;
	wall1.poly->vel.x = 0.;
	wall1.poly->vel.y = 0.;
	//wall2.poly->ploc.x = wall2.poly->loc.x = width / 2.;
	//wall2.poly->ploc.y = wall2.poly->loc.y = height + 50.;
	//wall2.poly->width = width;
	wall2.poly->rot = 0.;
	wall2.poly->rps = 0.;
	wall2.poly->vel.x = 0.;
	wall2.poly->vel.y = 0.;
	//wall3.poly->ploc.x = wall3.poly->loc.x = -50.;
	//wall3.poly->ploc.y = wall3.poly->loc.y = height / 2.;
	//wall3.poly->height = height;
	wall3.poly->rot = 0.;
	wall3.poly->rps = 0.;
	wall3.poly->vel.x = 0.;
	wall3.poly->vel.y = 0.;
	//wall4.poly->ploc.x = wall4.poly->loc.x = width + 50.;
	//wall4.poly->ploc.y = wall4.poly->loc.y = height / 2.;
	//wall4.poly->height = height;
	wall4.poly->rot = 0.;
	wall4.poly->rps = 0.;
	wall4.poly->vel.x = 0.;
	wall4.poly->vel.y = 0.;
}

int sfs;

void test_tick() {
	if (!paused) tsim();

}

void test_text(unsigned int c) {
	//if (c == 'g') tsim();
}

void test_keyboard(int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) paused = !paused;
	else if (key == GLFW_KEY_S && paused && (action == GLFW_PRESS || action == GLFW_REPEAT)) tsim();
}

double lmx = 0.;
double lmy = 0.;

void test_mouseMotion(double x, double y) {
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
		double dx = lmx - x;
		double dy = lmy - y;
		camX += dx * zoom;
		camY += dy * zoom;
	}
	lmx = x;
	lmy = y;
}

void test_scroll(double x, double y) {
	zoom -= y;
	if (zoom < 1.) zoom = 1.;
}

void main_init() {
	//glEnable (GL_MULTISAMPLE);
	//glEnable (GL_MULTISAMPLE_ARB);
	//glEnable (GL_DEPTH_TEST);
	//glEnable (GL_TEXTURE_2D);
	//glAlphaFunc(GL_GREATER, 0.1);
	//glEnable (GL_ALPHA_TEST);
	zoom = 1.;
	paused = 1;
	camX = width / 2.;
	camY = height / 2.;
	glEnable (GL_CULL_FACE);
	//load guis here via guistate_register_<event>(event)
	pctx = physics2_init();
	//union physics2_shape shape = physics2_newRect(150., 100.);
	union physics2_shape shape = physics2_newCircle(50.);
	//physics2_addShape(pctx, shape);
	shape.poly->mass = 15000.;
	shape.poly->ploc.x = shape.poly->loc.x = 500.;
	shape.poly->ploc.y = shape.poly->loc.y = 500.;
	shape.poly->vel.x = -2.;
	shape.poly->vel.y = -2.;
	physics2_calculateMOI(shape);
	//shape.poly->rps = .01;
	//shape.poly->rot = 1;
	union physics2_shape shape2 = physics2_newRect(100., 150.);
	//physics2_addShape(pctx, shape2);
	shape2.poly->mass = 15000.;
	shape2.poly->ploc.x = shape2.poly->loc.x = 300.;
	shape2.poly->ploc.y = shape2.poly->loc.y = 200.;
	shape2.poly->vel.x = 1.;
	shape2.poly->vel.y = 1.;
	physics2_calculateMOI(shape2);

	union physics2_shape shape3 = physics2_newCircle(50.);
	//physics2_addShape(pctx, shape3);
	shape3.poly->mass = 15000.;
	shape3.poly->ploc.x = shape3.poly->loc.x = 500.;
	shape3.poly->ploc.y = shape3.poly->loc.y = 300.;
	shape3.poly->vel.x = 0.;
	shape3.poly->vel.y = 2.;
	physics2_calculateMOI(shape3);
	/*vec2f pts[5];
	 pts[0].x = -75;
	 pts[0].y = -75;
	 pts[1].x = 75;
	 pts[1].y = -75;
	 pts[2].x = 90;
	 pts[2].y = 0;
	 pts[3].x = 0;
	 pts[3].y = 75;
	 pts[4].x = -90;
	 pts[4].y = 0;*/
	vec2f pts[6];
	pts[0].x = 200.;
	pts[0].y = 0.;
	pts[1].x = 0.;
	pts[1].y = 0.;
	pts[2].x = 0.;
	pts[2].y = 200.;
	pts[3].x = 100.;
	pts[3].y = 200.;
	pts[4].x = 100.;
	pts[4].y = 100.;
	pts[5].x = 200.;
	pts[5].y = 100.;
	union physics2_shape shape4 = physics2_newPoly(pts, 6, 1);
	//union physics2_shape shape4 = physics2_newIsoscelesTriangle(180., 150.);
	//union physics2_shape shape4 = physics2_newRegularPolygon(100., 2);
	physics2_addShape(pctx, shape4);
	//shape4.poly->mass = 15000.;
	physics2_setMassByArea(shape4, 1.);
	shape4.poly->ploc.x = shape4.poly->loc.x = 500.;
	shape4.poly->ploc.y = shape4.poly->loc.y = 50.;
	//shape4.poly->vel.x = -2.;
	shape4.poly->vel.x = 2.;
	shape4.poly->vel.y = 0.;
	//shape4.poly->vel.y = -3.;
	physics2_adjustCOM(shape4);
	physics2_calculateMOI(shape4);
	physics2_triangulate(shape4);
//shape2.poly->rps = .02;
	//shape2.poly->rot = 2;
	wall1 = physics2_newRect(width + 100., 100.);
	//physics2_addShape(pctx, wall1);
	wall1.poly->ploc.x = wall1.poly->loc.x = width / 2. + 100;
	wall1.poly->ploc.y = wall1.poly->loc.y = -50 + 100;
	wall2 = physics2_newRect(width, 100.);
	//physics2_addShape(pctx, wall2);
	wall2.poly->ploc.x = wall2.poly->loc.x = width / 2. + 110.;
	wall2.poly->ploc.y = wall2.poly->loc.y = height + 50. + 200;
	wall3 = physics2_newRect(100., height);
	physics2_addShape(pctx, wall3);
	physics2_setMassByArea(wall3, 10.);
	wall3.poly->ploc.x = wall3.poly->loc.x = -50. + 100;
	wall3.poly->ploc.y = wall3.poly->loc.y = height / 2. + 100;
	physics2_calculateMOI(wall3);
	wall4 = physics2_newRect(100., height);
	physics2_addShape(pctx, wall4);
	physics2_setMassByArea(wall4, 10.);
	wall4.poly->ploc.x = wall4.poly->loc.x = width + 50.;
	wall4.poly->ploc.y = wall4.poly->loc.y = height / 2. + 100;
	physics2_calculateMOI(wall4);
	guistate_register_render(0, test_render);
	guistate_register_tick(0, test_tick);
	guistate_register_text(0, test_text);
	guistate_register_keyboard(0, test_keyboard);
	guistate_register_mousemotion(0, test_mouseMotion);
	guistate_register_scroll(0, test_scroll);
}

struct timespec __main_ts;
double __main_lt;
double __main_lf;
double __main_lfms = 0.;

void displayCallback() {
	clock_gettime(CLOCK_MONOTONIC, &__main_ts);
	double cms = (double) __main_ts.tv_sec * 1000. + (double) __main_ts.tv_nsec / 1000000.;
	if (cms - __main_lfms < (1000. / frameLimit)) {
		double usd = (cms - __main_lfms);
		double ttnf = (1000. / frameLimit) - usd;
		if (ttnf > 0.) {
			usleep(ttnf * 1000.);
		}
	}
	frames++;
	glfwGetFramebufferSize(window, &width, &height);
	clock_gettime(CLOCK_MONOTONIC, &__main_ts);
	double ms2 = (double) __main_ts.tv_sec * 1000. + (double) __main_ts.tv_nsec / 1000000.;
	__main_lfms = ms2;
	while (ms2 > __main_lt + (1000. / (float) tps)) {
		__main_lt += (1000. / (float) tps);
		__gui_tick();
	}
	if (ms2 > __main_lf + 1000.) {
		double t = ms2 - __main_lf;
		__main_lf = ms2;
		//printf("FPS: %f\n", (float) frames / (t / 1000.));
		frames = 0;
	}
	float partialTick = ((ms2 - __main_lt) / 50.);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	__gui_render(partialTick);
	size_t error = glGetError();
	if (error) {
		char* es;
		if (error == GL_INVALID_ENUM) {
			es = "GL_INVALID_ENUM";
		} else if (error == GL_INVALID_VALUE) {
			es = "GL_INVALID_VALUE";
		} else if (error == GL_INVALID_OPERATION) {
			es = "GL_INVALID_OPERATION";
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION
		} else if (error == GL_INVALID_FRAMEBUFFER_OPERATION) {
			es = "GL_INVALID_FRAMEBUFFER_OPERATION";
#endif
		} else if (error == GL_STACK_OVERFLOW) {
			es = "GL_STACK_OVERFLOW";
		} else if (error == GL_STACK_UNDERFLOW) {
			es = "GL_STACK_UNDERFLOW";
		} else if (error == GL_OUT_OF_MEMORY) {
			es = "GL_OUT_OF_MEMORY";
		} else es = "UNKNOWN";
		printf("GLError: %s\n", es);
	}
	glFlush();
}

void __error_callback(int error, const char* description) {
	printf("GLFW error: %s\n", description);
}

int main(int argc, char* argv[]) {
	//#ifdef __MINGW32__
	//	WORD versionWanted = MAKEWORD(1, 1);
	//	WSADATA wsaData;
	//	WSAStartup(versionWanted, &wsaData);
	//#endif

	char ncwd[strlen(argv[0]) + 1];
	memcpy(ncwd, argv[0], strlen(argv[0]) + 1);
	char* ecwd =
#ifdef __MINGW32__
			strrchr(ncwd, '\\');
#else
			strrchr(ncwd, '/');
#endif
	if (ecwd != NULL) {
		ecwd++;
		ecwd[0] = 0;
		chdir(ncwd);
	}
	printf("Loading... [FROM=%s]\n", ncwd);
	main_preinit();
	width = 800;
	height = 600;
	if (!glfwInit()) return -1;
	glfwWindowHint(GLFW_DOUBLEBUFFER, 1);
	glfwWindowHint(GLFW_SAMPLES, 4); // antialiasing
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	//glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	//glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwSetErrorCallback(__error_callback);
	window = glfwCreateWindow(800, 600, windowTitle, NULL, NULL);
	if (!window) {
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent (window);
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		printf("GLEW Init error: %s\n", glewGetErrorString(err));
		glfwTerminate();
		return -1;
	}
	if (!glewIsSupported("GL_VERSION_3_0") || !glewIsSupported("GL_ARB_vertex_program")) {
		printf("OpenGL version 3.0+ or GL_ARB_vertex_program not satisfied.\n");
		glfwTerminate();
		return -1;
	}
	printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
	glfwSetInputMode(window, GLFW_STICKY_KEYS, 1);
	main_init();
	__gui_load();
	guistate_set(0);
	clock_gettime(CLOCK_MONOTONIC, &__main_ts);
	__main_lt = (double) __main_ts.tv_sec * 1000. + (double) __main_ts.tv_nsec / 1000000.;
	__main_lf = __main_lt;
	glfwSetKeyCallback(window, __gui_keyboardCallback);
	glfwSetCharCallback(window, __gui_textCallback);
	glfwSetCursorPosCallback(window, __gui_mouseMotionCallback);
	glfwSetMouseButtonCallback(window, __gui_mouseCallback);
	glfwSetScrollCallback(window, __gui_scrollCallback);
	printf("Loaded.\n");
	while (!glfwWindowShouldClose(window)) {
		displayCallback();
		glfwSwapBuffers(window);
		glfwSwapInterval(1);
		glfwPollEvents();
	}
	glfwTerminate();
	return 0;
}
