#include "nf_internal.h"
#include <stdio.h>

/********** Internal hashtable start (till hashtables is made generic) **********/
typedef struct list {
	char *key;
	uint8_t val;
	struct list *nxt;
} NFList;

typedef struct hashtable {
	uint16_t size;
	NFList **array;
} NFHashTable;

NFHashTable *nft_create()
{
	NFHashTable *ht;

	ht = (NFHashTable *)malloc(sizeof(NFHashTable));
	if (ht == NULL) return NULL;

	ht->size = HASH_BUCKETS;

	ht->array = (NFList **)malloc(ht->size * sizeof(NFList));
	if (ht->array == NULL) return NULL;

	memset(ht->array, 0, ht->size * sizeof(NFList));
	return ht;
}

unsigned int hash(const char *key, unsigned int size)
{
	unsigned int n_hash;
	unsigned int i;

	n_hash = 0;
	i = 0;
	while (key && key[i]) {
		n_hash = (n_hash + key[i]) % size;
		i++;
	}
	return n_hash;
}

/* daisy chain in case of collision */
void node_handler(NFHashTable *ht, NFList *node)
{
	unsigned int i = hash(node->key, ht->size);
	NFList *tmp = ht->array[i];

	if (ht->array[i] != NULL) {
		while (tmp != NULL) {
			if (tmp->key == node->key) break;
			tmp = tmp->nxt;
		}

		if (tmp == NULL) {
			node->nxt = ht->array[i];
			ht->array[i] = node;
		} else {
			tmp->val = node->val;
			free((void *)&node->key);
			free(node);
		}
	} else {
		node->nxt = NULL;
		ht->array[i] = node;
	}
}


int nft_put(NFHashTable *ht, char *key, uint8_t val)
{
	NFList *node;
	if (ht == NULL) return 1;

	node = malloc(sizeof(NFList));
	if (node == NULL) return 1;

	node->key = strdup(key);
	node->val = val;

	node_handler(ht, node);

	return 0;
}

#define NOT_FOUND 65

uint8_t nft_get(NFHashTable *ht, const char *key)
{
	NFList *tmp;
	char *key_cp;
	unsigned int i;

	if (ht == NULL) return NOT_FOUND;

	key_cp = strdup(key);
	i = hash(key, ht->size);

	tmp = ht->array[i];

	while (tmp != NULL) {
		if (tmp->key == key) return tmp->val;
		tmp = tmp->nxt;
	}

	return NOT_FOUND; /* control reaches here only if key not found */
}

void nft_free(NFHashTable *ht)
{
	int i;
	NFList *tmp;
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
/********** Internal hashtable end (till hashtables is made generic) **********/

void create_nf_hashtable()
{
	NFHashTable *nft = nft_create();
	if (nft != NULL) NF_TABLE = nft;
}

void create_nf_bitmap()
{
	NF_BIT_MAP = (uint64_t)0;
}

int register_nf(char *nf_name)
{
	/* check if this name already is known */
	if (nft_get(NF_TABLE, nf_name) != NOT_FOUND) {
		fprintf(stderr, "nf already exists\n");
		return EXIT_FAILURE;
	}

	/* find a free bit position */
	int position = first_free_bit(&NF_BIT_MAP);
	if (position == -1) {
		fprintf(stderr, "no bits are free\n");
		return EXIT_FAILURE;
	}

	/* set the position in the bitmap */
	set_bit(&NF_BIT_MAP, position);

	/* add NF and bit position to map */
	if (nft_put(&NF_TABLE, nf_name, (uint8_t)position) != 0) {
		fprintf(stderr, "failed to register NF\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}