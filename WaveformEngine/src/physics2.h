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

union physics2_shape {
		struct physics2_circle* circle;
		struct physics2_poly* poly;
		struct physics2_compound* compound;
};

struct physics2_ctx {
		union physics2_shape* shapes;
		size_t shape_count;
		vec2f constant_accel;
		int (*canCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape);
		// for these 3 callbacks, the first 2 shapes are the shapes including compound ones, the second 2 are possibly subshapes and always circles or polygons.
		void (*sensorCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape, union physics2_shape, union physics2_shape); // called when two objects are colliding but canCollide is 0
		void (*preCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape, vec2f, union physics2_shape, union physics2_shape);
		void (*postCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape, vec2f, union physics2_shape, union physics2_shape);
};

void physics2_setCanCollideCallback(struct physics2_ctx* ctx, int (*canCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape));

void physics2_setSensorCollideCallback(struct physics2_ctx* ctx, void (*sensorCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape, union physics2_shape, union physics2_shape));

void physics2_setPreCollideCallback(struct physics2_ctx* ctx, void (*preCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape, vec2f, union physics2_shape, union physics2_shape));

void physics2_setPostCollideCallback(struct physics2_ctx* ctx, void (*postCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape, vec2f, union physics2_shape, union physics2_shape));

#define PHYSICS2_POLY 0
#define PHYSICS2_CIRCLE 1
#define PHYSICS2_COMPOUND 2

union physics2_shape physics2_newRect(float width, float height);

union physics2_shape physics2_newRightTriangle(float width, float height);

union physics2_shape physics2_newEquilateralTriangle(float width);

union physics2_shape physics2_newIsoscelesTriangle(float width, float height);

union physics2_shape physics2_newRegularPolygon(float radius, int n);

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
		void* data;
		union physics2_shape parent;
		/*vec2f p1;
		 vec2f p2;
		 vec2f p3;*/
};

union physics2_shape physics2_newCircle(float radius);

struct __physics2_triangle {
		vec2f v1;
		vec2f v2;
		vec2f v3;
};

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
		void* data;
		union physics2_shape parent;
		/*vec2f p1;
		 vec2f p2;
		 vec2f p3;*/
		vec2f* points;
		size_t point_count;
		uint8_t concave;
		struct __physics2_triangle* triangles;
		size_t triangle_count;
};

struct physics2_compound {
		uint8_t type;
		vec2f loc;
		vec2f ploc;
		float rot;
		float lrot;
		float rps; // radians per second
		vec2f vel;
		vec2f constant_accel;
		float mass; // mass is assumed universally distributed regardless of subshape
		float moi;
		float radius;
		size_t index;
		float friction; // ignored for compound, look at subshape individually
		float elasticity; // ignored for compound, look at subshape individually
		float drag; // ignored for compound, look at subshape individually
		float rdrag; // ignored for compound, look at subshape individually
		void* data;
		union physics2_shape parent;
		/*vec2f p1;
		 vec2f p2;
		 vec2f p3;*/
		union physics2_shape* subshapes;
		size_t subshape_count;
};

void physics2_triangulate(struct physics2_poly* poly);

union physics2_shape physics2_newPoly(vec2f* points, size_t point_count, uint8_t concave);

void physics2_finalizeShape(union physics2_shape shape, float massMultiplier);

union physics2_shape physics2_newCompound(union physics2_shape* shapes, size_t shape_count);

void physics2_delShape(struct physics2_ctx* ctx, union physics2_shape shape);

void physics2_addShape(struct physics2_ctx* ctx, union physics2_shape shape);

struct physics2_ctx* physics2_init();

void physics2_free(struct physics2_ctx* ctx);

void physics2_drawShape(union physics2_shape shape, float partialTick); // glBegin()->glEnd(), not VAO

void physics2_drawAllShapes(struct physics2_ctx* ctx, float x1, float y1, float x2, float y2, float partialTick); // draws all shapes partially or entirely within the bounds. not vao

void physics2_setMassByArea(union physics2_shape shape, float multiplier);

void physics2_adjustCOM(union physics2_shape shape);

void physics2_applyForce(union physics2_shape shape, vec2f force, vec2f loc);

void physics2_teleport(union physics2_shape shape, vec2f loc);

void physics2_teleportRot(union physics2_shape shape, float rot);

vec2f physics2_getInterpolatedPosition(union physics2_shape shape, float partialTick);

float physics2_getInterpolatedRotation(union physics2_shape shape, float partialTick);

void physics2_simulate(struct physics2_ctx* ctx);

void physics2_calculateMOI(union physics2_shape shape);

#endif /* PHYSICS2_H_ */
