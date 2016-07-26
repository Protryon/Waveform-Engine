/*
 * pqueue.c
 *
 *  Created on: Nov 19, 2015
 *      Author: root
 */

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "xstring.h"
#include <stdlib.h>
#include "pqueue.h"
#include "smem.h"

struct pqueue* new_pqueue(size_t capacity, int mt) {
	struct pqueue* pqueue = smalloc(sizeof(struct pqueue));
	pqueue->capacity = capacity;
	pqueue->data = smalloc((capacity == 0 ? 2 : capacity + 1) * sizeof(struct __pqueue_entry));
	pqueue->rc = capacity == 0 ? 1 : 0;
	pqueue->end = 0;
	pqueue->size = 0;
	pqueue->mt = mt;
	if (mt) {
		if (pthread_mutex_init(&pqueue->data_mutex, NULL)) {
			free(pqueue->data);
			pqueue->data = NULL;
			free(pqueue);
			return NULL;
		}
		if (pthread_cond_init(&pqueue->out_cond, NULL)) {
			free(pqueue->data);
			pqueue->data = NULL;
			pthread_mutex_destroy(&pqueue->data_mutex);
			free(pqueue);
			return NULL;
		}
		if (pthread_cond_init(&pqueue->in_cond, NULL)) {
			free(pqueue->data);
			pqueue->data = NULL;
			pthread_mutex_destroy(&pqueue->data_mutex);
			pthread_cond_destroy(&pqueue->out_cond);
			free(pqueue);
			return NULL;
		}
	}
	return pqueue;
}

int del_pqueue(struct pqueue* pqueue) {
	if (pqueue == NULL || pqueue->data == NULL) return -1;
	if (pqueue->mt) {
		if (pthread_mutex_destroy(&pqueue->data_mutex)) return -1;
		if (pthread_cond_destroy(&pqueue->out_cond)) return -1;
		if (pthread_cond_destroy(&pqueue->in_cond)) return -1;
	}
	free(pqueue->data);
	pqueue->data = NULL;
	free(pqueue);
	return 0;
}

int add_pqueue(struct pqueue* pqueue, void* data, float priority) {
	if (pqueue->mt) pthread_mutex_lock(&pqueue->data_mutex);
	if (pqueue->size == pqueue->rc && pqueue->capacity == 0) {
		pqueue->rc += 1024 / sizeof(struct __pqueue_entry);
		pqueue->data = srealloc(pqueue->data, pqueue->rc * sizeof(struct __pqueue_entry));
	} else if (pqueue->capacity == 0) {
	} else {
		while (pqueue->size == pqueue->capacity) {
			if (!pqueue->mt) return 1;
			pthread_cond_wait(&pqueue->in_cond, &pqueue->data_mutex);
		}
	}
	struct __pqueue_entry pd;
	pd.ptr = data;
	pd.prio = priority;
	int tb = ++pqueue->end;
	for (; tb > 1 && priority < pqueue->data[tb / 2].prio; tb /= 2)
		memcpy(pqueue->data + tb, pqueue->data + (tb / 2), sizeof(struct __pqueue_entry));
	memcpy(pqueue->data + tb, &pd, sizeof(struct __pqueue_entry));
	size_t rp = pqueue->capacity > 0 ? pqueue->capacity : pqueue->rc;
	if (pqueue->end == rp) {
		pqueue->rc += 1024 / sizeof(struct __pqueue_entry);
		pqueue->data = srealloc(pqueue->data, pqueue->rc * sizeof(struct __pqueue_entry));
	}
	pqueue->size++;
	if (pqueue->mt) {
		pthread_mutex_unlock(&pqueue->data_mutex);
		pthread_cond_signal(&pqueue->out_cond);
	}
	return 0;
}

void* pop_pqueue(struct pqueue* pqueue) {
	if (pqueue->mt) {
		pthread_mutex_lock(&pqueue->data_mutex);
		while (pqueue->size == 0) {
			pthread_cond_wait(&pqueue->out_cond, &pqueue->data_mutex);
		}
	} else if (pqueue->size == 0) {
		return NULL;
	}
	struct __pqueue_entry data = pqueue->data[1];
	memcpy(pqueue->data + 1, pqueue->data + pqueue->size--, sizeof(struct __pqueue_entry));
	struct __pqueue_entry te;
	memcpy(&te, pqueue->data + 1, sizeof(struct __pqueue_entry));
	int i = 1;
	for (int c = 0; 2 * i <= pqueue->size; i = c) {
		c = 2 * i;
		if (c != pqueue->size && pqueue->data[c].prio > pqueue->data[c + 1].prio) c++;
		if (te.prio > pqueue->data[c].prio) memcpy(pqueue->data + i, pqueue->data + c, sizeof(struct __pqueue_entry));
		else break;
	}
	memcpy(pqueue->data + i, &te, sizeof(struct __pqueue_entry));
	if (pqueue->mt) {
		pthread_mutex_unlock(&pqueue->data_mutex);
		pthread_cond_signal(&pqueue->in_cond);
	}
	return data.ptr;
}

void* timedpop_pqueue(struct pqueue* pqueue, struct timespec* abstime) {
	if (pqueue->mt) {
		pthread_mutex_lock(&pqueue->data_mutex);
		while (pqueue->size == 0) {
			int x = pthread_cond_timedwait(&pqueue->out_cond, &pqueue->data_mutex, abstime);
			if (x) {
				pthread_mutex_unlock(&pqueue->data_mutex);
				errno = x;
				return NULL;
			}
		}
	} else if (pqueue->size == 0) {
		return NULL;
	}
	struct __pqueue_entry data = pqueue->data[1];
	size_t rp = pqueue->capacity > 0 ? pqueue->capacity : pqueue->rc;
	memcpy(pqueue->data + 1, pqueue->data + rp - 1, sizeof(struct __pqueue_entry));
	struct __pqueue_entry te;
	memcpy(&te, pqueue->data + 1, sizeof(struct __pqueue_entry));
	int i = 1;
	for (int c = 0; 2 * i < rp; i = c) {
		c = 2 * i;
		if (c != rp && pqueue->data[c].prio > pqueue->data[c + 1].prio) c++;
		if (te.prio > pqueue->data[c].prio) memcpy(pqueue->data + i, pqueue->data + c, sizeof(struct __pqueue_entry));
	}
	memcpy(pqueue->data + i, &te, sizeof(struct __pqueue_entry));
	pqueue->size--;
	if (pqueue->mt) {
		pthread_mutex_unlock(&pqueue->data_mutex);
		pthread_cond_signal(&pqueue->in_cond);
	}
	return data.ptr;
}
