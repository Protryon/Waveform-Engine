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
	shape.rect->friction = .9;
	shape.rect->drag = .001;
	shape.rect->rdrag = .001;
	shape.rect->elasticity = 1.;
	shape.rect->data = NULL;
}

void physics2_teleport(union physics2_shape shape, vec2f loc) {
	shape.rect->ploc = shape.rect->loc = loc;
}

void physics2_teleportRot(union physics2_shape shape, float rot) {
	shape.rect->lrot = shape.rect->rot = rot;
}

void physics2_finalizeShape(union physics2_shape shape, float massMultiplier) {
	physics2_setMassByArea(shape, 1.);
	physics2_adjustCOM(shape);
	physics2_calculateMOI(shape);
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

void physics2_adjustCOM(union physics2_shape shape) {
	if (shape.rect->type == PHYSICS2_POLY) {
		float xc = 0.;
		float yc = 0.;
		float area = 0.;
		for (size_t i = 0; i < shape.poly->point_count; i++) {
			size_t ni = i + 1 == shape.poly->point_count ? 0 : i + 1;
			xc += (shape.poly->points[i].x + shape.poly->points[ni].x) * (shape.poly->points[i].x * shape.poly->points[ni].y - shape.poly->points[ni].x * shape.poly->points[i].y);
			yc += (shape.poly->points[i].y + shape.poly->points[ni].y) * (shape.poly->points[i].x * shape.poly->points[ni].y - shape.poly->points[ni].x * shape.poly->points[i].y);
			area += shape.poly->points[i].x * shape.poly->points[ni].y - shape.poly->points[i].y * shape.poly->points[ni].x;
		}
		xc /= 3. * area;
		yc /= 3. * area;
		for (size_t i = 0; i < shape.poly->point_count; i++) {
			shape.poly->points[i].x -= xc;
			shape.poly->points[i].y -= yc;
		}
	}
}

size_t __physics2_getPointCount(union physics2_shape shape) {
	if (shape.rect->type == PHYSICS2_RECT) {
		return 4;
	} else if (shape.rect->type == PHYSICS2_CIRCLE) {
		return 2; // really infinite or 0, but we generate the 2 needed on demand
	} else if (shape.rect->type == PHYSICS2_POLY) {
		return shape.poly->point_count;
	}
	return 0;
}

void __physics2_getAdjustedGlobalPoints(union physics2_shape shape, vec2f* points, vec2f onLine) { // onLine is only used for circles, always an axis (but doesnt need to be)
	float c = cosf(shape.rect->rot);
	float s = sinf(shape.rect->rot);
	if (shape.rect->type == PHYSICS2_RECT) {
		points[0].x = -shape.rect->width / 2.;
		points[0].y = -shape.rect->height / 2.;
		float nx = points[0].x * c - points[0].y * s;
		points[0].y = points[0].y * c + points[0].x * s;
		points[0].x = nx;
		points[0].x += shape.rect->loc.x;
		points[0].y += shape.rect->loc.y;
		points[1].x = -shape.rect->width / 2.;
		points[1].y = shape.rect->height / 2.;
		nx = points[1].x * c - points[1].y * s;
		points[1].y = points[1].y * c + points[1].x * s;
		points[1].x = nx;
		points[1].x += shape.rect->loc.x;
		points[1].y += shape.rect->loc.y;
		points[2].x = shape.rect->width / 2.;
		points[2].y = shape.rect->height / 2.;
		nx = points[2].x * c - points[2].y * s;
		points[2].y = points[2].y * c + points[2].x * s;
		points[2].x = nx;
		points[2].x += shape.rect->loc.x;
		points[2].y += shape.rect->loc.y;
		points[3].x = shape.rect->width / 2.;
		points[3].y = -shape.rect->height / 2.;
		nx = points[3].x * c - points[3].y * s;
		points[3].y = points[3].y * c + points[3].x * s;
		points[3].x = nx;
		points[3].x += shape.rect->loc.x;
		points[3].y += shape.rect->loc.y;
	} else if (shape.rect->type == PHYSICS2_CIRCLE) {
		points[0] = vec2f_scale(onLine, shape.circle->radius);
		float nx = points[0].x * c - points[0].y * s;
		points[0].y = points[0].y * c + points[0].x * s;
		points[0].x = nx;
		points[1] = vec2f_scale(onLine, -shape.circle->radius);
		nx = points[1].x * c - points[1].y * s;
		points[1].y = points[1].y * c + points[1].x * s;
		points[1].x = nx;
		points[0] = vec2f_add(points[0], shape.circle->loc);
		points[1] = vec2f_add(points[1], shape.circle->loc);
	} else if (shape.rect->type == PHYSICS2_POLY) {
		float nx;
		for (size_t i = 0; i < shape.poly->point_count; i++) {
			points[i] = shape.poly->points[i];
			nx = points[i].x * c - points[i].y * s;
			points[i].y = points[i].y * c + points[i].x * s;
			points[i].x = nx;
			points[i] = vec2f_add(points[i], shape.poly->loc);
		}
	}
}

void __physics2_fillAxis(union physics2_shape shape, union physics2_shape shape2, vec2f* axes, size_t axes_count) {
	vec2f t;
	t.x = 0.;
	t.y = 0.;
	if (shape.rect->type == PHYSICS2_CIRCLE) {
		if (shape2.rect == NULL) {
			axes[0].x = 0.;
			axes[0].y = 0.;
		} else if (shape2.rect->type == PHYSICS2_CIRCLE) {
			axes[0] = vec2f_norm(vec2f_sub(shape.circle->loc, shape2.circle->loc));
		} else if (shape2.rect->type == PHYSICS2_RECT || shape2.rect->type == PHYSICS2_POLY) {
			size_t pc = __physics2_getPointCount(shape2);
			vec2f tax[pc];
			__physics2_getAdjustedGlobalPoints(shape2, tax, t);
			vec2f best;
			float bd = 0.;
			vec2f t;
			for (size_t i = 0; i < pc; i++) {
				if (i == 0) {
					t = tax[i];
					best = vec2f_sub(shape.circle->loc, t);
					bd = best.x * best.x + best.y * best.y;
				} else {
					t = tax[i];
					t = vec2f_sub(shape.circle->loc, t);
					float c = t.x * t.x + t.y * t.y;
					if (c < bd) {
						bd = c;
						best = t;
					}
				}

			}
			axes[0] = vec2f_norm(vec2f_perp(best));
		}
		return;
	}
	__physics2_getAdjustedGlobalPoints(shape, axes, t);
	vec2f t0 = axes[0];
	for (size_t j = 0; j < axes_count; j++) {
		axes[j] = vec2f_norm(vec2f_perp(vec2f_sub(axes[j], (j + 1) == axes_count ? t0 : axes[j + 1])));
	}
}

void physics2_drawShape(union physics2_shape shape, float partialTick) {
	vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
	glPushMatrix();
	glTranslatef(iloc.x, iloc.y, 0.);
	glRotatef(360. * physics2_getInterpolatedRotation(shape, partialTick) / (M_PI * 2.), 0., 0., 1.);
	if (shape.rect->type == PHYSICS2_RECT) {
		glBegin (GL_QUADS);
		//if (shape.rect->p2.x == 3) glColor4f(0., 1., 0., 1.);
		//if (shape.rect->p2.x == 0) glColor4f(0., 1., 0., 1.);
		glVertex2f(-shape.rect->width / 2., -shape.rect->height / 2.);
		//if (shape.rect->p2.x == 3) glColor4f(1., 0., 0., 1.);
		//if (shape.rect->p2.x == 1) glColor4f(0., 1., 0., 1.);
		glVertex2f(-shape.rect->width / 2., shape.rect->height / 2.);
		//if (shape.rect->p2.x == 0) glColor4f(1., 0., 0., 1.);
		//if (shape.rect->p2.x == 2) glColor4f(0., 1., 0., 1.);
		glVertex2f(shape.rect->width / 2., shape.rect->height / 2.);
		//if (shape.rect->p2.x == 1) glColor4f(1., 0., 0., 1.);
		//if (shape.rect->p2.x == 3) glColor4f(0., 1., 0., 1.);
		glVertex2f(shape.rect->width / 2., -shape.rect->height / 2.);
		//if (shape.rect->p2.x == 2) glColor4f(1., 0., 0., 1.);
		//if (shape.rect->p2.x == 3) glColor4f(1., 0., 0., 1.);
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

/*void __physics2_drawDebug(union physics2_shape shape, float partialTick) {
 glBegin (GL_LINES);
 glColor4f(1., 0., 1., 1.);
 glVertex2f(0., 0.);
 glVertex2f(shape.rect->p2.x, shape.rect->p2.y);
 glColor4f(.0, .5, .6, 1.);
 glVertex2f(0., 0.);
 glVertex2f(shape.rect->p3.x, shape.rect->p3.y);
 glColor4f(0., 1., 1., 1.);
 glVertex2f(0., 0.);
 glVertex2f(shape.rect->p1.x, shape.rect->p1.y);
 glEnd();
 vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
 glPushMatrix();
 glTranslatef(iloc.x, iloc.y, 0.);
 glBegin (GL_LINES);
 glColor4f(0., 1., 0., 1.);
 glVertex2f(0., 0.);
 vec2f xproj;
 xproj.x = 1.;
 xproj.y = 0.;
 xproj = vec2f_project(shape.rect->vel, xproj);
 vec2f yproj;
 yproj.x = 0.;
 yproj.y = 1.;
 yproj = vec2f_project(shape.rect->vel, yproj);
 //printf("%f, %f\n", xproj.x, xproj.y);
 glVertex2f(xproj.x * 100., xproj.y * 100.);
 glVertex2f(0., 0.);
 glVertex2f(yproj.x * 100., yproj.y * 100.);
 glEnd();
 //glRotatef(360. * physics2_getInterpolatedRotation(shape, partialTick) / (M_PI * 2.), 0., 0., 1.);
 size_t ac = __physics2_getPointCount(shape);
 vec2f vecs[ac];
 union physics2_shape shape2;
 shape2.circle = NULL;
 __physics2_fillAxis(shape, shape2, vecs, ac);
 glBegin(GL_LINES);
 for (int i = 0; i < ac; i++) {
 glVertex2f(0., 0.);
 glVertex2f(vecs[i].x * 100., vecs[i].y * 100);
 }
 glEnd();
 glPopMatrix();
 glColor4f(1., 0., 0., 1.);
 }*/

void physics2_drawAllShapes(struct physics2_ctx* ctx, float x1, float y1, float x2, float y2, float partialTick) {
	for (size_t i = 0; i < ctx->shape_count; i++) {
		union physics2_shape shape = ctx->shapes[i];
		vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
		//printf("%f, %f, %f, %f, %f, %f, %f\n", iloc.x, iloc.y, x1, y1, x2, y2, shape.rect->radius);
		if (iloc.x - x1 > -shape.rect->radius && iloc.x - x2 < shape.rect->radius && iloc.y - y1 > -shape.rect->radius && iloc.y - y2 < shape.rect->radius) {
			physics2_drawShape(ctx->shapes[i], partialTick);
		}
	}
	/*for (size_t i = 0; i < ctx->shape_count; i++) {
	 union physics2_shape shape = ctx->shapes[i];
	 vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
	 //printf("%f, %f, %f, %f, %f, %f, %f\n", iloc.x, iloc.y, x1, y1, x2, y2, shape.rect->radius);
	 if (iloc.x - x1 > -shape.rect->radius && iloc.x - x2 < shape.rect->radius && iloc.y - y1 > -shape.rect->radius && iloc.y - y2 < shape.rect->radius) {
	 //printf("dun\n");
	 __physics2_drawDebug(ctx->shapes[i], partialTick);
	 }
	 }*/
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

void physics2_applyForce(union physics2_shape shape, vec2f force, vec2f loc) {
	shape.rect->vel.x += force.x / shape.rect->mass;
	shape.rect->vel.y += force.y / shape.rect->mass;
	shape.rect->rps += vec2f_cross(loc, force, 0.).z / shape.rect->moi;
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

vec2f __physics2_project(vec2f axis, union physics2_shape shape, union physics2_shape shape2, vec2f* pmin, vec2f* pmax) { // x = min, y = max
	vec2f r;
	r.x = 0.;
	r.y = 0.;
	size_t pc = __physics2_getPointCount(shape2);
	vec2f tax[pc];
	__physics2_getAdjustedGlobalPoints(shape2, tax, axis);
	for (size_t i = 0; i < pc; i++) {
		float d = vec2f_dot(axis, tax[i]);
		if (i == 0) {
			r.x = d;
			r.y = d;
			pmin->x = tax[i].x;
			pmin->y = tax[i].y;
			pmax->x = tax[i].x;
			pmax->y = tax[i].y;
		} else if (d < r.x) {
			r.x = d;
			pmin->x = tax[i].x;
			pmin->y = tax[i].y;
		} else if (d > r.y) {
			r.y = d;
			pmax->x = tax[i].x;
			pmax->y = tax[i].y;
		}
	}
	return r;
}

vec2f __physics2_getCollisionPoint(vec2f mtv, union physics2_shape shape, union physics2_shape shape2) {
	vec2f support[2];
	int s2 = 0;
	vec2f support2[2];
	int s22 = 0;
	vec2f nmtv = vec2f_norm(mtv);
	{
		size_t pc = __physics2_getPointCount(shape);
		vec2f tax[pc];
		__physics2_getAdjustedGlobalPoints(shape, tax, nmtv); // axis might be better than mtv?
		float mind = 0.;
		for (size_t i = 0; i < pc; i++) {
			float d = vec2f_dot(tax[i], mtv);
			if (i == 0 || d < mind) {
				mind = d;
			}
		}
		for (size_t i = 0; i < pc; i++) {
			float d = vec2f_dot(tax[i], mtv);
			if (d < (mind + 1.0E-8)) {
				support[s2 ? 1 : 0] = tax[i];
				if (s2++) break;
			}
		}
	}
	{
		size_t pc = __physics2_getPointCount(shape2);
		vec2f tax[pc];
		__physics2_getAdjustedGlobalPoints(shape2, tax, nmtv); // axis might be better than mtv?
		float mind = 0.;
		for (size_t i = 0; i < pc; i++) {
			float d = vec2f_dot(tax[i], vec2f_scale(mtv, -1.));
			if (i == 0 || d < mind) {
				mind = d;
			}
		}
		for (size_t i = 0; i < pc; i++) {
			float d = vec2f_dot(tax[i], vec2f_scale(mtv, -1.));
			if (d < (mind + 1.0E-8)) {
				support2[s22 ? 1 : 0] = tax[i];
				if (s22++) break;
			}
		}
	}
	if (s22 == 1 && s2 == 1) {
		return support[0];
	} else if (s22 == 2 && s2 == 1) {
		return support[0];
	} else if (s22 == 1 && s2 == 2) {
		return support2[0];
	} else if (s22 == 2 && s2 == 2) {
		vec2f bma = vec2f_sub(support2[1], support2[0]);
		vec2f c1ma = vec2f_sub(support[0], support2[0]);
		vec2f c2ma = vec2f_sub(support[1], support2[0]);
		float cp1 = vec2f_cross(bma, c1ma, 0.).z;
		float dpx1 = vec2f_dot(bma, c1ma);
		int dp1 = cp1 == 0. && dpx1 > 0. && dpx1 < (bma.x * bma.x + bma.y * bma.y);
		float cp2 = vec2f_cross(bma, c2ma, 0.).z;
		float dpx2 = vec2f_dot(bma, c2ma);
		int dp2 = cp2 == 0. && dpx2 > 0. && dpx2 < (bma.x * bma.x + bma.y * bma.y);
		//printf("s10 = %f, %f  s11 = %f, %f  s20 = %f, %f, s21 = %f, %f\n", support[0].x, support[0].y, support[1].x, support[1].y, support2[0].x, support2[0].y, support2[1].x, support2[1].y);
		//printf("%f, %f, %i, %i, %f, %f, %f, %f\n", cp1, cp2, dp1, dp2, vec2f_dot(bma, c1ma), (bma.x * bma.x + bma.y * bma.y), vec2f_dot(bma, c2ma), (bma.x * bma.x + bma.y * bma.y));
		if (dp1 && dp2) {
			vec2f avg;
			avg.x = (support[0].x + support[1].x) / 2.;
			avg.y = (support[0].y + support[1].y) / 2.;
			return avg;
		} else if (dp1) {
			vec2f avg;
			avg.x = (support2[0].x + support[0].x) / 2.;
			avg.y = (support2[0].y + support[0].y) / 2.;
			return avg;
		} else if (dp2) {
			vec2f avg;
			avg.x = (support[1].x + support2[1].x) / 2.;
			avg.y = (support[1].y + support2[1].y) / 2.;
			return avg;
		} else {
			vec2f avg;
			avg.x = (support2[0].x + support2[1].x) / 2.;
			avg.y = (support2[0].y + support2[1].y) / 2.;
			return avg;
		}
	}
	vec2f t;
	t.x = 0.;
	t.y = 0.;
	return t;
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
			size_t ai = 0;
			int a = 0;
			//vec2f cpmin1;
			vec2f cpmax1;
			//vec2f cpmin2;
			vec2f cpmax2;
			for (size_t j = 0; j < axes1_count; j++) {
				vec2f cmin1;
				vec2f cmax1;
				vec2f cmin2;
				vec2f cmax2;
				vec2f v1 = __physics2_project(axes1[j], shape, shape2, &cmin1, &cmax1);
				vec2f v2 = __physics2_project(axes1[j], shape2, shape, &cmin2, &cmax2);
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
						a = 1;
						ai = j;
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
						a = 1;
						ai = j;
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
						a = 1;
						ai = j;
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
						a = 1;
						ai = j;
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
				vec2f v1 = __physics2_project(axes2[j], shape, shape2, &cmin1, &cmax1);
				vec2f v2 = __physics2_project(axes2[j], shape2, shape, &cmin2, &cmax2);
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
						a = 2;
						ai = j;
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
						a = 2;
						ai = j;
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
						a = 2;
						ai = j;
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
						a = 2;
						ai = j;
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
					printf("invert iscircle = %i\n", shape.rect->type == PHYSICS2_CIRCLE);
					smallest_axis = vec2f_scale(smallest_axis, -1.);
				}
				vec2f normal;
				normal.x = smallest_axis.x;
				normal.y = smallest_axis.y;
				//printf("mo = %f  --- %f, %f\n", mo, normal.x, normal.y);
				smallest_axis = vec2f_scale(smallest_axis, mo);
				shape2.rect->loc = vec2f_sub(shape2.rect->loc, smallest_axis);
				vec2f avg = __physics2_getCollisionPoint(smallest_axis, shape, shape2);
				/*
				 if (shape.rect->type == PHYSICS2_CIRCLE) {
				 avg = cpmax1;
				 } else if (shape2.rect->type == PHYSICS2_CIRCLE) {
				 avg = cpmax2;
				 } else {
				 avg.x = (cpmax2.x - cpmax1.x) / 2. + shape.rect->loc.x;
				 avg.y = (cpmax2.y - cpmax1.y) / 2. + shape.rect->loc.y;
				 }*/
				//shape.rect->p1.x = avg.x;						//shape.rect->loc.x + smallest_axis.x * 100.; // avg.x; //(cpmax2.x - cpmax1.x) / 2. + shape.rect->loc.x;
				//shape.rect->p1.y = avg.y;						//shape.rect->loc.y + smallest_axis.y * 100.; // avg.y; //(cpmax2.y - cpmax1.y) / 2. + shape.rect->loc.y;
				//shape.rect->p2.x = cpmax2.x;
				//shape.rect->p2.y = cpmax2.y;
				//(a == 1 ? shape : shape2).rect->p2.x = ai;
				//shape.rect->p3.x = cpmax1.x + shape.rect->loc.x * 2.;
				//shape.rect->p3.y = -cpmax1.y + shape.rect->loc.y * 2.;
				/*shape.rect->p1.x = cpmin1.x;
				 shape.rect->p1.y = cpmin1.y;
				 shape.rect->p2.x = cpmax1.x;
				 shape.rect->p2.y = cpmax1.y;
				 shape2.rect->p1.x = -cpmin2.x;
				 shape2.rect->p1.y = -cpmin2.y;
				 shape2.rect->p2.x = -cpmax2.x;
				 shape2.rect->p2.y = -cpmax2.y;*/
				//printf("<%f, %f> - %f, %f\n", shape.rect->loc.y + shape.rect->height / 2., shape2.rect->loc.y - shape2.rect->height / 2., smallest_axis.x, smallest_axis.y);
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
				float dj = (1. / shape.rect->mass) + (1. / shape2.rect->mass);
				vec3f t1;
				t1.x = rap.x;
				t1.y = rap.y;
				t1.z = 0.;
				vec3f t2;
				t2.x = normal.x;
				t2.y = normal.y;
				t2.z = 0.;
				vec3f dt = vec3f_cross(t1, t2);
				if (shape.rect->moi > 0.) {
					float sd = vec3f_dot(dt, dt);
					dj += sd / shape.rect->moi;
				}
				t1.x = rbp.x;
				t1.y = rbp.y;
				t1.z = 0.;
				dt = vec3f_cross(t1, t2);
				if (shape2.rect->moi > 0.) {
					float sd = vec3f_dot(dt, dt);
					dj += sd / shape2.rect->moi;
				}
				//printf("dj = %f\n", j);
				j /= dj;
				//printf("pj = %f\n", j);
				//printf("1 %f: %f, %f; %f, %f\n", j, shape.rect->vel.x, shape.rect->vel.y, shape2.rect->vel.x, shape2.rect->vel.y);
				shape.rect->vel = vec2f_add(shape.rect->vel, vec2f_scale(normal, j / shape.rect->mass));
				shape2.rect->vel = vec2f_sub(shape2.rect->vel, vec2f_scale(normal, j / shape2.rect->mass));
				float frict = (shape2.rect->friction + shape.rect->friction) / 2.;
				shape.rect->vel = vec2f_add(vec2f_scale(vec2f_project(shape.rect->vel, vec2f_perp(normal)), frict), vec2f_project(shape.rect->vel, normal));
				shape2.rect->vel = vec2f_add(vec2f_scale(vec2f_project(shape2.rect->vel, vec2f_perp(normal)), frict), vec2f_project(shape2.rect->vel, normal));
				//printf("2 %f: %f, %f; %f, %f\n", j, shape.rect->vel.x, shape.rect->vel.y, shape2.rect->vel.x, shape2.rect->vel.y);
				//printf("s1 moi = %f, s2 moi = %f\n", shape.rect->moi, shape2.rect->moi);
				if (shape.rect->moi > 0.) {
					t1.x = rap.x;
					t1.y = rap.y;
					t1.z = 0.;
					vec3f rx = vec3f_cross(t1, vec3f_scale(t2, j));
					shape.rect->rps += rx.z / shape.rect->moi;
					printf("sr1ps type = %i, rx.z = %f, moi = %f    ---   %f, %f\n", shape.rect->type, rx.z, shape.rect->moi, rap.x, rap.y);
//printf("s1rps type = %i, rpsd = %f, trps = %f\n", shape.rect->type, -rx.z / shape.rect->moi, shape.rect->rps);
				}
				if (shape2.rect->moi > 0.) {
					t1.x = rbp.x;
					t1.y = rbp.y;
					t1.z = 0.;
					vec3f rx = vec3f_cross(t1, vec3f_scale(t2, j));
					shape2.rect->rps -= rx.z / shape2.rect->moi;
					printf("sr2ps type = %i, rx.z = %f, moi = %f    ---   %f, %f\n", shape2.rect->type, rx.z, shape2.rect->moi, rbp.x, rbp.y);
					//printf("s2rps type = %i, rpsd = %f, trps = %f\n", shape2.rect->type, rx.z / shape2.rect->moi, shape2.rect->rps);
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

