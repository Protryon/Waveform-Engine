/*
 * streams.h
 *
 *  Created on: Nov 17, 2015
 *      Author: root
 */

#ifndef STREAMS_H_
#define STREAMS_H_

size_t readLine(int fd, char* line, size_t len);

ssize_t readLineDynamic(int fd, char** line);

size_t writeLine(int fd, char* line, size_t len);

ssize_t writeFully(int fd, void* buf, size_t size);

ssize_t readFully(int fd, void* buf, size_t size);

#endif /* STREAMS_H_ */
