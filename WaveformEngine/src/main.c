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

void main_preinit() {
	windowTitle = "Waveform Engine";
	frameLimit = 60;
}

void main_init() {
	//glEnable (GL_MULTISAMPLE);
	//glEnable (GL_MULTISAMPLE_ARB);
	//glEnable (GL_DEPTH_TEST);
	//glEnable (GL_TEXTURE_2D);
	//glAlphaFunc(GL_GREATER, 0.1);
	//glEnable (GL_ALPHA_TEST);
	glEnable (GL_CULL_FACE);
	//load guis here via gui_register_(event)
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
		printf("FPS: %f\n", (float) frames / (t / 1000.));
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
	//glfwWindowHint(GLFW_SAMPLES, 4); // antialiasing
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
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
	if (!glewIsSupported("GL_VERSION_2_1") || !glewIsSupported("GL_ARB_vertex_program")) {
		printf("OpenGL version 2.1+ or GL_ARB_vertex_program not satisfied.\n");
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
