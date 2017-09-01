/*
 * manip.c
 *
 *  Created on: Jul 10, 2016
 *      Author: root
 */

#include <stdlib.h>
#include "manip.h"
#include <stdint.h>

void swapEndian(void* dou, size_t ss) {
	uint8_t* pxs = (uint8_t*) dou;
	for (int i = 0; i < ss / 2; i++) {
		uint8_t tmp = pxs[i];
		pxs[i] = pxs[ss - 1 - i];
		pxs[ss - 1 - i] = tmp;
	}
}

int memeq(const unsigned char* mem1, size_t mem1_size, const unsigned char* mem2, size_t mem2_size) {
	if (mem1 == NULL || mem2 == NULL) return 0;
	if (mem1 == mem2 && mem1_size == mem2_size) return 1;
	if (mem1_size != mem2_size) return 0;
	for (int i = 0; i < mem1_size; i++) {
		if (mem1[i] != mem2[i]) {
			return 0;
		}
	}
	return 1;
}

int memseq(const unsigned char* mem, size_t mem_size, const unsigned char c) {
	if (mem == NULL) return 0;
	for (int i = 0; i < mem_size; i++) {
		if (mem[i] != c) {
			return 0;
		}
	}
	return 1;
}
