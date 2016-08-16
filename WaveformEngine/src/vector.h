/*
 * vector.h
 *
 *  Created on: Jul 25, 2016
 *      Author: root
 */

#ifndef VECTOR_H_
#define VECTOR_H_

typedef struct {
		float x;
		float y;
} vec2f;

typedef struct {
		float x;
		float y;
		float z;
} vec3f;

vec2f vec2f_add(vec2f v1, vec2f v2);

vec3f vec3f_add(vec3f v1, vec3f v2);

vec2f vec2f_sub(vec2f v1, vec2f v2);

vec3f vec3f_sub(vec3f v1, vec3f v2);

vec2f vec2f_scale(vec2f v1, float f);

vec3f vec3f_scale(vec3f v1, float f);

vec2f vec2f_project(vec2f v1, vec2f v2);

int vec2f_eq(vec2f v1, vec2f v2);

int vec3f_eq(vec3f v1, vec3f v2);

float vec2f_mag(vec2f v1);

float vec3f_mag(vec3f v1);

float vec2f_dot(vec2f v1, vec2f v2);

float vec3f_dot(vec3f v1, vec3f v2);

vec2f vec2f_norm(vec2f v1);

vec3f vec3f_norm(vec3f v1);

vec3f vec2f_cross(vec2f v1, vec2f v2, float z); // z can be 0., and check z for magnitude

vec3f vec3f_cross(vec3f v1, vec3f v2);

vec2f vec2f_perp(vec2f v1);

#endif /* VECTOR_H_ */
