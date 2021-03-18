#include "includes.h"
#include "hashtable.h"

static NFHashTable *NF_TABLE = NULL;
word_t NF_BIT_MAP;

void create_nf_hashtable();