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
#define PATH_SEP "\\"
void srand48(long int seedval);

double drand48();
#else
#define PATH_SEP "/"
#endif

#endif /* WINCOMPAT_H_ */
