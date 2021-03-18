
#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "includes.h"
#include "internal.h"

typedef struct list {
	uint32_t key;
	Entry val;
	struct list *nxt;
} List;

typedef struct hashtable {
	uint16_t size;
	List **array;
} HashTable;

/* create a new hashtable */
HashTable *ht_create();

/* add a new connection to the hash table
 * @ht: pointer to hashtable
 * @key: hash_record(record) identifies the connection
 * @mem: virtual memory pointer where this connection is added, offset within the memory where this connection is added
 */
int ht_put(HashTable *ht, uint32_t key, Record *mem, off_t off);

/* get a connection detail from the hashtable */
Entry ht_get(HashTable *ht, uint32_t key);

/* destroy the hashtable */
void ht_free(HashTable *ht);

#endif