/*
 * font.c
 *
 *  Created on: Jul 25, 2016
 *      Author: root
 */

#include "font.h"
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <png.h>
#include "globals.h"
#include "render.h"
#include <fcntl.h>
#include "streams.h"
#include <errno.h>

unsigned char fontWidth[256];
int cfont = -1;

struct __attribute__((__packed__)) rpix {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
};

void setFont(int tx) {
	cfont = tx;
}

int loadFont(char* fontpath, char* sizepath, int tx) {
	FILE* fd = fopen(fontpath, "rb");
	if (!fd) return 1;
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) {
		fclose(fd);
		return 1;
	}
	png_infop info = png_create_info_struct(png);
	if (!info) {
		fclose(fd);
		return 1;
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
	/*struct rpix* v4 = pngd;
	 int cw = width / 16;
	 int ch = height / 16;
	 for (int i = 0; i < 256; i++) {
	 int v10 = i % 16;
	 int v11 = i / 16;
	 if (i == 32) {
	 fontWidth[i] = 4;
	 continue;
	 }
	 int v12 = cw - 1;
	 while (1) {
	 if (v12 >= 0) {
	 int v13 = v10 * cw + v12;
	 int v14 = 1;
	 for (int v15 = 0; (v15 < ch) && v14; v15++) {
	 int v16 = (v11 * cw + v15) * width;
	 if (v4[v13 + v16].a > 0) v14 = 0;
	 }
	 if (v14) {
	 --v12;
	 continue;
	 }
	 }
	 v12++;
	 fontWidth[i] = (unsigned char) (0.5 + ((float) v12 * (8. / cw))) + 1;
	 break;
	 }
	 }*/
	loadTextureData(tx, width, height, pngd, GL_REPEAT, GL_NEAREST);
	free(pngd);
	png_destroy_read_struct(&png, &info, NULL);
	fclose(fd);
	int fd2 = open(sizepath, O_RDONLY);
	if (fd2 < 0) {
		return -1;
	}
	int r = read(fd2, fontWidth, 256);
	if (r < 256) {
		close(fd2);
		return -1;
	}
	close(fd2);
	return 0;
}

void drawChar(char c, int italic) {
	float v3 = c % 16 * 8;
	float v4 = c / 16 * 8;
	float v5 = italic ? 1. : 0.;
	float v6 = fontWidth[c] - .01;
	glBindTexture(GL_TEXTURE_2D, cfont);
	glBegin (GL_TRIANGLE_STRIP);
	glTexCoord2f(v3 / 128., (v4 + 0.1) / 128.);
	glVertex3f(v5, 0., 0.);
	glTexCoord2f(v3 / 128., (v4 + 7.99 - 0.2) / 128.);
	glVertex3f(-v5, 7.99, 0.);
	glTexCoord2f((v3 + v6 - 1.) / 128., (v4 + 0.1) / 128.);
	glVertex3f(v6 - 1. + v5, 0., 0.);
	glTexCoord2f((v3 + v6 - 1.) / 128., (v4 + 7.99 - 0.2) / 128.);
	glVertex3f(v6 - 1. - v5, 7.99, 0.0);
	glEnd();
}

float drawChar2(char c, int italic) {
	if (c == ' ') return 4.;
	glBindTexture(GL_TEXTURE_2D, cfont);
	unsigned char cwid = fontWidth[c];
	if (cwid == 0) return 0.;
	float k = (float) (cwid >> 4);
	float l = (float) (cwid & 15) + 1.;
	float j = (float) (c % 16 * 16) + k;
	float m = (float) ((c & 255) / 16 * 16);
	float n = l - k - .02;
	float im = italic ? 1. : 0.;
	glBegin (GL_TRIANGLE_STRIP);
	glTexCoord2f(j / 256., m / 256.);
	glVertex3f(im, 0., 0.);
	glTexCoord2f(j / 256., (m + 15.98) / 256.);
	glVertex3f(-im, 7.9, 0.);
	glTexCoord2f((j + n) / 256., m / 256.);
	glVertex3f(n / 2. + im, 0., 0.);
	glTexCoord2f((j + n) / 256., (m + 15.98) / 256.);
	glVertex3f(n / 2. - im, 7.99, 0.);
	glEnd();
	return (l - k) / 2. + 1.;
}

void drawString(char* str, float x, float y, uint32_t color) {
	size_t sl = strlen(str);
	//glDisable (GL_DEPTH_TEST);
	glPushMatrix();
	glTranslatef(x, y, 0.);
	//glPushMatrix();
	glTranslatef(1., 1., 0.);
	glColor4f(((color >> 16) & 0xff) / 255., ((color >> 8) & 0xff) / 255., ((color) & 0xff) / 255., 1.);
	for (size_t i = 0; i < sl; i++) {
		glTranslatef(drawChar2(str[i], 0), 0., 0.);
	}
	//glPopMatrix();
	//glColor4f(((color >> 16) & 0xff) / 255., ((color >> 8) & 0xff) / 255., ((color) & 0xff) / 255., 1.);
	//for (size_t i = 0; i < sl; i++) {
	//glTranslatef(drawChar2(str[i], 0), 0., 0.);
	//}
	glPopMatrix();
	//glEnable(GL_DEPTH_TEST);
}

size_t stringWidth(char* str) {
	size_t sx = 0;
	size_t sl = strlen(str);
	for (size_t i = 0; i < sl; i++) {
		int cwid = fontWidth[str[i]];
		float k = (float) (cwid >> 4);
		float l = (float) (cwid & 15) + 1.;
		sx += (l - k) / 2. + 1.;
	}
	return sx;
}

void trimStringToWidth(char* str, size_t width) {
	size_t sx = 0;
	size_t sl = strlen(str);
	for (size_t i = 0; i < sl; i++) {
		int cwid = fontWidth[str[i]];
		float k = (float) (cwid >> 4);
		float l = (float) (cwid & 15) + 1.;
		sx += (l - k) / 2. + 1.;
		if (sx >= width) {
			str[i] = 0;
			return;
		}
	}
}
