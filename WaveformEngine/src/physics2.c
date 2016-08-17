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
	shape.poly->loc.x = 0.;
	shape.poly->loc.y = 0.;
	shape.poly->ploc = shape.poly->loc;
	shape.poly->rot = 0.;
	shape.poly->lrot = 0.;
	shape.poly->rps = 0.;
	shape.poly->vel.x = 0.;
	shape.poly->vel.y = 0.;
	shape.poly->constant_accel.x = 0.;
	shape.poly->constant_accel.y = 0.;
	shape.poly->mass = 1.;
	shape.poly->moi = 0.;
	shape.poly->friction = .1;
	shape.poly->drag = .001;
	shape.poly->rdrag = .001;
	shape.poly->elasticity = 1.;
	shape.poly->data = NULL;
}

void physics2_setCanCollideCallback(struct physics2_ctx* ctx, void (*canCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape)) {
	ctx->canCollide = canCollide;
}

void physics2_setSensorCollideCallback(struct physics2_ctx* ctx, void (*sensorCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape)) {
	ctx->sensorCollide = sensorCollide;
}

void physics2_setPreCollideCallback(struct physics2_ctx* ctx, void (*preCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape, vec2f)) {
	ctx->preCollide = preCollide;
}

void physics2_setPostCollideCallback(struct physics2_ctx* ctx, void (*postCollide)(struct physics2_ctx*, union physics2_shape, union physics2_shape, vec2f)) {
	ctx->postCollide = postCollide;
}

void physics2_teleport(union physics2_shape shape, vec2f loc) {
	shape.poly->ploc = shape.poly->loc = loc;
}

void physics2_teleportRot(union physics2_shape shape, float rot) {
	shape.poly->lrot = shape.poly->rot = rot;
}

void physics2_finalizeShape(union physics2_shape shape, float massMultiplier) {
	physics2_setMassByArea(shape, 1.);
	physics2_adjustCOM(shape);
	physics2_calculateMOI(shape);
}

union physics2_shape physics2_newRect(float width, float height) {
	vec2f vecs[4];
	vecs[0].x = -width / 2.;
	vecs[0].y = -height / 2.;
	vecs[1].x = -width / 2.;
	vecs[1].y = height / 2.;
	vecs[2].x = width / 2.;
	vecs[2].y = height / 2.;
	vecs[3].x = width / 2.;
	vecs[3].y = -height / 2.;
	return physics2_newPoly(vecs, 4, 0);
}

union physics2_shape physics2_newRightTriangle(float width, float height) {
	vec2f vecs[3];
	vecs[0].x = -width / 2.;
	vecs[0].y = -height / 2.;
	vecs[1].x = -width / 2.;
	vecs[1].y = height / 2.;
	vecs[2].x = width / 2.;
	vecs[2].y = height / 2.;
	return physics2_newPoly(vecs, 3, 0);
}

union physics2_shape physics2_newEquilateralTriangle(float width) {
	vec2f vecs[3];
	vecs[0].x = 0.;
	vecs[0].y = width;
	vecs[1].x = width;
	vecs[1].y = width;
	vecs[2].x = width / 2.;
	vecs[2].y = 0.;
	return physics2_newPoly(vecs, 3, 0);
}

union physics2_shape physics2_newIsoscelesTriangle(float width, float height) {
	vec2f vecs[3];
	vecs[0].x = 0.;
	vecs[0].y = height;
	vecs[1].x = width;
	vecs[1].y = height;
	vecs[2].x = width / 2.;
	vecs[2].y = 0.;
	return physics2_newPoly(vecs, 3, 0);
}

union physics2_shape physics2_newRegularPolygon(float radius, int n) {
	vec2f vecs[n];
	for (int i = 0; i < n; i++) {
		vecs[i].x = radius * cosf(2. * M_PI * (float) i / (float) n);
		vecs[i].y = radius * sinf(2. * M_PI * (float) i / (float) n);
	}
	return physics2_newPoly(vecs, n, 0);
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

struct __physics2_earlink {
		struct __physics2_earlink* prev;
		struct __physics2_earlink* next;
		vec2f point;
};

void physics2_triangulate(struct physics2_poly* poly) {
	if (!poly->concave) return;
	if (poly->triangles != NULL) free(poly->triangles);
	poly->triangle_count = poly->point_count - 2;
	poly->triangles = smalloc(sizeof(struct __physics2_triangle) * poly->triangle_count);
	struct __physics2_earlink* pointhead = smalloc(sizeof(struct __physics2_earlink) * poly->point_count);
	struct __physics2_earlink* point = pointhead;
	for (size_t i = 0; i < poly->point_count; i++) {
		point->point = poly->points[i];
		if (i < poly->point_count - 1) {
			struct __physics2_earlink* nextpoint = pointhead + ((i + 1) % poly->point_count);
			nextpoint->prev = point;
			point->next = nextpoint;
			point = nextpoint;
		}
	}
	//this is a naive ear clipping running at O(n^3), this could certainly be optimized, but is probably unneeded due to the very rare calling and generally small data size
	pointhead->prev = point;
	point->next = pointhead;
	point = pointhead;
	for (size_t i = 0; i < poly->triangle_count; i++) {
		struct __physics2_earlink* start = point;
		do {
			vec2f p0 = point->prev->point;
			vec2f p1 = point->point;
			vec2f p2 = point->next->point;
			vec2f px1 = vec2f_sub(p1, p0);
			vec2f px2 = vec2f_sub(p1, p2);
			float internal_angle = atan2(vec2f_cross(px1, px2, 0.).z, vec2f_dot(px1, px2));
			if (internal_angle >= 0.) {
				poly->triangles[i].v1 = p0;
				poly->triangles[i].v2 = p1;
				poly->triangles[i].v3 = p2;
				//printf("%f, %f; %f, %f; %f, %f  --  %f\n", p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, internal_angle);
				point->prev->next = point->next;
				point->next->prev = point->prev;
				point = point->prev;
				break;
			}
			if (point == point->prev) break;
			point = point->prev;
		} while (point != start);
	}
	free(pointhead);
}

union physics2_shape physics2_newPoly(vec2f* points, size_t point_count, uint8_t concave) {
	if (point_count == 0) {
		union physics2_shape shape;
		shape.poly = NULL;
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
	rect->concave = concave;
	rect->triangles = NULL;
	rect->triangle_count = 0;
	return shape;
}

void physics2_delShape(struct physics2_ctx* ctx, union physics2_shape shape) {
	if (ctx->shapes[shape.poly->index].poly->type == PHYSICS2_POLY) {
		if (ctx->shapes[shape.poly->index].poly->calllist > 0) glDeleteLists(ctx->shapes[shape.poly->index].poly->calllist, 1);
		free(ctx->shapes[shape.poly->index].poly->points);
		if (shape.poly->concave) free(shape.poly->triangles);
	}
	ctx->shapes[shape.poly->index].poly = NULL;
	free(shape.poly);
}

void physics2_addShape(struct physics2_ctx* ctx, union physics2_shape shape) {
	if (ctx->shapes == NULL) {
		ctx->shapes = smalloc(sizeof(union physics2_shape));
		ctx->shape_count = 0;
	} else ctx->shapes = srealloc(ctx->shapes, sizeof(union physics2_shape) * (ctx->shape_count + 1));
	shape.poly->index = ctx->shape_count;
	ctx->shapes[ctx->shape_count++] = shape;
}

struct physics2_ctx* physics2_init() {
	struct physics2_ctx* ctx = scalloc(sizeof(struct physics2_ctx));
	return ctx;
}

void physics2_free(struct physics2_ctx* ctx) {
	for (size_t i = 0; i < ctx->shape_count; i++) {
		if (ctx->shapes[i].poly != NULL) physics2_delShape(ctx, ctx->shapes[i]);
	}
	if (ctx->shapes != NULL) free(ctx->shapes);
	free(ctx);
}

/*
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
 }*/

void physics2_adjustCOM(union physics2_shape shape) {
	if (shape.poly->type == PHYSICS2_POLY) {
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
	if (shape.poly->type == PHYSICS2_CIRCLE) {
		return 2; // really infinite or 0, but we generate the 2 needed on demand
	} else if (shape.poly->type == PHYSICS2_POLY) {
		return shape.poly->point_count;
	}
	return 0;
}

void __physics2_getAdjustedGlobalPoints(union physics2_shape shape, vec2f* points, vec2f onLine, int caxisgen) { // onLine is only used for circles, always an axis (but doesnt need to be); caxisgen is 1 when axes are being generated for a concave shape
	float c = cosf(shape.poly->rot);
	float s = sinf(shape.poly->rot);
	if (shape.poly->type == PHYSICS2_CIRCLE) {
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
	} else if (shape.poly->type == PHYSICS2_POLY) {
		float nx;
		if (caxisgen && shape.poly->concave) {
			for (size_t i = 0; i < shape.poly->triangle_count; i++) {
				points[i * 3] = shape.poly->triangles[i].v1;
				points[i * 3 + 1] = shape.poly->triangles[i].v2;
				points[i * 3 + 2] = shape.poly->triangles[i].v3;
				nx = points[i * 3].x * c - points[i * 3].y * s;
				points[i * 3].y = points[i * 3].y * c + points[i * 3].x * s;
				points[i * 3].x = nx;
				points[i * 3] = vec2f_add(points[i * 3], shape.poly->loc);
				nx = points[i * 3 + 1].x * c - points[i * 3 + 1].y * s;
				points[i * 3 + 1].y = points[i * 3 + 1].y * c + points[i * 3 + 1].x * s;
				points[i * 3 + 1].x = nx;
				points[i * 3 + 1] = vec2f_add(points[i * 3 + 1], shape.poly->loc);
				nx = points[i * 3 + 2].x * c - points[i * 3 + 2].y * s;
				points[i * 3 + 2].y = points[i * 3 + 2].y * c + points[i * 3 + 2].x * s;
				points[i * 3 + 2].x = nx;
				points[i * 3 + 2] = vec2f_add(points[i * 3 + 2], shape.poly->loc);
			}
		} else for (size_t i = 0; i < shape.poly->point_count; i++) {
			points[i] = shape.poly->points[i];
			nx = points[i].x * c - points[i].y * s;
			points[i].y = points[i].y * c + points[i].x * s;
			points[i].x = nx;
			points[i] = vec2f_add(points[i], shape.poly->loc);
		}
	}
}

void __physics2_fillAxis(union physics2_shape shape, union physics2_shape shape2, vec2f* axes, size_t axes_count, int debug) {
	vec2f t;
	t.x = 0.;
	t.y = 0.;
	if (shape.poly->type == PHYSICS2_CIRCLE) {
		if (shape2.poly == NULL) {
			axes[0].x = 0.;
			axes[0].y = 0.;
		} else if (shape2.poly->type == PHYSICS2_CIRCLE) {
			axes[0] = vec2f_norm(vec2f_sub(shape.circle->loc, shape2.circle->loc));
		} else if (shape2.poly->type == PHYSICS2_POLY) {
			size_t pc = __physics2_getPointCount(shape2);
			vec2f tax[pc];
			__physics2_getAdjustedGlobalPoints(shape2, tax, t, 0);
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
	__physics2_getAdjustedGlobalPoints(shape, axes, t, 1);
	vec2f lt = axes[0];
	for (size_t j = 0; j < axes_count; j++) {
		vec2f nv = shape.poly->concave ? (j % 3 == 2 ? lt : axes[j + 1]) : lt;
		if (shape.poly->concave && j % 3 == 0) lt = axes[j];
		//
		axes[j] = debug ? vec2f_sub(axes[j], shape.poly->loc) : vec2f_norm(vec2f_perp(vec2f_sub(axes[j], shape.poly->concave ? nv : ((j + 1) == axes_count ? lt : axes[j + 1]))));
	}
}

void physics2_drawShape(union physics2_shape shape, float partialTick) {
	//glColor4f(1., 0., 1., 1.);
	//glBegin (GL_LINES);
	//glVertex2f(0., 0.);
	//glVertex2f(shape.poly->p1.x, shape.poly->p1.y);
	//glEnd();
	vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
	glPushMatrix();
	glTranslatef(iloc.x, iloc.y, 0.);
	/*glBegin(GL_LINES);
	 glColor4f(0., 1., 0., 1.);
	 glVertex2f(0., 0.);
	 vec2f xproj;
	 xproj.x = 1.;
	 xproj.y = 0.;
	 xproj = vec2f_project(shape.poly->vel, xproj);
	 vec2f yproj;
	 yproj.x = 0.;
	 yproj.y = 1.;
	 yproj = vec2f_project(shape.poly->vel, yproj);
	 //printf("%f, %f\n", xproj.x, xproj.y);
	 glVertex2f(xproj.x * 100., xproj.y * 100.);
	 glVertex2f(0., 0.);
	 glVertex2f(yproj.x * 100., yproj.y * 100.);
	 glEnd();*/
	//glColor4f(1., 0., 0., 1.);
	glRotatef(360. * physics2_getInterpolatedRotation(shape, partialTick) / (M_PI * 2.), 0., 0., 1.);
	if (shape.poly->type == PHYSICS2_CIRCLE) {
		glBegin (GL_TRIANGLE_FAN);
		glVertex2f(0., 0.);
		for (int i = PHYSICS2_CIRCLEPREC; i >= 0; i--) {
			glVertex2f((shape.poly->radius * cos((float) i * 2. * M_PI / 20.)), (shape.poly->radius * sin((float) i * 2. * M_PI / 20.)));
		}
		glEnd();
	} else if (shape.poly->type == PHYSICS2_POLY) {
		//glDisable (GL_CULL_FACE);
		if (shape.poly->concave) {
			//glBegin (GL_LINE_LOOP);
			glBegin (GL_TRIANGLES);
			for (size_t i = 0; i < shape.poly->triangle_count; i++) {
				//glColor4f(0., 0., ((float) i + 1.) / (float) shape.poly->triangle_count, 1.);
				//glColor4f(0., 0., 1., 1.);
				glVertex2f(shape.poly->triangles[i].v1.x, shape.poly->triangles[i].v1.y);
				//glColor4f(0., 1., 0., 1.);
				glVertex2f(shape.poly->triangles[i].v2.x, shape.poly->triangles[i].v2.y);
				//glColor4f(0., 1., 1., 1.);
				glVertex2f(shape.poly->triangles[i].v3.x, shape.poly->triangles[i].v3.y);
			}
			glEnd();
		} else {
			glBegin (GL_POLYGON);
			for (size_t i = 0; i < shape.poly->point_count; i++) {
				glVertex2f(shape.poly->points[i].x, shape.poly->points[i].y);
			}
			glEnd();
			/*
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
			 }*/
		}
		//glEnable(GL_CULL_FACE);

	}
	glPopMatrix();
}

/*void __physics2_drawDebug(union physics2_shape shape, float partialTick) {
 glBegin (GL_LINES);
 // glColor4f(1., 0., 1., 1.);
 //glVertex2f(0., 0.);
 // glVertex2f(shape.poly->p2.x, shape.poly->p2.y);
 //glColor4f(.0, .5, .6, 1.);
 //glVertex2f(0., 0.);
 //glVertex2f(shape.poly->p3.x, shape.poly->p3.y);
 glColor4f(0., 1., 1., 1.);
 glVertex2f(0., 0.);
 glVertex2f(shape.poly->p1.x, shape.poly->p1.y);
 glEnd();
 vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
 glPushMatrix();
 glTranslatef(iloc.x, iloc.y, 0.);
 //glRotatef(-(physics2_getInterpolatedRotation(shape, partialTick) / (M_PI * 2)) * 360., 0., 0., 1.);
 glColor4f(0., 1., 0., 1.);
 glBegin (GL_LINES);
 glColor4f(0., 1., 0., 1.);
 glVertex2f(0., 0.);
 vec2f xproj;
 xproj.x = 1.;
 xproj.y = 0.;
 xproj = vec2f_project(shape.poly->vel, xproj);
 vec2f yproj;
 yproj.x = 0.;
 yproj.y = 1.;
 yproj = vec2f_project(shape.poly->vel, yproj);
 //printf("%f, %f\n", xproj.x, xproj.y);
 glVertex2f(xproj.x * 100., xproj.y * 100.);
 glVertex2f(0., 0.);
 glVertex2f(yproj.x * 100., yproj.y * 100.);
 glEnd();
 //glRotatef(360. * physics2_getInterpolatedRotation(shape, partialTick) / (M_PI * 2.), 0., 0., 1.);
 size_t ac = 0;
 if (shape.poly->type == PHYSICS2_CIRCLE) {
 ac = 1;
 } else if (shape.poly->type == PHYSICS2_POLY) {
 ac = shape.poly->concave ? shape.poly->triangle_count * 3 : shape.poly->point_count;
 }
 vec2f vecs[ac];
 union physics2_shape shape2;
 shape2.circle = NULL;
 if (ac > 0) __physics2_fillAxis(shape, shape2, vecs, ac, 0);
 glBegin (GL_LINES);
 //glVertex2f(-shape.poly->loc.x, -shape.poly->loc.y);
 //glVertex2f(-shape.poly->loc.x + shape.poly->p1.x, -shape.poly->loc.y + shape.poly->p1.y);
 for (int i = 0; i < ac; i++) {
 glVertex2f(0., 0.);
 glVertex2f(vecs[i].x * 100., vecs[i].y * 100.);
 }
 glEnd();
 glPopMatrix();
 glColor4f(1., 0., 0., 1.);
 }*/

void physics2_drawAllShapes(struct physics2_ctx* ctx, float x1, float y1, float x2, float y2, float partialTick) {
	for (size_t i = 0; i < ctx->shape_count; i++) {
		union physics2_shape shape = ctx->shapes[i];
		vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
		//printf("%f, %f, %f, %f, %f, %f, %f\n", iloc.x, iloc.y, x1, y1, x2, y2, shape.poly->radius);
		if (iloc.x - x1 > -shape.poly->radius && iloc.x - x2 < shape.poly->radius && iloc.y - y1 > -shape.poly->radius && iloc.y - y2 < shape.poly->radius) {
			physics2_drawShape(ctx->shapes[i], partialTick);
		}
	}
	/*for (size_t i = 0; i < ctx->shape_count; i++) {
	 union physics2_shape shape = ctx->shapes[i];
	 vec2f iloc = physics2_getInterpolatedPosition(shape, partialTick);
	 //printf("%f, %f, %f, %f, %f, %f, %f\n", iloc.x, iloc.y, x1, y1, x2, y2, shape.poly->radius);
	 if (iloc.x - x1 > -shape.poly->radius && iloc.x - x2 < shape.poly->radius && iloc.y - y1 > -shape.poly->radius && iloc.y - y2 < shape.poly->radius) {
	 //printf("dun\n");
	 __physics2_drawDebug(ctx->shapes[i], partialTick);
	 }
	 }*/
}

void physics2_setMassByArea(union physics2_shape shape, float multiplier) {
	float area = 0.;
	if (shape.poly->type == PHYSICS2_CIRCLE) {
		area = M_PI * shape.circle->radius * shape.circle->radius;
	} else if (shape.poly->type == PHYSICS2_POLY) {
		for (size_t i = 0; i < shape.poly->point_count; i++) {
			size_t i2 = (i + 1) == shape.poly->point_count ? 0 : i + 1;
			area += shape.poly->points[i].x * shape.poly->points[i2].y - shape.poly->points[i].y * shape.poly->points[i2].x;
		}
		if (area < 0.) area *= -1.;
		area /= 2.;
	}
	area *= multiplier;
	shape.poly->mass = area;
}

void physics2_applyForce(union physics2_shape shape, vec2f force, vec2f loc) {
	shape.poly->vel.x += force.x / shape.poly->mass;
	shape.poly->vel.y += force.y / shape.poly->mass;
	shape.poly->rps += vec2f_cross(loc, force, 0.).z / shape.poly->moi;
}

vec2f physics2_getInterpolatedPosition(union physics2_shape shape, float partialTick) {
	vec2f v;
	v.x = shape.poly->loc.x * (1. - partialTick) + shape.poly->ploc.x * partialTick;
	v.y = shape.poly->loc.y * (1. - partialTick) + shape.poly->ploc.y * partialTick;
	return v;
}

float physics2_getInterpolatedRotation(union physics2_shape shape, float partialTick) {
	return shape.poly->rot * (1. - partialTick) + shape.poly->lrot * partialTick;
}

struct __physics2_projdata {
		vec2f axis;
		float eox;
};

union __physics2_branchprojdata {
		struct __physics2_projdata data;
		struct __physics2_projectionctx* datum;
};

struct __physics2_projectionctx {
		int mos;
		float mo;
		vec2f smallest_axis;
		union __physics2_branchprojdata* buf; // precalced
		size_t bufi;
		float eox;
};

vec2f __physics2_project(vec2f axis, vec2f* points, size_t point_count) { // x = min, y = max
	vec2f r;
	r.x = 0.;
	r.y = 0.;
	for (size_t i = 0; i < point_count; i++) {
		float d = vec2f_dot(axis, points[i]);
		if (i == 0) {
			r.x = d;
			r.y = d;
		} else if (d < r.x) {
			r.x = d;
		} else if (d > r.y) {
			r.y = d;
		}
	}
	return r;
}

vec2f __physics2_getCollisionPoint(vec2f mtv, union physics2_shape shape, union physics2_shape shape2, struct __physics2_triangle* t1, struct __physics2_triangle* t2) {
	vec2f support[2];
	int s2 = 0;
	vec2f support2[2];
	int s22 = 0;
	vec2f nmtv = vec2f_norm(mtv);
	{
		size_t pc = t1 != NULL ? 0 : __physics2_getPointCount(shape);
		vec2f tax[pc];
		if (t1 == NULL) __physics2_getAdjustedGlobalPoints(shape, tax, nmtv, 0); // axis might be better than mtv?
		vec2f* tx2 = tax;
		if (t1 != NULL) {
			tx2 = t1;
			pc = 3;
		}
		float mind = 0.;
		for (size_t i = 0; i < pc; i++) {
			float d = vec2f_dot(tx2[i], nmtv);
			//printf("s1-%i %f from %f, %f\n", i, d, tx2[i].x, tx2[i].y);
			if (i == 0 || d < mind) {
				mind = d;
			}
		}
		for (size_t i = 0; i < pc; i++) {
			float d = vec2f_dot(tx2[i], nmtv);
			if (d < (mind + 1.0E-8)) {
				support[s2 ? 1 : 0] = tx2[i];
				if (s2++) break;
			}
		}
	}
	{
		size_t pc = t2 != NULL ? 0 : __physics2_getPointCount(shape2);
		vec2f tax[pc];
		if (t2 == NULL) __physics2_getAdjustedGlobalPoints(shape2, tax, nmtv, 0);
		vec2f* tx2 = tax;
		if (t2 != NULL) {
			tx2 = t2;
			pc = 3;
		}
		float mind = 0.;
		for (size_t i = 0; i < pc; i++) {
			float d = vec2f_dot(tx2[i], vec2f_scale(nmtv, -1.));
			//printf("s2-%i %f from %f, %f\n", i, d, tx2[i].x, tx2[i].y);
			if (i == 0 || d < mind) {
				mind = d;
			}
		}
		for (size_t i = 0; i < pc; i++) {
			float d = vec2f_dot(tx2[i], vec2f_scale(nmtv, -1.)); // maybe 1 instead
			if (d < (mind + 1.0E-8)) {
				support2[s22 ? 1 : 0] = tx2[i];
				if (s22++) break;
			}
		}
	}
	//printf("s1 = %i, s2 = %i\n", s2, s22);
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
	if (shape.poly->type == PHYSICS2_CIRCLE) shape.circle->moi = shape.circle->mass * shape.circle->radius * shape.circle->radius / 2.;
	else if (shape.poly->type == PHYSICS2_POLY) {
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

int __physics2_fillProjection(struct __physics2_projectionctx* ctx, vec2f axis, vec2f* points, size_t point_count, vec2f* points2, size_t point2_count) {
	vec2f v1 = __physics2_project(axis, points, point_count);
	vec2f v2 = __physics2_project(axis, points2, point2_count);
	float eox = 0.;
	if (v1.x > v2.x && v1.x < v2.y) {
		eox = v1.y > v2.y ? v2.y : v1.y;
		eox -= v1.x;
	} else if (v1.y > v2.x && v1.y < v2.y) {
		eox = v1.y;
		eox -= v1.x < v2.x ? v2.x : v1.x;
	} else if (v1.x > v2.x && v1.y < v2.y) {
		eox = v1.y - v1.x;
	} else if (v2.x > v1.x && v2.y < v1.y) {
		eox = v2.y - v2.x;
	}
	ctx->buf[ctx->bufi].data.axis = axis;
	ctx->buf[ctx->bufi++].data.eox = eox;
	return eox > 0.;
}

int __physics2_finishProjection(struct __physics2_projectionctx* ctx, size_t axis1_count, size_t axis2_count, union physics2_shape shape, union physics2_shape shape2, vec2f* points1, vec2f* points2, struct __physics2_triangle** p1, struct __physics2_triangle** p2) {
	int cc = shape.poly->type == PHYSICS2_POLY && shape.poly->concave;
	int cc2 = shape2.poly->type == PHYSICS2_POLY && shape2.poly->concave;
	if (cc && cc2) {
		for (size_t l = 0; l < ctx->bufi; l++) {
			union __physics2_branchprojdata* pdx2 = ctx->buf + l;
			for (size_t j = 0; j < pdx2->datum->bufi; j++) {
				union __physics2_branchprojdata* pdx = pdx2->datum->buf + j;
				int g = 1;
				for (size_t i = 0; i < pdx->datum->bufi; i++) {
					union __physics2_branchprojdata* pd = pdx->datum->buf + i;
					if (pd->data.eox == 0.) {
						ctx->mos = 0;
						g = 0;
						break;
					}
					if (!ctx->mos || pd->data.eox < ctx->mo) {
						ctx->mo = pd->data.eox;
						ctx->smallest_axis = pd->data.axis;
						ctx->mos = 1;
						ctx->eox = pd->data.eox;
					}
				}
				if (g) {
					*p1 = points1 + l * 3;
					*p2 = points2 + j * 3;
					return 1;
				}
			}
		}
		return 0;
	} else if (cc2 != cc) {
		for (size_t l = 0; l < ctx->bufi; l++) {
			union __physics2_branchprojdata* pdx = ctx->buf + l;
			int g = 1;
			//printf("%f, %f; %f, %f; %f, %f\n", shape.poly->triangles[l].v1.x, shape.poly->triangles[l].v1.y, shape.poly->triangles[l].v2.x, shape.poly->triangles[l].v2.y, shape.poly->triangles[l].v3.x, shape.poly->triangles[l].v3.y);
			for (size_t i = 0; i < pdx->datum->bufi; i++) {
				union __physics2_branchprojdata* pd = pdx->datum->buf + i;
				if (pd->data.eox == 0.) {
					ctx->mos = 0;
					g = 0;
					break;
				}
				if (!ctx->mos || pd->data.eox < ctx->mo) {
					ctx->mo = pd->data.eox;
					ctx->smallest_axis = pd->data.axis;
					ctx->mos = 1;
					ctx->eox = pd->data.eox;
				}
			}
			if (g) {
				if (cc) *p1 = points1 + l * 3;
				else *p2 = points2 + l * 3;
				return 1;
			}
		}
		return 0;
	} else {
		for (size_t i = 0; i < ctx->bufi; i++) {
			union __physics2_branchprojdata* pd = ctx->buf + i;
			if (pd->data.eox == 0.) return 0;
			if (!ctx->mos || pd->data.eox < ctx->mo) {
				ctx->mo = pd->data.eox;
				ctx->smallest_axis = pd->data.axis;
				ctx->mos = 1;
				ctx->eox = pd->data.eox;
			}
		}
	}
	return 1;
}

void physics2_simulate(struct physics2_ctx* ctx) {
	for (size_t i = 0; i < ctx->shape_count; i++) {
		union physics2_shape shape = ctx->shapes[i];
		shape.poly->ploc.x = shape.poly->loc.x;
		shape.poly->ploc.y = shape.poly->loc.y;
		shape.poly->loc.x += shape.poly->vel.x;
		shape.poly->loc.y += shape.poly->vel.y;
		shape.poly->vel.x += shape.poly->constant_accel.x;
		shape.poly->vel.y += shape.poly->constant_accel.y;
		shape.poly->vel.x += ctx->constant_accel.x;
		shape.poly->vel.y += ctx->constant_accel.y;
		shape.poly->vel.x *= 1. - shape.poly->drag;
		shape.poly->vel.y *= 1. - shape.poly->drag;
		shape.poly->lrot = shape.poly->rot;
		shape.poly->rot += shape.poly->rps;
		shape.poly->rps *= 1. - shape.poly->rdrag;
		for (size_t x = i + 1; x < ctx->shape_count; x++) { // todo quad trees or something
			union physics2_shape shape2 = ctx->shapes[x];
			if (shape.poly->loc.x + shape.poly->radius < shape2.poly->loc.x - shape2.poly->radius || shape.poly->loc.x - shape.poly->radius > shape2.poly->loc.x + shape2.poly->radius || shape.poly->loc.y + shape.poly->radius < shape2.poly->loc.y - shape2.poly->radius || shape.poly->loc.y - shape.poly->radius > shape2.poly->loc.y + shape2.poly->radius) continue;
			size_t axes1_count = 0;
			size_t axes2_count = 0;
			if (shape.poly->type == PHYSICS2_CIRCLE) {
				axes1_count = 1;
			} else if (shape.poly->type == PHYSICS2_POLY) {
				axes1_count = shape.poly->concave ? shape.poly->triangle_count * 3 : shape.poly->point_count;
			}
			if (shape2.poly->type == PHYSICS2_CIRCLE) {
				axes2_count = shape.poly->type == PHYSICS2_CIRCLE ? 0 : 1;
			} else if (shape2.poly->type == PHYSICS2_POLY) {
				axes2_count = shape2.poly->concave ? shape2.poly->triangle_count * 3 : shape2.poly->point_count;
			}
			vec2f axes1[axes1_count];
			if (axes1_count > 0) __physics2_fillAxis(shape, shape2, axes1, axes1_count, 0);
			vec2f axes2[axes2_count];
			if (axes2_count > 0) __physics2_fillAxis(shape2, shape, axes2, axes2_count, 0);
			size_t points1_count = shape.poly->type == PHYSICS2_POLY ? axes1_count : 2;
			vec2f points1[points1_count];
			vec2f t0;
			t0.x = 0.;
			t0.y = 0.;
			__physics2_getAdjustedGlobalPoints(shape, points1, t0, 1);
			size_t points2_count = shape2.poly->type == PHYSICS2_POLY ? axes2_count : 2;
			vec2f points2[points2_count];
			__physics2_getAdjustedGlobalPoints(shape2, points2, t0, 1);
			struct __physics2_projectionctx pctx;
			pctx.mo = 0.;
			pctx.mos = 0;
			pctx.bufi = 0;
			int cc = shape.poly->type == PHYSICS2_POLY && shape.poly->concave;
			int cc2 = shape2.poly->type == PHYSICS2_POLY && shape2.poly->concave;
			pctx.buf = smalloc(sizeof(union __physics2_branchprojdata) * (cc ? shape.poly->triangle_count : (cc2 ? shape2.poly->triangle_count : (axes1_count + axes2_count))));
			if (cc) {
				for (size_t k = 0; k < shape.poly->triangle_count; k++) {
					struct __physics2_projectionctx* pj1 = smalloc(sizeof(struct __physics2_projectionctx));
					pctx.buf[pctx.bufi++].datum = pj1;
					pj1->buf = smalloc(sizeof(union __physics2_branchprojdata) * (cc2 ? shape2.poly->triangle_count : (axes1_count + axes2_count)));
					pj1->bufi = 0;
					pj1->mo = 0.;
					pj1->mos = 0;
					if (!cc2) {
						for (size_t j = k * 3; j < (k + 1) * 3; j++)
							__physics2_fillProjection(pj1, axes1[j], points1 + k * 3, 3, points2, points2_count);
						for (size_t j = 0; j < axes2_count; j++)
							__physics2_fillProjection(pj1, axes2[j], points1 + k * 3, 3, points2, points2_count);
					} else {
						for (size_t l = 0; l < shape2.poly->triangle_count; l++) {
							struct __physics2_projectionctx* pj2 = smalloc(sizeof(struct __physics2_projectionctx));
							pj1->buf[pj1->bufi++].datum = pj2;
							pj2->buf = smalloc(sizeof(union __physics2_branchprojdata) * (axes1_count + axes2_count));
							pj2->bufi = 0;
							pj2->mo = 0.;
							pj2->mos = 0;
							for (size_t j = k * 3; j < (k + 1) * 3; j++)
								__physics2_fillProjection(pj2, axes1[j], points1 + k * 3, 3, points2 + l * 3 + l, 3);
							for (size_t j = l * 3; j < (l + 1) * 3; j++)
								__physics2_fillProjection(pj2, axes2[j], points1 + k * 3, 3, points2 + l * 3, 3);
						}
					}
				}
			} else if (cc2) {
				for (size_t l = 0; l < shape2.poly->triangle_count; l++) {
					struct __physics2_projectionctx* pj1 = smalloc(sizeof(struct __physics2_projectionctx));
					pctx.buf[pctx.bufi++].datum = pj1;
					pj1->buf = smalloc(sizeof(union __physics2_branchprojdata) * (axes1_count + axes2_count));
					pj1->bufi = 0;
					pj1->mo = 0.;
					pj1->mos = 0;
					for (size_t j = 0; j < axes1_count; j++)
						__physics2_fillProjection(pj1, axes1[j], points1, points1_count, points2 + l * 3, 3);
					for (size_t j = l * 3; j < (l + 1) * 3; j++)
						__physics2_fillProjection(pj1, axes2[j], points1, points1_count, points2 + l * 3, 3);
				}
			} else {
				for (size_t j = 0; j < axes1_count; j++)
					if (!__physics2_fillProjection(&pctx, axes1[j], points1, points1_count, points2, points2_count)) {
						free(pctx.buf);
						goto scont;
					}
				for (size_t j = 0; j < axes2_count; j++)
					if (!__physics2_fillProjection(&pctx, axes2[j], points1, points1_count, points2, points2_count)) {
						free(pctx.buf);
						goto scont;
					}
			}
			struct __physics2_triangle* tr1 = NULL;
			struct __physics2_triangle* tr2 = NULL;
			if (!__physics2_finishProjection(&pctx, axes1_count, axes2_count, shape, shape2, points1, points2, &tr1, &tr2)) {
				free(pctx.buf);
				goto scont;
			}
			free(pctx.buf);
			if (pctx.mos) {
				if (vec2f_dot(pctx.smallest_axis, vec2f_sub(shape2.poly->loc, shape.poly->loc)) > 0.) {
					//printf("invert iscircle = %i\n", shape.poly->type == PHYSICS2_CIRCLE);
					pctx.smallest_axis = vec2f_scale(pctx.smallest_axis, -1.);
				}
				vec2f normal;
				normal.x = pctx.smallest_axis.x;
				normal.y = pctx.smallest_axis.y;
				//printf("mo = %f  --- %f, %f\n", mo, normal.x, normal.y);
				pctx.smallest_axis = vec2f_scale(pctx.smallest_axis, pctx.mo);
				shape2.poly->loc = vec2f_sub(shape2.poly->loc, pctx.smallest_axis);
				vec2f avg = __physics2_getCollisionPoint(pctx.smallest_axis, shape, shape2, tr1, tr2);
				/*
				 if (shape.poly->type == PHYSICS2_CIRCLE) {
				 avg = cpmax1;
				 } else if (shape2.poly->type == PHYSICS2_CIRCLE) {
				 avg = cpmax2;
				 } else {
				 avg.x = (cpmax2.x - cpmax1.x) / 2. + shape.poly->loc.x;
				 avg.y = (cpmax2.y - cpmax1.y) / 2. + shape.poly->loc.y;
				 }*/
				//shape.poly->p1.x = avg.x;						//shape.poly->loc.x + smallest_axis.x * 100.; // avg.x; //(cpmax2.x - cpmax1.x) / 2. + shape.poly->loc.x;
				//shape.poly->p1.y = avg.y;						//shape.poly->loc.y + smallest_axis.y * 100.; // avg.y; //(cpmax2.y - cpmax1.y) / 2. + shape.poly->loc.y;
				//shape.poly->p2.x = cpmax2.x;
				//shape.poly->p2.y = cpmax2.y;
				//(a == 1 ? shape : shape2).rect->p2.x = ai;
				//shape.poly->p3.x = cpmax1.x + shape.poly->loc.x * 2.;
				//shape.poly->p3.y = -cpmax1.y + shape.poly->loc.y * 2.;
				/*shape.poly->p1.x = cpmin1.x;
				 shape.poly->p1.y = cpmin1.y;
				 shape.poly->p2.x = cpmax1.x;
				 shape.poly->p2.y = cpmax1.y;
				 shape2.poly->p1.x = -cpmin2.x;
				 shape2.poly->p1.y = -cpmin2.y;
				 shape2.poly->p2.x = -cpmax2.x;
				 shape2.poly->p2.y = -cpmax2.y;*/
				//printf("<%f, %f> - %f, %f\n", shape.poly->loc.y + shape.poly->height / 2., shape2.poly->loc.y - shape2.poly->height / 2., smallest_axis.x, smallest_axis.y);
				//
				vec2f rap = vec2f_sub(avg, shape.poly->loc);
				vec2f rbp = vec2f_sub(avg, shape2.poly->loc);
				vec3f pt;
				pt.x = rap.x;
				pt.y = rap.y;
				pt.z = 0.;
				vec3f tmp;
				tmp.z = shape.poly->rps;
				vec2f tx;
				tmp = vec3f_cross(tmp, pt);
				tx.x = tmp.x;
				tx.y = tmp.y;
				vec2f vap1 = vec2f_add(shape.poly->vel, tx);
				tmp.z = shape2.poly->rps;
				pt.x = rbp.x;
				pt.y = rbp.y;
				pt.z = 0.;
				tmp = vec3f_cross(tmp, pt);
				tx.x = tmp.x;
				tx.y = tmp.y;
				vec2f vbp1 = vec2f_add(shape2.poly->vel, tx);
				vec2f vab1 = vec2f_sub(vap1, vbp1);
				float j = vec2f_dot(vec2f_scale(vab1, -(1 + (shape2.poly->elasticity > shape.poly->elasticity ? shape2.poly->elasticity : shape.poly->elasticity))), normal);
				float dj = (1. / shape.poly->mass) + (1. / shape2.poly->mass);
				vec3f t1;
				t1.x = rap.x;
				t1.y = rap.y;
				t1.z = 0.;
				vec3f t2;
				t2.x = normal.x;
				t2.y = normal.y;
				t2.z = 0.;
				vec3f dt = vec3f_cross(t1, t2);
				if (shape.poly->moi > 0.) {
					float sd = vec3f_dot(dt, dt);
					dj += sd / shape.poly->moi;
				}
				t1.x = rbp.x;
				t1.y = rbp.y;
				t1.z = 0.;
				dt = vec3f_cross(t1, t2);
				if (shape2.poly->moi > 0.) {
					float sd = vec3f_dot(dt, dt);
					dj += sd / shape2.poly->moi;
				}
				//printf("dj = %f\n", j);
				j /= dj;
				//printf("pj = %f\n", j);
				//printf("1 %f: %f, %f; %f, %f\n", j, shape.poly->vel.x, shape.poly->vel.y, shape2.poly->vel.x, shape2.poly->vel.y);
				shape.poly->vel = vec2f_add(shape.poly->vel, vec2f_scale(normal, j / shape.poly->mass));
				shape2.poly->vel = vec2f_sub(shape2.poly->vel, vec2f_scale(normal, j / shape2.poly->mass));
				float frict = (shape2.poly->friction + shape.poly->friction) / 2.;
				vec2f force = vec2f_scale(vec2f_project(shape.poly->vel, vec2f_perp(normal)), -frict * shape.poly->mass);
				physics2_applyForce(shape, force, avg);
				vec2f force2 = vec2f_scale(vec2f_project(shape2.poly->vel, vec2f_perp(normal)), -frict * shape2.poly->mass);
				physics2_applyForce(shape2, force2, avg);
				//shape.poly->vel = vec2f_add(vec2f_scale(vec2f_project(shape.poly->vel, vec2f_perp(normal)), frict), vec2f_project(shape.poly->vel, normal));
				//shape2.poly->vel = vec2f_add(vec2f_scale(vec2f_project(shape2.poly->vel, vec2f_perp(normal)), frict), vec2f_project(shape2.poly->vel, normal));
				//printf("2 %f: %f, %f; %f, %f\n", j, shape.poly->vel.x, shape.poly->vel.y, shape2.poly->vel.x, shape2.poly->vel.y);
				//printf("s1 moi = %f, s2 moi = %f\n", shape.poly->moi, shape2.poly->moi);
				if (shape.poly->moi > 0.) {
					t1.x = rap.x;
					t1.y = rap.y;
					t1.z = 0.;
					vec3f rx = vec3f_cross(t1, vec3f_scale(t2, j));
					shape.poly->rps += rx.z / shape.poly->moi;
					//printf("sr1ps type = %i, rx.z = %f, moi = %f    ---   %f, %f\n", shape.poly->type, rx.z, shape.poly->moi, rap.x, rap.y);
//printf("s1rps type = %i, rpsd = %f, trps = %f\n", shape.poly->type, -rx.z / shape.poly->moi, shape.poly->rps);
				}
				if (shape2.poly->moi > 0.) {
					t1.x = rbp.x;
					t1.y = rbp.y;
					t1.z = 0.;
					vec3f rx = vec3f_cross(t1, vec3f_scale(t2, j));
					shape2.poly->rps -= rx.z / shape2.poly->moi;
					//printf("sr2ps type = %i, rx.z = %f, moi = %f    ---   %f, %f\n", shape2.poly->type, rx.z, shape2.poly->moi, rbp.x, rbp.y);
					//printf("s2rps type = %i, rpsd = %f, trps = %f\n", shape2.poly->type, rx.z / shape2.poly->moi, shape2.poly->rps);
				}
				//shape2.poly->vel.x = 0.;
				//shape2.poly->vel.y = 0.;
				//shape.poly->vel.x = 0.;
				//shape.poly->vel.y = 0.;
			}
			//printf("collide <%f, %f> %f, %f vs <%f, %f> %f, %f\n", shape.poly->width, shape.poly->height, shape.poly->loc.x, shape.poly->loc.y, shape2.poly->width, shape2.poly->height, shape2.poly->loc.x, shape2.poly->loc.y);
			continue;
			scont: ;
		}
	}
}

