/*
 * hashmap.h
 *
 *  Created on: Nov 19, 2015
 *      Author: root
 */

#ifndef HASHMAP_H_
#define HASHMAP_H_

#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

struct hashmap {
		union hashmap_entry** buckets;
		size_t bucket_count;
		size_t entry_count;
		uint8_t mc;
		uint8_t size_flags;
		uint8_t varkeys;
		pthread_rwlock_t data_mutex;
};

struct hashmap_rkentry {
		union hashmap_entry* next;
		void* value;
		uint64_t key;
};

struct hashmap_vkentry {
		union hashmap_entry* next;
		void* value;
		uint64_t key; // a pointer, but guaranteed to be 64-bit
		uint32_t keysize;
};

union hashmap_entry {
		struct hashmap_rkentry rkentry;
		struct hashmap_vkentry vkentry;
};

struct hashmap* new_hashmap(size_t initial_size, uint8_t mc, uint8_t varkeys, uint8_t size_flags);

int del_hashmap(struct hashmap* map);

void put_hashmap(struct hashmap* map, uint64_t key, uint32_t keysize, void* value);

float loadfactor_hashmap(struct hashmap* map);

void* get_hashmap(struct hashmap* map, uint64_t key, uint32_t keysize);

int contains_hashmap(struct hashmap* map, uint64_t data, uint32_t keysize);

#define BEGIN_HASHMAP_ITERATION(hashmap) if (hashmap->mc) pthread_rwlock_rdlock(&hashmap->data_mutex); for (size_t _hmi = 0; _hmi < hashmap->bucket_count; _hmi++) {struct hashmap_vkentry* he = &hashmap->buckets[_hmi]->vkentry;int _s = 1;struct hashmap_vkentry* nhe = NULL;while (he != NULL) {if(!_s)he = nhe;else _s=0;if(he == NULL)break;nhe = he->next;uint64_t key = he->key;void* value = he->value;
#define END_HASHMAP_ITERATION(hashmap)	}} if (hashmap->mc) pthread_rwlock_unlock(&hashmap->data_mutex);
#define BREAK_HASHMAP_ITERATION(hashmap)	if (hashmap->mc) pthread_rwlock_unlock(&hashmap->data_mutex);

#endif /* HASHMAP_H_ */
