/*
 * render.c
 *
 *  Created on: Feb 23, 2016
 *      Author: root
 */

#include "render.h"
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include "xstring.h"
#include <math.h>
#include <time.h>
#include <png.h>

void virtVertex2f(union uvertex* vert, float x, float y) {
	vert->vertex2.x = x;
	vert->vertex2.y = y;
}

void virtVertex3f(union uvertex* vert, float x, float y, float z) {
	vert->vertex3.x = x;
	vert->vertex3.y = y;
	vert->vertex3.z = z;
}

void virt2TexCoord2f(struct vertex2_tex* vert, float tx, float ty) {
	vert->tx = tx;
	vert->ty = ty;
}

void virt3TexCoord2f(struct vertex3_tex* vert, float tx, float ty) {
	vert->tx = tx;
	vert->ty = ty;
}

void createVAO(union uvertex* verticies, size_t count, uint8_t type, struct vao* vao, int overwrite) {
	size_t vs = 0;
	if (type == VERTEXTYPE_2) vs = sizeof(struct vertex2);
	else if (type == VERTEXTYPE_3) vs = sizeof(struct vertex3);
	else if (type == VERTEXTYPE_TEX2) vs = sizeof(struct vertex2_tex);
	else if (type == VERTEXTYPE_TEX3) vs = sizeof(struct vertex3_tex);
	else return;
	vao->type = type;
	if (!overwrite) glGenVertexArrays(1, &vao->vao);
	glBindVertexArray(vao->vao);
	if (!overwrite) {
		glGenBuffers(1, &vao->vbo);
	}
	glBindBuffer(GL_ARRAY_BUFFER, vao->vbo);
	glBufferData(GL_ARRAY_BUFFER, count * vs, verticies, GL_STATIC_DRAW);
	vao->vertex_count = count;
	glVertexPointer((type == VERTEXTYPE_2 || type == VERTEXTYPE_TEX2) ? 2 : 3, GL_FLOAT, vs, 0);
	glEnableClientState (GL_VERTEX_ARRAY);
	if (type == VERTEXTYPE_TEX2 || type == VERTEXTYPE_TEX3) {
		glTexCoordPointer(2, GL_FLOAT, vs, type == VERTEXTYPE_TEX2 ? sizeof(struct vertex2) : sizeof(struct vertex3));
		glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	}
}

void deleteVAO(struct vao* vao) {
	glDeleteBuffers(1, &vao->vbo);
	glDeleteVertexArrays(1, &vao->vao);
}

void drawTriangles(struct vao* vao) {
	if (vao->vertex_count % 3 != 0) return;
	glBindVertexArray(vao->vao);
	glDrawArrays(GL_TRIANGLES, 0, vao->vertex_count);
}

void drawQuads(struct vao* vao) {
	if (vao->vertex_count % 4 != 0) return;
	glBindVertexArray(vao->vao);
	glDrawArrays(GL_QUADS, 0, vao->vertex_count);
}

int loadTexturePNG(char* path, int id, int wrap, int filter) {
	FILE* fd = fopen(path, "rb");
	if (!fd) return -1;
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) {
		fclose(fd);
		return -1;
	}
	png_infop info = png_create_info_struct(png);
	if (!info) {
		fclose(fd);
		return -1;
	}
	png_init_io(png, fd);
	png_read_info(png, info);
	int width = png_get_image_width(png, info);
	int height = png_get_image_height(png, info);
	png_byte color_type = png_get_color_type(png, info);
	png_byte bit_depth = png_get_bit_depth(png, info);
	if (bit_depth == 16) png_set_strip_16(png);
	if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
	if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
	if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
	png_read_update_info(png, info);
	void* pngd = malloc(height * png_get_rowbytes(png, info));
	png_bytep* row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
	for (int y = 0; y < height; y++) {
		row_pointers[y] = (png_byte*) (pngd + (png_get_rowbytes(png, info) * y));
	}
	png_read_image(png, row_pointers);
	free(row_pointers);
	loadTextureData(id, width, height, pngd, wrap, filter);
	free(pngd);
	png_destroy_read_struct(&png, &info, NULL);
	fclose(fd);
	return 0;
}

void loadTextureData(int id, size_t width, size_t height, void* data, int wrap, int filter) {
	glBindTexture(GL_TEXTURE_2D, id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
#ifdef GL_TEXTURE_MAX_ANISOTROPY_EXT
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4.0);
#endif
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

