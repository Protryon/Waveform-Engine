/*
 * manip.h
 *
 *  Created on: Jul 10, 2016
 *      Author: root
 */

#ifndef MANIP_H_
#define MANIP_H_

void swapEndian(void* dou, size_t ss);

int memeq(const unsigned char* mem1, size_t mem1_size, const unsigned char* mem2, size_t mem2_size);

int memseq(const unsigned char* mem, size_t mem_size, const unsigned char c);

#endif /* MANIP_H_ */
