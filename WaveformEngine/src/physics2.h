/*
 * physics2.h
 *
 *  Created on: Jul 31, 2016
 *      Author: root
 */

#ifndef PHYSICS2_H_
#define PHYSICS2_H_

#include "vector.h"
#include "smem.h"
#include <stdint.h>

#define PHYSICS2_CIRCLEPREC 20

struct physics2_ctx {
		union physics2_shape* shapes;
		size_t shape_count;
		vec2f constant_accel;
};

#define PHYSICS2_RECT 0
#define PHYSICS2_CIRCLE 1
#define PHYSICS2_POLY 2

struct physics2_rect {
		uint8_t type;
		vec2f loc;
		vec2f ploc;
		float rot;
		float lrot;
		float rps; // radians per second
		vec2f vel;
		vec2f constant_accel;
		float mass;
		float moi;
		float radius;
		size_t index;
		float friction; // 1 = instantly stop, 0 = no friction
		float elasticity;
		float drag; // 1 = never move 0 = infinite terminal velocity
		float rdrag;
		vec2f p1;
		vec2f p2;
		vec2f p3;
		float width;
		float height;
};

union physics2_shape physics2_newRect(float width, float height);

struct physics2_circle {
		uint8_t type;
		vec2f loc;
		vec2f ploc;
		float rot;
		float lrot;
		float rps; // radians per second
		vec2f vel;
		vec2f constant_accel;
		float mass;
		float moi;
		float radius;
		size_t index;
		float friction; // 1 = instantly stop, 0 = no friction
		float elasticity;
		float drag; // 1 = never move 0 = infinite terminal velocity
		float rdrag;
		vec2f p1;
		vec2f p2;
		vec2f p3;
};

union physics2_shape physics2_newCircle(float radius);

struct physics2_poly {
		uint8_t type;
		vec2f loc;
		vec2f ploc;
		float rot;
		float lrot;
		float rps; // radians per second
		vec2f vel;
		vec2f constant_accel;
		float mass;
		float moi;
		float radius;
		size_t index;
		float friction; // 1 = instantly stop, 0 = no friction
		float elasticity;
		float drag; // 1 = never move 0 = infinite terminal velocity
		float rdrag;
		vec2f p1;
		vec2f p2;
		vec2f p3;
		vec2f* points;
		size_t point_count;
		unsigned int calllist;
};

union physics2_shape physics2_newPoly(vec2f* points, size_t point_count);

union physics2_shape {
		struct physics2_rect* rect;
		struct physics2_circle* circle;
		struct physics2_poly* poly;
};

void physics2_delShape(struct physics2_ctx* ctx, union physics2_shape shape);

void physics2_addShape(struct physics2_ctx* ctx, union physics2_shape shape);

struct physics2_ctx* physics2_init();

void physics2_free(struct physics2_ctx* ctx);

void physics2_drawShape(union physics2_shape shape, float partialTick); // glBegin()->glEnd(), not VAO

void physics2_drawAllShapes(struct physics2_ctx* ctx, float x1, float y1, float x2, float y2, float partialTick); // draws all shapes partially or entirely within the bounds. not vao

void physics2_setMassByArea(union physics2_shape shape, float multiplier);

void physics2_applyForce(union physics2_shape shape, vec2f force);

vec2f physics2_getInterpolatedPosition(union physics2_shape shape, float partialTick);

float physics2_getInterpolatedRotation(union physics2_shape shape, float partialTick);

void physics2_simulate(struct physics2_ctx* ctx);

void physics2_calculateMOI(union physics2_shape shape);

#endif /* PHYSICS2_H_ */
