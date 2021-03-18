/* This code is a variant of the one found here:
 * https://medium.com/@bennettbuchanan/an-introduction-to-hash-tables-in-c-b83cbf2b4cf6
 */
#include "hashtable.h"

HashTable *ht_create()
{
	HashTable *ht;

	ht = (HashTable *)malloc(sizeof(HashTable));
	if (ht == NULL) return NULL;

	ht->size = HASH_BUCKETS;

	ht->array = (List **)malloc(ht->size * sizeof(List));
	if (ht->array == NULL) return NULL;

	memset(ht->array, 0, ht->size * sizeof(List));
	return ht;
}

/* daisy chain in case of collision */
void node_handler(HashTable *ht, List *node)
{
	unsigned int i = node->key & (ht->size - 1); /* x % 2^n == x % (2^n - 1) if x > 0 */
	List *tmp = ht->array[i];

	if (ht->array[i] != NULL) {
		while (tmp != NULL) {
			if (tmp->key == node->key) break;
			tmp = tmp->nxt;
		}

		if (tmp == NULL) {
			node->nxt = ht->array[i];
			ht->array[i] = node;
		} else {
			free((void *)&tmp->val);
			tmp->val = node->val;
			free((void *)&node->val);
			free(node);
		}
	} else {
		node->nxt = NULL;
		ht->array[i] = node;
	}
}


int ht_put(HashTable *ht, uint32_t key, Record *mem, off_t off)
{
	Entry val = { .mem = mem, .off = off};
	List *node;
	if (ht == NULL) return 1;

	node = malloc(sizeof(List));
	if (node == NULL) return 1;

	node->key = key;
	node->val = val;

	node_handler(ht, node);

	return 0;
}

Entry ht_get(HashTable *ht, uint32_t key)
{
	List *tmp;
	unsigned int i = key & (ht->size - 1);
	Entry null_entry = {.mem = NULL, .off = 0};

	if (ht == NULL) return null_entry;

	tmp = ht->array[i];

	while (tmp != NULL) {
		if (tmp->key == key) return tmp->val;
		tmp = tmp->nxt;
	}

	return null_entry; /* control reaches here only if key not found */
}

void ht_free(HashTable *ht)
{
	int i;
	List *tmp;
	if (ht == NULL) return;

	for (i = 0; i < ht->size; i++) {
		if (ht->array[i] != NULL) {
			while (ht->array[i] != NULL) {
				tmp = ht->array[i];
				ht->array[i] = ht->array[i]->nxt;
				free((void *)&tmp->val);
				free(tmp);
			}
		}
	}
	free(ht->array);
	free(ht);
}