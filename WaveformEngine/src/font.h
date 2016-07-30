/*
 * font.h
 *
 *  Created on: Jul 25, 2016
 *      Author: root
 */

#ifndef FONT_H_
#define FONT_H_

#include <stdlib.h>
#include <stdint.h>

void setFont(int tx);

int loadFont(char* fontpath, char* sizepath, int tx);

void drawChar(char c, int italic);

void drawString(char* str, float x, float y, uint32_t color);

size_t stringWidth(char* str);

void trimStringToWidth(char* str, size_t width);

#endif /* FONT_H_ */
