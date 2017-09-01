/*
 * hashmap.c
 *
 *  Created on: Nov 19, 2015
 *      Author: root
 */

#include "hashmap.h"
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "xstring.h"
#include <math.h>
#include <sys/types.h>
#include "smem.h"
#include <math.h>
#include "manip.h"

//size_flags: 0x1: don't resize down, 0x2: don't resize at all (allow infinite load factor)
struct hashmap* new_hashmap(size_t initial_size, uint8_t mc, uint8_t varkeys, uint8_t size_flags) {
	if (initial_size < 1) return NULL;
	struct hashmap* map = smalloc(sizeof(struct hashmap));
	map->bucket_count = initial_size;
	map->buckets = scalloc(map->bucket_count * sizeof(void*));
	map->varkeys = varkeys;
	map->entry_count = 0;
	map->mc = mc;
	if (mc && pthread_rwlock_init(&map->data_mutex, NULL)) {
		free(map->buckets);
		map->buckets = NULL;
		free(map);
		return NULL;
	}
	return map;
}

int del_hashmap(struct hashmap* map) {
	if (map == NULL || map->buckets == NULL) return -1;
	if (map->mc && pthread_rwlock_destroy(&map->data_mutex)) return -1;
	for (size_t i = 0; i < map->bucket_count; i++) {
		union hashmap_entry* head = map->buckets[i];
		while (1) {
			if (head == NULL) break;
			union hashmap_entry* next = head->rkentry.next;
			free(head);
			head = next;
		}
	}
	free(map->buckets);
	map->buckets = NULL;
	free(map);
	return 0;
}

void put_hashmap(struct hashmap* map, uint64_t key, uint32_t keysize, void* value) {
	if (map->mc) pthread_rwlock_wrlock(&map->data_mutex);
	uint64_t hkey = key;
	if (map->varkeys) {
		hkey = (uint64_t) keysize;
		hkey |= (uint64_t) keysize << 32;
		hkey |= (uint64_t) keysize << 16;
		hkey |= (uint64_t) keysize << 48;
	}
	uint8_t* hashi = (uint8_t*) &hkey;
	uint8_t* hashr = map->varkeys ? (uint8_t*) key : (uint8_t*) &key;
	for (int i = 0; i < sizeof(uint64_t); i++) {
		for (int x = 0; x < (map->varkeys ? keysize : sizeof(uint64_t)); x++) {
			if (i == x && !map->varkeys) continue;
			hashi[i] ^= hashr[x];
		}
	}
	uint64_t rhash = hkey % map->bucket_count;
	//printf("%i\n", fhash);
	union hashmap_entry* prior = map->buckets[rhash];
	union hashmap_entry* entry = map->buckets[rhash];
	if (map->varkeys) while (entry != NULL && !memeq((void*) entry->vkentry.key, entry->vkentry.keysize, (void*) key, keysize)) {
		prior = entry;
		entry = entry->rkentry.next;
	}
	else while (entry != NULL && entry->rkentry.key != key) {
		prior = entry;
		entry = entry->rkentry.next;
	}
	if (entry == NULL) {
		if (value != NULL) {
			map->entry_count++;
			entry = smalloc(map->varkeys ? sizeof(struct hashmap_vkentry) : sizeof(struct hashmap_rkentry));
			entry->rkentry.key = key;
			entry->rkentry.value = value;
			entry->rkentry.next = NULL;
			if (map->varkeys) entry->vkentry.keysize = keysize;
			if (prior != NULL && prior != entry) {
				prior->rkentry.next = entry;
			} else {
				map->buckets[rhash] = entry;
			}
		}
	} else {
		if (value == NULL) {
			if (prior != entry && prior != NULL) {
				prior->rkentry.next = entry->rkentry.next;
			} else {
				map->buckets[rhash] = entry->rkentry.next;
			}
			free(entry);
			map->entry_count--;
		} else {
			entry->rkentry.value = value;
		}
	}
	if (map->mc) pthread_rwlock_unlock(&map->data_mutex);
}

float loadfactor_hashmap(struct hashmap* map) {
	return (float) map->entry_count / (float) map->bucket_count;
}

void* get_hashmap(struct hashmap* map, uint64_t key, uint32_t keysize) {
	if (map->mc) pthread_rwlock_rdlock(&map->data_mutex);
	uint64_t hkey = key;
	if (map->varkeys) {
		hkey = (uint64_t) keysize;
		hkey |= (uint64_t) keysize << 32;
		hkey |= (uint64_t) keysize << 16;
		hkey |= (uint64_t) keysize << 48;
	}
	uint8_t* hashi = (uint8_t*) &hkey;
	uint8_t* hashr = map->varkeys ? (uint8_t*) key : (uint8_t*) &key;
	for (int i = 0; i < sizeof(uint64_t); i++) {
		for (int x = 0; x < (map->varkeys ? keysize : sizeof(uint64_t)); x++) {
			if (i == x && !map->varkeys) continue;
			hashi[i] ^= hashr[x];
		}
	}
	uint64_t rhash = hkey % map->bucket_count;
	union hashmap_entry* entry = map->buckets[rhash];
	if (map->varkeys) while (entry != NULL && !memeq((void*) entry->vkentry.key, entry->vkentry.keysize, (void*) key, keysize)) {
		entry = entry->rkentry.next;
	}
	else while (entry != NULL && entry->rkentry.key != key) {
		entry = entry->rkentry.next;
	}
	void* value = NULL;
	if (entry != NULL) {
		value = entry->rkentry.value;
	}
	if (map->mc) pthread_rwlock_unlock(&map->data_mutex);
	return value;
}

int contains_hashmap(struct hashmap* map, uint64_t key, uint32_t keysize) {
	return get_hashmap(map, key, keysize) != NULL;
}
