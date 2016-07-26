/*
 * render.h
 *
 *  Created on: Feb 23, 2016
 *      Author: root
 */

#ifndef RENDER_H_
#define RENDER_H_

#include <stdlib.h>
#include <stdint.h>

#define VERTEXTYPE_2 0
#define VERTEXTYPE_3 1
#define VERTEXTYPE_TEX2 2
#define VERTEXTYPE_TEX3 3

struct __attribute__((packed)) vertex2 {
		float x;
		float y;
};

struct __attribute__((packed)) vertex3 {
		float x;
		float y;
		float z;
};

struct __attribute__((packed)) vertex2_tex {
		float x;
		float y;
		float tx;
		float ty;
};

struct __attribute__((packed)) vertex3_tex {
		float x;
		float y;
		float z;
		float tx;
		float ty;
};

//TODO: normals

union uvertex {
		struct vertex2 vertex2;
		struct vertex3 vertex3;
		struct vertex2_tex vertex2_tex;
		struct vertex3_tex vertex3_tex;
};

struct vao {
		int vao;
		int vbo;
		size_t vertex_count;
		uint8_t type;
};

void virtVertex2f(union uvertex* vert, float x, float y);

void virtVertex3f(union uvertex* vert, float x, float y, float z);

void virt2TexCoord2f(struct vertex2_tex* vert, float tx, float ty);

void virt3TexCoord2f(struct vertex3_tex* vert, float tx, float ty);

void createVAO(union uvertex* verticies, size_t count, uint8_t type, struct vao* vao, int overwrite);

void deleteVAO(struct vao* vao);

void drawTriangles(struct vao* vao);

void drawQuads(struct vao* vao);

int loadTexturePNG(char* path, int id, int wrap, int filter);

void loadTextureData(int id, size_t width, size_t height, void* data, int wrap, int filter);

#endif /* RENDER_H_ */
