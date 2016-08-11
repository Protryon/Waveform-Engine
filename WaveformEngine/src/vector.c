/*
 * vector.c
 *
 *  Created on: Jul 25, 2016
 *      Author: root
 */

#include "vector.h"
#include <math.h>

vec2f vec2f_add(vec2f v1, vec2f v2) {
	vec2f v3;
	v3.x = v1.x + v2.x;
	v3.y = v1.y + v2.y;
	return v3;
}

vec3f vec3f_add(vec3f v1, vec3f v2) {
	vec3f v3;
	v3.x = v1.x + v2.x;
	v3.y = v1.y + v2.y;
	v3.z = v1.z + v2.z;
	return v3;
}

vec2f vec2f_sub(vec2f v1, vec2f v2) {
	vec2f v3;
	v3.x = v1.x - v2.x;
	v3.y = v1.y - v2.y;
	return v3;
}

vec3f vec3f_sub(vec3f v1, vec3f v2) {
	vec3f v3;
	v3.x = v1.x - v2.x;
	v3.y = v1.y - v2.y;
	v3.z = v1.z - v2.z;
	return v3;
}

vec2f vec2f_scale(vec2f v1, float f) {
	vec2f v3;
	v3.x = v1.x * f;
	v3.y = v1.y * f;
	return v3;
}

vec3f vec3f_scale(vec3f v1, float f) {
	vec3f v3;
	v3.x = v1.x * f;
	v3.y = v1.y * f;
	v3.z = v1.z * f;
	return v3;
}

float vec2f_mag(vec2f v1) {
	return sqrt(v1.x * v1.x + v1.y * v1.y);
}

float vec3f_mag(vec3f v1) {
	return sqrt(v1.x * v1.x + v1.y * v1.y + v1.z * v1.z);
}

float vec2f_dot(vec2f v1, vec2f v2) {
	return v1.x * v2.x + v1.y * v2.y;
}

float vec3f_dot(vec3f v1, vec3f v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

vec2f vec2f_norm(vec2f v1) {
	float v1mag = vec2f_mag(v1);
	vec2f v3;
	v3.x = v1.x / (v1mag == 0. ? 1. : v1mag);
	v3.y = v1.y / (v1mag == 0. ? 1. : v1mag);
	return v3;
}

vec2f vec2f_project(vec2f v1, vec2f v2) {
	return vec2f_scale(v2, vec2f_dot(v1, v2) / vec2f_dot(v2, v2));
}

vec3f vec3f_norm(vec3f v1) {
	float v1mag = vec3f_mag(v1);
	vec3f v3;
	v3.x = v1.x / (v1mag == 0. ? 1. : v1mag);
	v3.y = v1.y / (v1mag == 0. ? 1. : v1mag);
	v3.z = v1.z / (v1mag == 0. ? 1. : v1mag);
	return v3;
}

vec3f vec2f_cross(vec2f v1, vec2f v2, float z) { // z can be 0, and check z for magnitude
	vec3f v3;
	v3.x = v1.y * z - z * v2.y;
	v3.y = z * v2.x - v1.x * z;
	v3.z = v1.x * v2.y - v1.y * v2.x;
	return v3;
}

vec3f vec3f_cross(vec3f v1, vec3f v2) {
	vec3f v3;
	v3.x = v1.y * v2.z - v1.z * v2.y;
	v3.y = v1.z * v2.x - v1.x * v2.z;
	v3.z = v1.x * v2.y - v1.y * v2.x;
	return v3;
}

vec2f vec2f_perp(vec2f v1) {
	vec2f v2;
	v2.x = -v1.y;
	v2.y = v1.x;
	return v2;
}
