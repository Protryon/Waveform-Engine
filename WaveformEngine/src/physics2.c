/*
 * physics2.c
 *
 *  Created on: Jul 31, 2016
 *      Author: root
 */

#include "vector.h"
#include "physics2.h"
#include "smem.h"
#include <math.h>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <stdint.h>
#include "xstring.h"
#include <stdio.h>

void __physics2_initShape(union physics2_shape shape) {
	shape.rect->loc.x = 0.;
	shape.rect->loc.y = 0.;
	shape.rect->ploc = shape.rect->loc;
	shape.rect->rot = 0.;
	shape.rect->lrot = 0.;
	shape.rect->rps = 0.;
	shape.rect->vel.x = 0.;
	shape.rect->vel.y = 0.;
	shape.rect->constant_accel.x = 0.;
	shape.rect->constant_accel.y = 0.;
	shape.rect->mass = 1.;
	shape.rect->moi = 0.;
	shape.rect->friction = .5;
	shape.rect->drag = .001;
	shape.rect->rdrag = .001;
	shape.rect->elasticity = 1.;
}

union physics2_shape physics2_newRect(float width, float height) {
	struct physics2_rect* rect = smalloc(sizeof(struct physics2_rect));
	rect->type = PHYSICS2_RECT;
	rect->index = -1;
	union physics2_shape shape;
	shape.rect = rect;
	__physics2_initShape(shape);
	rect->width = width;
	rect->height = height;
	float x = width / 2.;
	float y = height / 2.;
	rect->radius = sqrt(x * x + y * y);
	return shape;
}

union physics2_shape physics2_newCircle(float radius) {
	struct physics2_circle* circle = smalloc(sizeof(struct physics2_circle));
	circle->type = PHYSICS2_CIRCLE;
	circle->index = -1;
	union physics2_shape shape;
	shape.circle = circle;
	__physics2_initShape(shape);
	circle->radius = radius;
	return shape;
}

union physics2_shape physics2_newPoly(vec2f* points, size_t point_count) {
	if (point_count == 0) {
		union physics2_shape shape;
		shape.rect = NULL;
		return shape;
	}
	struct physics2_poly* rect = smalloc(sizeof(struct physics2_poly));
	rect->type = PHYSICS2_POLY;
	rect->index = -1;
	union physics2_shape shape;
	shape.poly = rect;
	__physics2_initShape(shape);
	rect->points = smalloc(point_count * sizeof(vec2f));
	memcpy(rect->points, points, point_count * sizeof(vec2f));
	rect->point_count = point_count;
	rect->radius = 0.;
	for (size_t i = 0; i < rect->point_count; i++) {
		float len = sqrt(rect->points[i].x * rect->points[i].x + rect->points[i].y * rect->points[i].y);
		if (len > rect->radius) rect->radius = len;
	}
	rect->calllist = 0;
	return shape;
}

void physics2_delShape(struct physics2_ctx* ctx, union physics2_shape shape) {
	if (ctx->shapes[shape.rect->index].rect->type == PHYSICS2_POLY) {
		if (ctx->shapes[shape.rect->index].poly->calllist > 0) glDeleteLists(ctx->shapes[shape.rect->index].poly->calllist, 1);
		free(ctx->shapes[shape.rect->index].poly->points);
	}
	ctx->shapes[shape.rect->index].rect = NULL;
	free(shape.rect);
}

void physics2_addShape(struct physics2_ctx* ctx, union physics2_shape shape) {
	if (ctx->shapes == NULL) {
		ctx->shapes = smalloc(sizeof(union physics2_shape));
		ctx->shape_count = 0;
	} else ctx->shapes = srealloc(ctx->shapes, sizeof(union physics2_shape) * (ctx->shape_count + 1));
	shape.rect->index = ctx->shape_count;
	ctx->shapes[ctx->shape_count++] = shape;
}

struct physics2_ctx* physics2_init() {
	struct physics2_ctx* ctx = smalloc(sizeof(struct physics2_ctx));
	ctx->constant_accel.x = 0.;
	ctx->constant_accel.y = 0.;
	ctx->shape_count = 0;
	ctx->shapes = NULL;
	return ctx;
}

void physics2_free(struct physics2_ctx* ctx) {
	for (size_t i = 0; i < ctx->shape_count; i++) {
		if (ctx->shapes[i].rect != NULL) physics2_delShape(ctx, ctx->shapes[i]);
	}
	if (ctx->shapes != NULL) free(ctx->shapes);
	free(ctx);
}

void __physics2_tesserror(GLenum errno) {
	printf("Physics2 error: %u\n", errno);
}

vec3f __physics2_tvecs[256];
size_t __physics2_tveci = 0;

void __physics2_tesscombine(GLdouble coords[3], void *vertex_data[4], GLfloat weight[4], void **outData) {
	printf("combine to %f, %f, %f\n", coords[0], coords[1], coords[2]);
	__physics2_tvecs[__physics2_tveci].x = coords[0];
	__physics2_tvecs[__physics2_tveci].y = coords[1];
	__physics2_tvecs[__physics2_tveci].z = coords[2];
	*outData = &__physics2_tvecs[__physics2_tveci++];
}

void proxy_glBegin(GLenum glb) {
	glBegin(glb);
	printf("glbegin %u\n", glb);
}

void proxy_glEnd() {
	glEnd();
	printf("glend\n");
}

void proxy_glVertex2fv(const GLfloat* v) {
	printf("vertex: %f, %f\n", v[0], v[1]);
	glVertex2fv(v);
}

void physics2_drawShape(union physics2_shape shape, float partialTick) {
	vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
	glPushMatrix();
	glTranslatef(iloc.x, iloc.y, 0.);
	glRotatef(360. * physics2_getInterpolatedRotation(shape, partialTick) / (M_PI * 2.), 0., 0., 1.);
	if (shape.rect->type == PHYSICS2_RECT) {
		glBegin (GL_QUADS);
		glVertex2f(-shape.rect->width / 2., -shape.rect->height / 2.);
		glVertex2f(-shape.rect->width / 2., shape.rect->height / 2.);
		glVertex2f(shape.rect->width / 2., shape.rect->height / 2.);
		glVertex2f(shape.rect->width / 2., -shape.rect->height / 2.);
		glEnd();
	} else if (shape.rect->type == PHYSICS2_CIRCLE) {
		glBegin (GL_TRIANGLE_FAN);
		glVertex2f(0., 0.);
		for (int i = PHYSICS2_CIRCLEPREC; i >= 0; i--) {
			glVertex2f((shape.rect->radius * cos((float) i * 2. * M_PI / 20.)), (shape.rect->radius * sin((float) i * 2. * M_PI / 20.)));
		}
		glEnd();
	} else if (shape.rect->type == PHYSICS2_POLY) {
		glDisable (GL_CULL_FACE);
		if (shape.poly->calllist == 0) {
			//printf("polycall\n");
			shape.poly->calllist = glGenLists(1);
			glNewList(shape.poly->calllist, GL_COMPILE_AND_EXECUTE);
			__physics2_tveci = 0;
			double* v3 = smalloc(sizeof(double) * shape.poly->point_count * 3);
			GLUtesselator* tess = gluNewTess();
			gluTessCallback(tess, GLU_TESS_BEGIN, proxy_glBegin);
			gluTessCallback(tess, GLU_TESS_END, proxy_glEnd);
			gluTessCallback(tess, GLU_TESS_VERTEX, proxy_glVertex2fv);
			gluTessCallback(tess, GLU_TESS_COMBINE, __physics2_tesscombine);
			gluTessCallback(tess, GLU_TESS_ERROR, __physics2_tesserror);
			gluTessBeginPolygon(tess, shape.poly->points);
			gluTessBeginContour(tess);
			for (size_t i = 0; i < shape.poly->point_count; i++) {
				v3[i * 3] = shape.poly->points[i].x;
				v3[i * 3 + 1] = shape.poly->points[i].y;
				v3[i * 3 + 2] = 0.;
				gluTessVertex(tess, &v3[i * 3], &shape.poly->points[i]);
			}
			gluTessEndContour(tess);
			gluTessEndPolygon(tess);
			gluDeleteTess(tess);
			free(v3);
			glEndList();
		} else {
			//printf("polycall-calllist %u\n", shape.poly->calllist);
			glCallList(shape.poly->calllist);
		}
		glEnable(GL_CULL_FACE);
	}
	glPopMatrix();
}

void __physics2_drawDebug(union physics2_shape shape, float partialTick) {
	glColor4f(0., 1., 1., 1.);
	glBegin (GL_LINES);
	glVertex2f(0., 0.);
	glVertex2f(shape.rect->p1.x, shape.rect->p1.y);
	glColor4f(1., 0., 1., 1.);
	glVertex2f(0., 0.);
	glVertex2f(shape.rect->p2.x, shape.rect->p2.y);
	glVertex2f(0., 0.);
	glVertex2f(shape.rect->p3.x, shape.rect->p3.y);
	glEnd();
	/*vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
	 glPushMatrix();
	 glTranslatef(iloc.x, iloc.y, 0.);
	 glRotatef(360. * physics2_getInterpolatedRotation(shape, partialTick) / (M_PI * 2.), 0., 0., 1.);
	 vec2f vecs[4];
	 __physics2_fillAxis(shape, vecs, 4);
	 glBegin(GL_LINES);
	 for (int i = 0; i < 4; i++) {
	 glVertex2f(0., 0.);
	 printf("%i: %f, %f\n", i, vecs[i].x, vecs[i].y);
	 glVertex2f(vecs[i].x * 100., vecs[i].y * 100);
	 }
	 glEnd();
	 glPopMatrix();
	 glColor4f(1., 0., 0., 1.);
	 */
}

void physics2_drawAllShapes(struct physics2_ctx* ctx, float x1, float y1, float x2, float y2, float partialTick) {
	for (size_t i = 0; i < ctx->shape_count; i++) {
		union physics2_shape shape = ctx->shapes[i];
		vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
		//printf("%f, %f, %f, %f, %f, %f, %f\n", iloc.x, iloc.y, x1, y1, x2, y2, shape.rect->radius);
		if (iloc.x - x1 > -shape.rect->radius && iloc.x - x2 < shape.rect->radius && iloc.y - y1 > -shape.rect->radius && iloc.y - y2 < shape.rect->radius) {
			physics2_drawShape(ctx->shapes[i], partialTick);
		}
	}
	for (size_t i = 0; i < ctx->shape_count; i++) {
		union physics2_shape shape = ctx->shapes[i];
		vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
		//printf("%f, %f, %f, %f, %f, %f, %f\n", iloc.x, iloc.y, x1, y1, x2, y2, shape.rect->radius);
		if (iloc.x - x1 > -shape.rect->radius && iloc.x - x2 < shape.rect->radius && iloc.y - y1 > -shape.rect->radius && iloc.y - y2 < shape.rect->radius) {
			//printf("dun\n");
			__physics2_drawDebug(ctx->shapes[i], partialTick);
		}
	}
}

void physics2_setMassByArea(union physics2_shape shape, float multiplier) {
	float area = 0.;
	if (shape.rect->type == PHYSICS2_RECT) {
		area = shape.rect->width * shape.rect->height;
	} else if (shape.rect->type == PHYSICS2_CIRCLE) {
		area = M_PI * shape.circle->radius * shape.circle->radius;
	} else if (shape.rect->type == PHYSICS2_POLY) {
		for (size_t i = 0; i < shape.poly->point_count; i++) {
			size_t i2 = (i + 1) == shape.poly->point_count ? 0 : i + 1;
			area += shape.poly->points[i].x * shape.poly->points[i2].y - shape.poly->points[i].y * shape.poly->points[i2].x;
		}
		area /= 2.;
	}
	area *= multiplier;
	shape.rect->mass = area;
}

void physics2_applyForce(union physics2_shape shape, vec2f force) {
	shape.rect->vel.x += force.x / shape.rect->mass;
	shape.rect->vel.y += force.y / shape.rect->mass;
}

vec2f physics2_getInterpolatedPosition(union physics2_shape shape, float partialTick) {
	vec2f v;
	v.x = shape.rect->loc.x * (1. - partialTick) + shape.rect->ploc.x * partialTick;
	v.y = shape.rect->loc.y * (1. - partialTick) + shape.rect->ploc.y * partialTick;
	return v;
}

float physics2_getInterpolatedRotation(union physics2_shape shape, float partialTick) {
	return shape.rect->rot * (1. - partialTick) + shape.rect->lrot * partialTick;
}

void __physics2_fillAxis(union physics2_shape shape, union physics2_shape shape2, vec2f* axes, size_t axes_count) {
	if (shape.rect->type == PHYSICS2_RECT) {
		axes[0].x = -shape.rect->width / 2.;
		axes[0].y = -shape.rect->height / 2.;
		axes[1].x = -shape.rect->width / 2.;
		axes[1].y = shape.rect->height / 2.;
		axes[2].x = shape.rect->width / 2.;
		axes[2].y = shape.rect->height / 2.;
		axes[3].x = shape.rect->width / 2.;
		axes[3].y = -shape.rect->height / 2.;
	} else if (shape.rect->type == PHYSICS2_CIRCLE) {
		if (shape2.rect->type == PHYSICS2_CIRCLE) {
			axes[0] = vec2f_norm(vec2f_sub(shape.circle->loc, shape2.circle->loc));
		} else if (shape2.rect->type == PHYSICS2_RECT) {
			float c = cosf(shape2.rect->rot);
			float s = sinf(shape2.rect->rot);
			vec2f best;
			float bd = 0.;
			vec2f t;
			t.x = -shape2.rect->width / 2.;
			t.y = -shape2.rect->height / 2.;
			float nx = t.x * c - t.y * s;
			t.y = t.y * c + t.x * s;
			t.x = nx;
			best = vec2f_sub(shape.circle->loc, vec2f_add(shape2.rect->loc, t));
			bd = best.x * best.x + best.y * best.y;
			t.x = -shape2.rect->width / 2.;
			t.y = shape2.rect->height / 2.;
			nx = t.x * c - t.y * s;
			t.y = t.y * c + t.x * s;
			t.x = nx;
			vec2f t2 = vec2f_sub(shape.circle->loc, vec2f_add(shape2.rect->loc, t));
			c = t2.x * t2.x + t2.y * t2.y;
			if (c < bd) {
				bd = c;
				best = t2;
			}
			t.x = shape2.rect->width / 2.;
			t.y = shape2.rect->height / 2.;
			nx = t.x * c - t.y * s;
			t.y = t.y * c + t.x * s;
			t.x = nx;
			t2 = vec2f_sub(shape.circle->loc, vec2f_add(shape2.rect->loc, t));
			c = t2.x * t2.x + t2.y * t2.y;
			if (c < bd) {
				bd = c;
				best = t2;
			}
			t.x = shape2.rect->width / 2.;
			t.y = -shape2.rect->height / 2.;
			nx = t.x * c - t.y * s;
			t.y = t.y * c + t.x * s;
			t.x = nx;
			t2 = vec2f_sub(shape.circle->loc, vec2f_add(shape2.rect->loc, t));
			c = t2.x * t2.x + t2.y * t2.y;
			if (c < bd) {
				bd = c;
				best = t2;
			}
			axes[0] = vec2f_norm(best);
		} else if (shape2.rect->type == PHYSICS2_POLY) {
			float c = cosf(shape2.rect->rot);
			float s = sinf(shape2.rect->rot);
			vec2f best;
			float bd = 0.;
			vec2f t;
			for (size_t i = 0; i < shape2.poly->point_count; i++) {
				if (i == 0) {
					t = shape2.poly->points[i];
					float nx = t.x * c - t.y * s;
					t.y = t.y * c + t.x * s;
					t.x = nx;
					best = vec2f_sub(shape.circle->loc, vec2f_add(shape2.rect->loc, t));
					bd = best.x * best.x + best.y * best.y;
				} else {
					t = shape2.poly->points[i];
					float nx = t.x * c - t.y * s;
					t.y = t.y * c + t.x * s;
					t.x = nx;
					t = vec2f_sub(shape.circle->loc, vec2f_add(shape2.poly->loc, t));
					c = t.x * t.x + t.y * t.y;
					if (c < bd) {
						bd = c;
						best = t;
					}
				}
			}
			axes[0] = vec2f_norm(vec2f_perp(best));
		}
		return;
	} else if (shape.rect->type == PHYSICS2_POLY) {
		memcpy(axes, shape.poly->points, shape.poly->point_count * sizeof(vec2f));
	}
	float c = cosf(shape.rect->rot);
	float s = sinf(shape.rect->rot);
	for (size_t j = 0; j < axes_count; j++) {
		float nx = axes[j].x * c - axes[j].y * s;
		axes[j].y = axes[j].y * c + axes[j].x * s;
		axes[j].x = nx;
	}
	vec2f t0 = axes[0];
	for (size_t j = 0; j < axes_count; j++) {
		axes[j] = vec2f_norm(vec2f_perp(vec2f_sub(axes[j], (j + 1) == axes_count ? t0 : axes[j + 1])));
	}
}

vec2f __physics2_project(vec2f axis, union physics2_shape shape, vec2f* pmin, vec2f* pmax) { // x = min, y = max
	vec2f r;
	r.x = 0.;
	r.y = 0.;
	float c = cosf(shape.rect->rot);
	float s = sinf(shape.rect->rot);
	if (shape.rect->type == PHYSICS2_RECT) {
		vec2f t;
		t.x = -shape.rect->width / 2.;
		t.y = -shape.rect->height / 2.;
		float nx = t.x * c - t.y * s;
		t.y = t.y * c + t.x * s;
		t.x = nx;
		t.x += shape.rect->loc.x;
		t.y += shape.rect->loc.y;
		float d = vec2f_dot(axis, t);
		r.x = d;
		pmin->x = t.x;
		pmin->y = t.y;
		pmax->x = t.x;
		pmax->y = t.y;
		r.y = d;
		t.x = -shape.rect->width / 2.;
		t.y = shape.rect->height / 2.;
		nx = t.x * c - t.y * s;
		t.y = t.y * c + t.x * s;
		t.x = nx;
		t.x += shape.rect->loc.x;
		t.y += shape.rect->loc.y;
		d = vec2f_dot(axis, t);
		if (d < r.x) {
			r.x = d;
			pmin->x = t.x;
			pmin->y = t.y;
		} else if (d > r.y) {
			r.y = d;
			pmax->x = t.x;
			pmax->y = t.y;
		}
		t.x = shape.rect->width / 2.;
		t.y = shape.rect->height / 2.;
		nx = t.x * c - t.y * s;
		t.y = t.y * c + t.x * s;
		t.x = nx;
		t.x += shape.rect->loc.x;
		t.y += shape.rect->loc.y;
		d = vec2f_dot(axis, t);
		if (d < r.x) {
			r.x = d;
			pmin->x = t.x;
			pmin->y = t.y;
		} else if (d > r.y) {
			r.y = d;
			pmax->x = t.x;
			pmax->y = t.y;
		}
		t.x = shape.rect->width / 2.;
		t.y = -shape.rect->height / 2.;
		nx = t.x * c - t.y * s;
		t.y = t.y * c + t.x * s;
		t.x = nx;
		t.x += shape.rect->loc.x;
		t.y += shape.rect->loc.y;
		d = vec2f_dot(axis, t);
		if (d < r.x) {
			r.x = d;
			pmin->x = t.x;
			pmin->y = t.y;
		} else if (d > r.y) {
			r.y = d;
			pmax->x = t.x;
			pmax->y = t.y;
		}
	} else if (shape.rect->type == PHYSICS2_CIRCLE) {
		vec2f p1 = vec2f_add(shape.circle->loc, vec2f_scale(axis, shape.circle->radius));
		float d1 = vec2f_dot(axis, p1);
		vec2f p2 = vec2f_add(shape.circle->loc, vec2f_scale(axis, -shape.circle->radius));
		float d2 = vec2f_dot(axis, p2);
		if (d1 < d2) {
			r.x = d1;
			r.y = d2;
			pmin->x = p1.x;
			pmin->y = p1.y;
			pmax->x = p2.x;
			pmax->y = p2.y;
		} else {
			r.x = d2;
			r.y = d1;
			pmax->x = p1.x;
			pmax->y = p1.y;
			pmin->x = p2.x;
			pmin->y = p2.y;
		}
	} else if (shape.rect->type == PHYSICS2_POLY) {
		vec2f rp;
		float d;
		for (size_t i = 0; i < shape.poly->point_count; i++) {
			float nx = shape.poly->points[i].x * c - shape.poly->points[i].y * s;
			rp.y = shape.poly->points[i].y * c + shape.poly->points[i].x * s;
			rp.x = nx;
			rp.x += shape.poly->loc.x;
			rp.y += shape.poly->loc.y;
			d = vec2f_dot(axis, rp);
			if (i == 0) {
				r.x = d;
				r.y = d;
				pmin->x = rp.x;
				pmin->y = rp.y;
				pmax->x = rp.x;
				pmax->y = rp.y;
			} else if (d < r.x) {
				r.x = d;
				pmin->x = rp.x;
				pmin->y = rp.y;
			} else if (d > r.y) {
				r.y = d;
				pmax->x = rp.x;
				pmax->y = rp.y;
			}
		}
	}
	return r;
}

void physics2_calculateMOI(union physics2_shape shape) {
	if (shape.rect->type == PHYSICS2_RECT) shape.rect->moi = (shape.rect->mass / 12.) * (shape.rect->width * shape.rect->width + shape.rect->height * shape.rect->height);
	else if (shape.rect->type == PHYSICS2_CIRCLE) shape.circle->moi = shape.circle->mass * shape.circle->radius * shape.circle->radius / 2.;
	else if (shape.rect->type == PHYSICS2_POLY) {
		float sum1 = 0.;
		float sum2 = 0.;
		for (size_t i = 0; i < shape.poly->point_count; i++) {
			size_t ni = i + 1 == shape.poly->point_count ? 0 : i + 1;
			sum1 += vec2f_cross(shape.poly->points[ni], shape.poly->points[i], 0.).z * (vec2f_dot(shape.poly->points[ni], shape.poly->points[ni]) + vec2f_dot(shape.poly->points[ni], shape.poly->points[i]) + vec2f_dot(shape.poly->points[i], shape.poly->points[i]));
			sum2 += vec2f_cross(shape.poly->points[ni], shape.poly->points[i], 0.).z;
		}
		shape.circle->moi = (shape.poly->mass / 6. * sum1 / sum2);
	}
}

void physics2_simulate(struct physics2_ctx* ctx) {
	for (size_t i = 0; i < ctx->shape_count; i++) {
		union physics2_shape shape = ctx->shapes[i];
		shape.rect->ploc.x = shape.rect->loc.x;
		shape.rect->ploc.y = shape.rect->loc.y;
		shape.rect->loc.x += shape.rect->vel.x;
		shape.rect->loc.y += shape.rect->vel.y;
		shape.rect->vel.x += shape.rect->constant_accel.x;
		shape.rect->vel.y += shape.rect->constant_accel.y;
		shape.rect->vel.x += ctx->constant_accel.x;
		shape.rect->vel.y += ctx->constant_accel.y;
		shape.rect->vel.x *= 1. - shape.rect->drag;
		shape.rect->vel.y *= 1. - shape.rect->drag;
		shape.rect->lrot = shape.rect->rot;
		shape.rect->rot += shape.rect->rps;
		shape.rect->rps *= 1. - shape.rect->rdrag;
		for (size_t x = i + 1; x < ctx->shape_count; x++) { // todo quad trees or something
			union physics2_shape shape2 = ctx->shapes[x];
			if (shape.rect->loc.x + shape.rect->radius < shape2.rect->loc.x - shape2.rect->radius || shape.rect->loc.x - shape.rect->radius > shape2.rect->loc.x + shape2.rect->radius || shape.rect->loc.y + shape.rect->radius < shape2.rect->loc.y - shape2.rect->radius || shape.rect->loc.y - shape.rect->radius > shape2.rect->loc.y + shape2.rect->radius) continue;
			size_t axes1_count = 0;
			size_t axes2_count = 0;
			if (shape.rect->type == PHYSICS2_RECT) axes1_count = 4;
			else if (shape.rect->type == PHYSICS2_CIRCLE) {
				axes1_count = 1;
			} else if (shape.rect->type == PHYSICS2_POLY) {
				axes1_count = shape.poly->point_count;
			}
			if (shape2.rect->type == PHYSICS2_RECT) axes2_count = 4;
			else if (shape2.rect->type == PHYSICS2_CIRCLE) {
				axes2_count = shape.rect->type == PHYSICS2_CIRCLE ? 0 : 1;
			} else if (shape2.rect->type == PHYSICS2_POLY) {
				axes2_count = shape2.poly->point_count;
			}
			vec2f axes1[axes1_count];
			if (axes1_count > 0) __physics2_fillAxis(shape, shape2, axes1, axes1_count);
			vec2f axes2[axes2_count];
			float mo = 0.;
			int mos = 0;
			vec2f smallest_axis;
			//vec2f cpmin1;
			vec2f cpmax1;
			//vec2f cpmin2;
			vec2f cpmax2;
			for (size_t j = 0; j < axes1_count; j++) {
				vec2f cmin1;
				vec2f cmax1;
				vec2f cmin2;
				vec2f cmax2;
				vec2f v1 = __physics2_project(axes1[j], shape, &cmin1, &cmax1);
				vec2f v2 = __physics2_project(axes1[j], shape2, &cmin2, &cmax2);
				if (v1.x > v2.x && v1.x < v2.y) {
					//printf("1-1-%i %f, %f vs %f, %f\n", j, v1.x, v1.y, v2.x, v2.y);
					float eox = v1.y > v2.y ? v2.y : v1.y;
					eox -= v1.x;
					//printf("%f\n", eox);
					if (!mos || eox < mo) {
						//cpmin1.x = cmin1.x;
						//cpmin1.y = cmin1.y;
						//cpmin2.x = cmin2.x;
						//cpmin2.y = cmin2.y;
						cpmax1.x = cmax1.x;
						cpmax1.y = cmax1.y;
						cpmax2.x = cmax2.x;
						cpmax2.y = cmax2.y;
						mo = eox;
						smallest_axis = axes1[j];
						mos = 1;
					}
				} else if (v1.y > v2.x && v1.y < v2.y) {
					//printf("1-2-%i %f, %f vs %f, %f\n", j, v1.x, v1.y, v2.x, v2.y);
					float eox = v1.y;
					eox -= v1.x < v2.x ? v2.x : v1.x;
					//printf("%f\n", eox);
					if (!mos || eox < mo) {
						//cpmin1.x = cmin1.x;
						//cpmin1.y = cmin1.y;
						//cpmin2.x = cmin2.x;
						//cpmin2.y = cmin2.y;
						cpmax1.x = cmax1.x;
						cpmax1.y = cmax1.y;
						cpmax2.x = cmax2.x;
						cpmax2.y = cmax2.y;
						mo = eox;
						smallest_axis = axes1[j];
						mos = 1;
					}
				} else if (v1.x > v2.x && v1.y < v2.y) {
					float eox = v1.y - v1.x;
					if (!mos || eox < mo) {
						//cpmin1.x = cmin1.x;
						//cpmin1.y = cmin1.y;
						//cpmin2.x = cmin2.x;
						//cpmin2.y = cmin2.y;
						cpmax1.x = cmax1.x;
						cpmax1.y = cmax1.y;
						cpmax2.x = cmax2.x;
						cpmax2.y = cmax2.y;
						mo = eox;
						smallest_axis = axes1[j];
						mos = 1;
					}
				} else if (v2.x > v1.x && v2.y < v1.y) {
					float eox = v2.y - v2.x;
					if (!mos || eox < mo) {
						//cpmin1.x = cmin1.x;
						//cpmin1.y = cmin1.y;
						//cpmin2.x = cmin2.x;
						//cpmin2.y = cmin2.y;
						cpmax1.x = cmax1.x;
						cpmax1.y = cmax1.y;
						cpmax2.x = cmax2.x;
						cpmax2.y = cmax2.y;
						mo = eox;
						smallest_axis = axes1[j];
						mos = 1;
					}
				} else {
					goto scont;
				}
			}
			if (axes2_count > 0) __physics2_fillAxis(shape2, shape, axes2, axes2_count);
			for (size_t j = 0; j < axes2_count; j++) {
				vec2f cmin1;
				vec2f cmax1;
				vec2f cmin2;
				vec2f cmax2;
				vec2f v1 = __physics2_project(axes2[j], shape, &cmin1, &cmax1);
				vec2f v2 = __physics2_project(axes2[j], shape2, &cmin2, &cmax2);
				if (v1.x > v2.x && v1.x < v2.y) {
					//printf("2-1-%i %f, %f vs %f, %f\n", j, v1.x, v1.y, v2.x, v2.y);
					float eox = v1.y > v2.y ? v2.y : v1.y;
					eox -= v1.x;
					//printf("%f\n", eox);
					if (!mos || eox < mo) {
						//cpmin1.x = cmin1.x;
						//cpmin1.y = cmin1.y;
						//cpmin2.x = cmin2.x;
						//cpmin2.y = cmin2.y;
						cpmax1.x = cmax1.x;
						cpmax1.y = cmax1.y;
						cpmax2.x = cmax2.x;
						cpmax2.y = cmax2.y;
						mo = eox;
						smallest_axis = axes2[j];
						mos = 1;
					}
				} else if (v1.y > v2.x && v1.y < v2.y) {
					//printf("2-2-%i %f, %f vs %f, %f\n", j, v1.x, v1.y, v2.x, v2.y);
					float eox = v1.y;
					eox -= v1.x < v2.x ? v2.x : v1.x;
					//printf("%f\n", eox);
					if (!mos || eox < mo) {
						//cpmin1.x = cmin1.x;
						//cpmin1.y = cmin1.y;
						//cpmin2.x = cmin2.x;
						//cpmin2.y = cmin2.y;
						cpmax1.x = cmax1.x;
						cpmax1.y = cmax1.y;
						cpmax2.x = cmax2.x;
						cpmax2.y = cmax2.y;
						mo = eox;
						smallest_axis = axes2[j];
						mos = 1;
					}
				} else if (v1.x > v2.x && v1.y < v2.y) {
					float eox = v1.y - v1.x;
					if (!mos || eox < mo) {
						//cpmin1.x = cmin1.x;
						//cpmin1.y = cmin1.y;
						//cpmin2.x = cmin2.x;
						//cpmin2.y = cmin2.y;
						cpmax1.x = cmax1.x;
						cpmax1.y = cmax1.y;
						cpmax2.x = cmax2.x;
						cpmax2.y = cmax2.y;
						mo = eox;
						smallest_axis = axes2[j];
						mos = 1;
					}
				} else if (v2.x > v1.x && v2.y < v1.y) {
					float eox = v2.y - v2.x;
					if (!mos || eox < mo) {
						//cpmin1.x = cmin1.x;
						//cpmin1.y = cmin1.y;
						//cpmin2.x = cmin2.x;
						//cpmin2.y = cmin2.y;
						cpmax1.x = cmax1.x;
						cpmax1.y = cmax1.y;
						cpmax2.x = cmax2.x;
						cpmax2.y = cmax2.y;
						mo = eox;
						smallest_axis = axes2[j];
						mos = 1;
					}
				} else {
					goto scont;
				}
			}
			if (mos) {
				if (vec2f_dot(smallest_axis, vec2f_sub(shape2.rect->loc, shape.rect->loc)) > 0.) {
					printf("invert iscirclr = %i\n", shape.rect->type == PHYSICS2_CIRCLE);
					smallest_axis = vec2f_scale(smallest_axis, -1.);
				}
				vec2f normal;
				normal.x = smallest_axis.x;
				normal.y = smallest_axis.y; // todo: fix polygon normals around here
				printf("mo = %f  --- %f, %f\n", mo, normal.x, normal.y);
				smallest_axis = vec2f_scale(smallest_axis, mo);
				vec2f avg;
				if (shape.rect->type == PHYSICS2_CIRCLE) {
					avg = cpmax1;
				} else if (shape.rect->type == PHYSICS2_CIRCLE) {
					avg = cpmax2;
				} else {
					avg.x = (cpmax2.x - cpmax1.x) / 2. + shape.rect->loc.x;
					avg.y = (cpmax2.y - cpmax1.y) / 2. + shape.rect->loc.y;
				}
				shape.rect->p1.x = avg.x;
				shape.rect->p1.y = avg.y;
				shape.rect->p2.x = cpmax2.x;
				shape.rect->p2.y = cpmax2.y;
				shape.rect->p3.x = -cpmax1.x + shape.rect->loc.x * 2.;
				shape.rect->p3.y = -cpmax1.y + shape.rect->loc.y * 2.;
				/*shape.rect->p1.x = cpmin1.x;
				 shape.rect->p1.y = cpmin1.y;
				 shape.rect->p2.x = cpmax1.x;
				 shape.rect->p2.y = cpmax1.y;
				 shape2.rect->p1.x = -cpmin2.x;
				 shape2.rect->p1.y = -cpmin2.y;
				 shape2.rect->p2.x = -cpmax2.x;
				 shape2.rect->p2.y = -cpmax2.y;*/
				//printf("<%f, %f> - %f, %f\n", shape.rect->loc.y + shape.rect->height / 2., shape2.rect->loc.y - shape2.rect->height / 2., smallest_axis.x, smallest_axis.y);
				shape2.rect->loc = vec2f_sub(shape2.rect->loc, smallest_axis);
				//
				vec2f rap = vec2f_sub(avg, shape.rect->loc);
				vec2f rbp = vec2f_sub(avg, shape2.rect->loc);
				vec2f tmp;
				tmp.x = -shape.rect->rps * rap.x;
				tmp.y = shape.rect->rps * rap.x;
				vec2f vap1 = vec2f_add(shape.rect->vel, tmp);
				tmp.x = -shape2.rect->rps * rap.x;
				tmp.y = shape2.rect->rps * rbp.x;
				vec2f vbp1 = vec2f_add(shape2.rect->vel, tmp);
				vec2f vab1 = vec2f_sub(vap1, vbp1);
				float j = vec2f_dot(vec2f_scale(vab1, -(1 + (shape2.rect->elasticity > shape.rect->elasticity ? shape2.rect->elasticity : shape.rect->elasticity))), normal);
				//printf("bj = %f\n", j);
				float dj = (1 / shape.rect->mass) + (1 / shape2.rect->mass);
				vec3f t1;
				t1.x = rap.x;
				t1.y = rap.y;
				t1.z = 0.;
				vec3f t2;
				t2.x = normal.x;
				t2.y = normal.y;
				t2.z = 0.;
				vec3f dt = vec3f_cross(t1, t2);
				if (shape.rect->moi > 0.) dj += vec3f_dot(dt, dt) / shape.rect->moi;
				t1.x = rbp.x;
				t1.y = rbp.y;
				t1.z = 0.;
				dt = vec3f_cross(t1, t2);
				if (shape2.rect->moi > 0.) dj += vec3f_dot(dt, dt) / shape2.rect->moi;
				//printf("dj = %f\n", dj);
				j /= dj;
				//printf("1 %f: %f, %f; %f, %f\n", j, shape.rect->vel.x, shape.rect->vel.y, shape2.rect->vel.x, shape2.rect->vel.y);
				shape.rect->vel = vec2f_add(shape.rect->vel, vec2f_scale(normal, j / shape.rect->mass));
				shape2.rect->vel = vec2f_sub(shape2.rect->vel, vec2f_scale(normal, j / shape2.rect->mass));
				//printf("2 %f: %f, %f; %f, %f\n", j, shape.rect->vel.x, shape.rect->vel.y, shape2.rect->vel.x, shape2.rect->vel.y);
				if (shape.rect->moi > 0.) {
					t1.x = rap.x;
					t1.y = rap.y;
					t1.z = 0.;
					vec3f rx = vec3f_cross(t1, vec3f_scale(t2, j));
					shape.rect->rps -= rx.z / shape.rect->moi;
				}
				if (shape2.rect->moi > 0.) {
					t1.x = rbp.x;
					t1.y = rbp.y;
					t1.z = 0.;
					vec3f rx = vec3f_cross(t1, vec3f_scale(t2, j));
					shape2.rect->rps += rx.z / shape2.rect->moi;
				}
				//shape2.rect->vel.x = 0.;
				//shape2.rect->vel.y = 0.;
				//shape.rect->vel.x = 0.;
				//shape.rect->vel.y = 0.;
			}
			//printf("collide <%f, %f> %f, %f vs <%f, %f> %f, %f\n", shape.rect->width, shape.rect->height, shape.rect->loc.x, shape.rect->loc.y, shape2.rect->width, shape2.rect->height, shape2.rect->loc.x, shape2.rect->loc.y);
			continue;
			scont: ;
		}
	}
}

