/*
 * wincompat.h
 *
 *  Created on: Jul 29, 2016
 *      Author: root
 */

#ifndef WINCOMPAT_H_
#define WINCOMPAT_H_

#include <stdlib.h>

#ifdef __MINGW32__
void srand48(long int seedval) {
	srand(seedval);
}

double drand48() {
	return (double)rand() / (double) RAND_MAX;
}
#else

#endif

#endif /* WINCOMPAT_H_ */
