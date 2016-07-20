/*
 * pqueue.c
 *
 *  Created on: Nov 19, 2015
 *      Author: root
 */

#include "pqueue.h"
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "xstring.h"
#include <stdlib.h>

struct pqueue* new_pqueue(size_t capacity, int mt) {
	struct pqueue* pqueue = malloc(sizeof(struct pqueue));
	pqueue->capacity = capacity;
	pqueue->data = malloc((capacity == 0 ? 1 : capacity) * sizeof(struct __pqueue_entry));
	pqueue->rc = capacity == 0 ? 1 : 0;
	pqueue->start = 0;
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
		if (pthread_mutex_init(&pqueue->out_mutex, NULL)) {
			free(pqueue->data);
			pqueue->data = NULL;
			free(pqueue);
			pthread_mutex_destroy(&pqueue->data_mutex);
			return NULL;
		}
		if (pthread_mutex_init(&pqueue->in_mutex, NULL)) {
			free(pqueue->data);
			pqueue->data = NULL;
			free(pqueue);
			pthread_mutex_destroy(&pqueue->data_mutex);
			pthread_mutex_destroy(&pqueue->out_mutex);
			return NULL;
		}
		if (pthread_cond_init(&pqueue->out_cond, NULL)) {
			free(pqueue->data);
			pqueue->data = NULL;
			free(pqueue);
			pthread_mutex_destroy(&pqueue->data_mutex);
			pthread_mutex_destroy(&pqueue->out_mutex);
			pthread_mutex_destroy(&pqueue->in_mutex);
			return NULL;
		}
		if (pthread_cond_init(&pqueue->in_cond, NULL)) {
			free(pqueue->data);
			pqueue->data = NULL;
			free(pqueue);
			pthread_mutex_destroy(&pqueue->data_mutex);
			pthread_mutex_destroy(&pqueue->out_mutex);
			pthread_mutex_destroy(&pqueue->in_mutex);
			pthread_cond_destroy(&pqueue->out_cond);
			return NULL;
		}
	}
	return pqueue;
}

int del_pqueue(struct pqueue* pqueue) {
	if (pqueue == NULL || pqueue->data == NULL) return -1;
	if (pqueue->mt) {
		if (pthread_mutex_destroy(&pqueue->data_mutex)) return -1;
		if (pthread_mutex_destroy(&pqueue->out_mutex)) return -1;
		if (pthread_cond_destroy(&pqueue->out_cond)) return -1;
		if (pthread_mutex_destroy(&pqueue->in_mutex)) return -1;
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
		size_t orc = pqueue->rc;
		pqueue->rc += 1024 / sizeof(struct __pqueue_entry);
		struct __pqueue_entry* ndata = malloc(pqueue->rc * sizeof(struct __pqueue_entry));
		if (pqueue->start < pqueue->end) {
			memcpy(ndata, pqueue->data + pqueue->start, (pqueue->end - pqueue->start) * sizeof(struct __pqueue_entry));
		} else {
			memcpy(ndata, pqueue->data + pqueue->start, (orc - pqueue->start) * sizeof(struct __pqueue_entry));
			memcpy(ndata + (orc - pqueue->start), pqueue->data + pqueue->end, (pqueue->start - pqueue->end) * sizeof(struct __pqueue_entry));
		}
		free(pqueue->data);
		pqueue->data = ndata;
	} else if (pqueue->capacity == 0) {
	} else {
		if (!pqueue->mt) return 1;
		pthread_mutex_unlock(&pqueue->data_mutex);
		pthread_mutex_lock(&pqueue->in_mutex);
		while (pqueue->size == pqueue->capacity) {
			pthread_cond_wait(&pqueue->in_cond, &pqueue->in_mutex);
		}
		pthread_mutex_unlock(&pqueue->in_mutex);
		pthread_mutex_lock(&pqueue->data_mutex);
	}
	struct __pqueue_entry pd;
	pd.ptr = data;
	pd.prio = priority;
	float i = (float) pqueue->size / 2.;
	float ts = (float) pqueue->size / 2.;
	size_t rp = pqueue->capacity > 0 ? pqueue->capacity : pqueue->rc;
	int tc = 0;
	int fri = 0;
	while (ts > 0.) {
		int ri = (int) i + pqueue->start;
		if (ri >= rp) ri -= rp;
		if (pqueue->data[ri].prio > pd.prio) i -= ts;
		else i += ts;
		if (i < 0.) i = 0.;
		if (i > pqueue->size) {
			i = pqueue->size - 1;
		}
		if ((ri == pqueue->end || pqueue->data[ri].prio > pd.prio) && (ri == pqueue->start || pqueue->data[ri - 1].prio < pd.prio)) {
			fri = ri;
			break;
		}
		ts /= 2.;
		tc++;
	}
	if (fri <= pqueue->end) if (pqueue->end > pqueue->start) {
		memmove(pqueue->data + fri + 1, pqueue->data + fri, (pqueue->end - fri) * sizeof(struct __pqueue_entry));
	} else {
		if (fri > pqueue->start) {
			memmove(pqueue->data + 1, pqueue->data, pqueue->end * sizeof(struct __pqueue_entry));
			memcpy(pqueue->data, pqueue->data + rp - 1, sizeof(struct __pqueue_entry));
			memmove(pqueue->data + fri + 1, pqueue->data + fri, (rp - (pqueue->start) - fri) * sizeof(struct __pqueue_entry));
		} else {
			memmove(pqueue->data + fri + 1, pqueue->data + fri, (pqueue->end - fri) * sizeof(struct __pqueue_entry));
		}
	}
	pqueue->end++;
	memcpy(pqueue->data + fri, &pd, sizeof(struct __pqueue_entry));
	if (pqueue->end >= rp) {
		if (pqueue->end - rp == pqueue->start) {
			size_t orc = pqueue->rc;
			pqueue->rc += 1024 / sizeof(struct __pqueue_entry);
			struct __pqueue_entry* ndata = malloc(pqueue->rc * sizeof(struct __pqueue_entry));
			if (pqueue->start < pqueue->end) {
				memcpy(ndata, pqueue->data + pqueue->start, (pqueue->end - pqueue->start) * sizeof(struct __pqueue_entry));
			} else {
				memcpy(ndata, pqueue->data + pqueue->start, (orc - pqueue->start) * sizeof(struct __pqueue_entry));
				memcpy(ndata + (orc - pqueue->start), pqueue->data + pqueue->end, (pqueue->start - pqueue->end) * sizeof(struct __pqueue_entry));
			}
			free(pqueue->data);
			pqueue->data = ndata;
		} else pqueue->end -= rp;
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
		pthread_mutex_lock(&pqueue->out_mutex);
		while (pqueue->size == 0) {
			pthread_cond_wait(&pqueue->out_cond, &pqueue->out_mutex);
		}
		pthread_mutex_unlock(&pqueue->out_mutex);
		pthread_mutex_lock(&pqueue->data_mutex);
	} else if (pqueue->size == 0) {
		return NULL;
	}
	struct __pqueue_entry data = pqueue->data[pqueue->start++];
	size_t rp = pqueue->capacity > 0 ? pqueue->capacity : pqueue->rc;
	if (pqueue->start >= rp) {
		pqueue->start -= rp;
	}
	pqueue->size--;
	if (pqueue->mt) {
		pthread_mutex_unlock(&pqueue->data_mutex);
		pthread_cond_signal(&pqueue->in_cond);
	}
	return data.ptr;
}

void* timedpop_pqueue(struct pqueue* pqueue, struct timespec* abstime) {
	if (pqueue->mt) {
		pthread_mutex_lock(&pqueue->out_mutex);
		while (pqueue->size == 0) {
			int x = pthread_cond_timedwait(&pqueue->out_cond, &pqueue->out_mutex, abstime);
			if (x) {
				pthread_mutex_unlock(&pqueue->out_mutex);
				errno = x;
				return NULL;
			}
		}
		pthread_mutex_unlock(&pqueue->out_mutex);
		pthread_mutex_lock(&pqueue->data_mutex);
	} else if (pqueue->size == 0) {
		return NULL;
	}
	struct __pqueue_entry data = pqueue->data[pqueue->start++];
	size_t rp = pqueue->capacity > 0 ? pqueue->capacity : pqueue->rc;
	if (pqueue->start >= rp) {
		pqueue->start -= rp;
	}
	pqueue->size--;
	if (pqueue->mt) {
		pthread_mutex_unlock(&pqueue->data_mutex);
		pthread_cond_signal(&pqueue->in_cond);
	}
	return data.ptr;
}
