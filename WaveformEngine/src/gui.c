/*
 * gui.c
 *
 *  Created on: Mar 5, 2016
 *      Author: root
 */
#include "gui.h"
#include <stdlib.h>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include "render.h"
#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include "render.h"
#include "globals.h"
#include "smem.h"
#include "xstring.h"

#define guistate_register(name, args) int guistate_register_##name(int index, void (*name)args) {struct __gui_reg* gr = __guistate_ensure(index);if (gr == NULL) return 1;gr->name = name;return 0;}

int __guistate;
struct __gui_reg* __guistate_cache;
struct __gui_reg** __guistate_regs;
size_t __guistate_reg_size;

struct __gui_reg {
		void (*render)(float);
		void (*tick)();
		void (*load)();
		void (*show)();
		void (*hide)();
		void (*keyboard)(int, int, int, int);
		void (*text)(unsigned int);
		void (*mousemotion)(double, double);
		void (*click)(int, int, int);
		void (*scroll)(double, double);
		void (*button)(int, int, double, double);
		struct gui_button** buttons;
		size_t button_count;
		struct gui_linebox** lineboxes;
		size_t linebox_count;
		int focused_lb;
		int lb_curpos;
};

struct __gui_reg* __guistate_find(int index) {
	if (index < 0 || __guistate_regs == NULL || index >= __guistate_reg_size) return NULL;
	for (size_t i = 0; i < __guistate_reg_size; i++) {
		if (__guistate_regs[i] != NULL) return __guistate_regs[i];
	}
	return NULL;
}

int gui_getselectedlinebox() {
	return __guistate_cache != NULL ? __guistate_cache->focused_lb : -1;
}

int gui_setselectedlinebox(int focused_linebox) {
	if (__guistate_cache == NULL || focused_linebox < 0 || focused_linebox >= __guistate_cache->focused_lb) return -1;
	__guistate_cache->lb_curpos = 0;
	__guistate_cache->focused_lb = focused_linebox;
	return 0;
}

int gui_getlineboxcursor() {
	return __guistate_cache != NULL ? __guistate_cache->lb_curpos : -1;
}

int gui_setlineboxcursor(int linebox_curpos) {
	if (__guistate_cache == NULL) return -1;
	size_t cl = __guistate_cache->lineboxes[__guistate_cache->focused_lb] == NULL ? 0 : strlen(__guistate_cache->lineboxes[__guistate_cache->focused_lb]->text);
	if (linebox_curpos < 0) linebox_curpos = 0;
	else if (linebox_curpos > cl) {
		linebox_curpos = cl;
	}
	__guistate_cache->lb_curpos = linebox_curpos;
	return 0;
}

void guistate_set(int gs) {
	if (__guistate_cache != NULL && __guistate_cache->hide != NULL) (__guistate_cache->hide)();
	__guistate = gs;
	__guistate_cache = __guistate_find(gs);
	if (__guistate_cache != NULL && __guistate_cache->show != NULL) (__guistate_cache->show)();
}

int guistate_get() {
	return __guistate;
}

struct __gui_reg* __guistate_ensure(int index) {
	if (index < 0) return NULL;
	if (__guistate_regs == NULL) {
		__guistate_regs = scalloc(sizeof(struct __gui_reg*) * (index + 1));
		__guistate_reg_size = 0;
	} else {
		if (index >= __guistate_reg_size) {
			__guistate_regs = srealloc(__guistate_regs, (index + 1) * sizeof(struct __gui_reg*));
			memset(__guistate_regs + __guistate_reg_size, 0, (__guistate_reg_size - index - 1) * sizeof(struct __gui_reg*));
		}
		for (size_t i = 0; i < __guistate_reg_size; i++) {
			if (__guistate_regs[i] != NULL) return __guistate_regs[i];
		}
	}
	struct __gui_reg* gr = smalloc(sizeof(struct __gui_reg));
	memset(gr, 0, sizeof(struct __gui_reg));
	__guistate_regs[index] = gr;
	return gr;
}

guistate_register(render, (float))
// partialTick

guistate_register(tick, ())

guistate_register(load, ())

guistate_register(show, ())

guistate_register(hide, ())

guistate_register(keyboard, (int, int, int, int))
// key, scancode, action, mods

guistate_register(text, (unsigned int))
// codepoint

guistate_register(mousemotion, (double, double))
// x, y

guistate_register(click, (int, int, int))
// button, action, mods

guistate_register(scroll, (double, double))
// x, y

guistate_register(button, (int, int, double, double))
// action, mods(from GLFW), mouse x, mouse y

void __gui_tick() {
	if (__guistate_cache != NULL && __guistate_cache->tick != NULL) (__guistate_cache->tick)();
}

void __gui_load() {
	if (__guistate_cache != NULL && __guistate_cache->load != NULL) (__guistate_cache->load)();
}

void __gui_render(float partialTick) {
	if (__guistate_cache != NULL && __guistate_cache->render != NULL) (__guistate_cache->render)(partialTick);
}

void __gui_keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (__guistate_cache != NULL) {
		if (__guistate_cache->linebox_count > 0 && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			struct gui_linebox* lb = __guistate_cache->lineboxes[__guistate_cache->focused_lb];
			if (lb != NULL) {
				if (key == GLFW_KEY_BACKSPACE) {
					size_t tl = strlen(lb->text);
					if (__guistate_cache->lb_curpos > 0) {
						memmove(lb->text + __guistate_cache->lb_curpos - 1, lb->text + __guistate_cache->lb_curpos, tl - __guistate_cache->lb_curpos + 1);
						__guistate_cache->lb_curpos--;
					}
				} else if (key == GLFW_KEY_DELETE) {
					size_t tl = strlen(lb->text);
					if (__guistate_cache->lb_curpos < tl) {
						memmove(lb->text + __guistate_cache->lb_curpos, lb->text + __guistate_cache->lb_curpos + 1, tl - __guistate_cache->lb_curpos);
					}
				} else if (key == GLFW_KEY_LEFT) {
					__guistate_cache->lb_curpos--;
					if (__guistate_cache->lb_curpos < 0) __guistate_cache->lb_curpos = 0;
				} else if (key == GLFW_KEY_RIGHT) {
					__guistate_cache->lb_curpos++;
					if (__guistate_cache->lb_curpos > strlen(lb->text)) __guistate_cache->lb_curpos--;
				} else if (key == GLFW_KEY_HOME) {
					__guistate_cache->lb_curpos = 0;
				} else if (key == GLFW_KEY_END) {
					__guistate_cache->lb_curpos = strlen(lb->text);
				} else if (key == GLFW_KEY_ENTER) {
					(__guistate_cache->button)(lb->action, mods, -1., -1.);
				} else if (key == GLFW_KEY_V && (mods & GLFW_MOD_CONTROL)) {
					size_t tl = strlen(lb->text);
					const char* ins = glfwGetClipboardString(window);
					if (ins != NULL) {
						size_t insl = strlen(ins);
						if (tl + insl < 256) {
							if (__guistate_cache->lb_curpos == tl) {
								memcpy(lb->text + __guistate_cache->lb_curpos, ins, insl);
								lb->text[tl + insl] = 0;
							} else if (__guistate_cache->lb_curpos < tl) {
								memmove(lb->text + __guistate_cache->lb_curpos + insl, lb->text + __guistate_cache->lb_curpos, tl - __guistate_cache->lb_curpos);
								memcpy(lb->text + __guistate_cache->lb_curpos, ins, insl);
								lb->text[tl + insl + 1] = 0;
							}
							__guistate_cache->lb_curpos += insl;
						}
					}
				}
			}
		}
		if (__guistate_cache->keyboard != NULL) (__guistate_cache->keyboard)(key, scancode, action, mods);
	}
}

void __gui_textCallback(GLFWwindow* window, unsigned int codepoint) {
	if (__guistate_cache != NULL && __guistate_cache->text != NULL) (__guistate_cache->text)(codepoint);
}

double __gui_mx = -1.;
double __gui_my = -1.;

void __gui_mouseMotionCallback(GLFWwindow* window, double x, double y) {
	__gui_mx = x;
	__gui_my = y;
	if (__guistate_cache != NULL && __guistate_cache->mousemotion != NULL) (__guistate_cache->mousemotion)(x, y);
}

void __gui_mouseCallback(GLFWwindow* window, int button, int action, int mods) {
	if (__guistate_cache != NULL) {
		if (action == GLFW_RELEASE && button == 0 && __guistate_cache->button != NULL) {
			for (size_t i = 0; i < __guistate_cache->button_count; i++) {
				struct gui_button* gb = __guistate_cache->buttons[i];
				if (gb != NULL && gb->x < __gui_mx && gb->y < __gui_my && gb->x + gb->width > __gui_mx && gb->y + gb->height > __gui_my) {
					(__guistate_cache->button)(gb->action, mods, __gui_mx, __gui_my);
				}
			}
			for (size_t i = 0; i < __guistate_cache->linebox_count; i++) {
				struct gui_linebox* lb = __guistate_cache->lineboxes[i];
				if (lb != NULL && lb->x < __gui_mx && lb->y < __gui_my && lb->x + lb->width > __gui_mx && lb->y + lb->height > __gui_my) {
					__guistate_cache->focused_lb = i;
				}
			}
		}
		if (__guistate_cache->click != NULL) (__guistate_cache->click)(button, action, mods);
	}
}

void __gui_scrollCallback(GLFWwindow* window, double x, double y) {
	if (__guistate_cache != NULL && __guistate_cache->scroll != NULL) (__guistate_cache->scroll)(x, y);
}

void gui_claimMouse() {
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void gui_unclaimMouse() {
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

int gui_addbutton(int guistate, struct gui_button button) {
	struct __gui_reg* gr = __guistate_find(guistate);
	if (gr == NULL) return -1;
	if (gr->buttons == NULL) {
		gr->buttons = smalloc(sizeof(struct gui_button*));
		gr->button_count = 0;
	} else gr->buttons = srealloc(gr->buttons, (gr->button_count + 1) * sizeof(struct gui_button*));
	struct gui_button* dup = smalloc(sizeof(struct gui_button));
	memcpy(dup, &button, sizeof(struct gui_button));
	gr->buttons[gr->button_count] = dup;
	return gr->button_count++;
}

int gui_addlinebox(int guistate, struct gui_linebox linebox) {
	struct __gui_reg* gr = __guistate_find(guistate);
	if (gr == NULL) return -1;
	if (gr->lineboxes == NULL) {
		gr->lineboxes = smalloc(sizeof(struct gui_linebox*));
		gr->linebox_count = 0;
	} else gr->lineboxes = srealloc(gr->lineboxes, (gr->linebox_count + 1) * sizeof(struct gui_linebox*));
	struct gui_linebox* dup = smalloc(sizeof(struct gui_linebox));
	memcpy(dup, &linebox, sizeof(struct gui_linebox));
	gr->lineboxes[gr->linebox_count] = dup;
	return gr->linebox_count++;
}

int gui_rembutton(int guistate, int index) {
	struct __gui_reg* gr = __guistate_find(guistate);
	if (gr == NULL || index < 0 || index >= gr->button_count) return -1;
	free(gr->buttons[index]);
	gr->buttons[index] = NULL;
	return 0;
}

int gui_remlinebox(int guistate, int index) { //does not free the char* in the textbox if it is in the heap!
	struct __gui_reg* gr = __guistate_find(guistate);
	if (gr == NULL || index < 0 || index >= gr->linebox_count) return -1;
	free(gr->lineboxes[index]);
	gr->lineboxes[index] = NULL;
	return 0;
}

