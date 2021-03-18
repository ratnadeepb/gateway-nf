#pragma once
#include <limits.h>
#include <stdint.h>

typedef uint64_t word_t;
enum { BITS_PER_WORD = sizeof(word_t) * CHAR_BIT };
#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b) ((b) % BITS_PER_WORD)

void set_bit(word_t *words, int n)
{
	words[WORD_OFFSET(n)] |= ((word_t)1 << BIT_OFFSET(n));
}

void clear_bit(word_t *words, int n)
{
	words[WORD_OFFSET(n)] &= ~((word_t)1 << BIT_OFFSET(n));
}

int get_bit(word_t *words, int n)
{
	word_t bit = words[WORD_OFFSET(n)] & ((word_t)1 << BIT_OFFSET(n));
	return bit != 0;
}

int first_free_bit(word_t *word)
{
	int i;
	for (i = 0; i < BITS_PER_WORD; i++) {
		if (get_bit(word, i) == 0) return i;
	}
	return -1;
}