/*
 * streams.c
 *
 *  Created on: Nov 17, 2015
 *      Author: root
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "smem.h"

ssize_t readLine(int fd, char* line, size_t len) {
	if (len >= 1) line[0] = 0;
	char b = 0;
	int s = 0;
	int i = 0;
	do {
		s = read(fd, &b, 1);
		if ((s == 0 && i == 0) || s < 0) {
			return -1;
		}
		if (s == 0) {
			break;
		}
		if (s > 0 && b != 13 && b != 10) {
			line[i++] = b;
		}
	} while (b > -1 && s > 0 && b != 10 && i < len - 1);
	line[i] = 0;
	return i;
}

ssize_t readLineDynamic(int fd, char** line) {
	if (*line != NULL) {
		free(*line);
		*line = NULL;
	}
	*line = smalloc(4097);
	size_t len = 4096;
	(*line)[len] = 0;
	char b = 0;
	ssize_t s = 0;
	size_t i = 0;
	do {
		s = read(fd, &b, 1);
		if ((s == 0 && i == 0) || s < 0) {
			return -1;
		}
		if (s == 0) {
			break;
		}
		if (s > 0 && b != 13 && b != 10) {
			(*line)[i++] = b;
		}
		if (i > len - 2048) {
			len += 4096;
			(*line) = srealloc(*line, len + 1);
			(*line)[len] = 0;
		}
	} while (b > -1 && s > 0 && b != 10 && i < len - 1);
	(*line)[i] = 0;
	return i;
}

ssize_t writeLine(int fd, char* line, size_t len) {
	static char nl[2] = { 0x0D, 0x0A };
	int i = 0;
	while (i < len) {
		int x = write(fd, line + i, len - i);
		if (x < 0) return -1;
		i += x;
	}
	int i2 = 0;
	while (i2 < 2) {
		int y = write(fd, nl + i2, 2 - i2);
		if (y < 0) return -1;
		i2 += y;
	}
	return i;
}

ssize_t writeFully(int fd, void* buf, size_t size) {
	ssize_t wr = 0;
	while (wr < size) {
		ssize_t t = write(fd, buf + wr, size - wr);
		if (t < 0) {
			return wr == 0 ? -1 : wr;
		} else {
			wr += t;
		}
	}
	return wr;
}

ssize_t readFully(int fd, void* buf, size_t size) {
	ssize_t rd = 0;
	while (rd < size) {
		ssize_t t = read(fd, buf + rd, size - rd);
		if (t <= 0) {
			return rd == 0 ? -1 : 0;
		} else {
			rd += t;
		}
	}
	return rd;
}
