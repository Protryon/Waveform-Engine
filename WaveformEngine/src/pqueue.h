/*
 * pqueue.h
 *
 *  Created on: Nov 19, 2015
 *      Author: root
 */

#ifndef pqueue_H_
#define pqueue_H_

#include <pthread.h>

struct __pqueue_entry {
		float prio;
		void* ptr;
};

struct pqueue {
		size_t size;
		size_t capacity;
		size_t start;
		size_t end;
		size_t rc;
		struct __pqueue_entry* data;
		pthread_mutex_t data_mutex;
		pthread_mutex_t in_mutex;
		pthread_cond_t in_cond;
		pthread_mutex_t out_mutex;
		pthread_cond_t out_cond;
		int mt;
};

struct pqueue* new_pqueue(size_t capacity, int mt);

int del_pqueue(struct pqueue* pqueue);

int add_pqueue(struct pqueue* pqueue, void* data, float priority);

void* pop_pqueue(struct pqueue* pqueue);

void* timedpop_pqueue(struct pqueue* pqueue, struct timespec* abstime);

#endif /* pqueue_H_ */
