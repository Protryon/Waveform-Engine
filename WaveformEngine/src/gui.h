/*
 * gui.h
 *
 *  Created on: Mar 5, 2016
 *      Author: root
 */

#ifndef GUI_H_
#define GUI_H_

#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

struct gui_button { // in window coordinates, not opengl coordinates!
		int x;
		int y;
		int width;
		int height;
		int action; // -1 sends no event
};

struct gui_linebox { // in window coordinates, not opengl coordinates!
		int x;
		int y;
		int width;
		int height;
		char* text;
		size_t text_size;
		int action; // same namespace for buttons, sent for pressing enter, -1 disables
};

int gui_getselectedlinebox();

int gui_setselectedlinebox(int focused_linebox);

void guistate_set(int gs);

int guistate_get();

void __gui_tick();

void __gui_load();

void __gui_render(float partialTick);

void __gui_keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

void __gui_textCallback(GLFWwindow* window, unsigned int codepoint);

void __gui_mouseMotionCallback(GLFWwindow* window, double x, double y);

void __gui_mouseCallback(GLFWwindow* window, int button, int action, int mods);

void __gui_scrollCallback(GLFWwindow* window, double x, double y);

void gui_claimMouse();

void gui_unclaimMouse();

int gui_addbutton(int guistate, struct gui_button button);

int gui_addlinebox(int guistate, struct gui_linebox linebox);

int gui_rembutton(int guistate, int index);

int gui_remlinebox(int guistate, int index);

#endif /* GUI_H_ */
