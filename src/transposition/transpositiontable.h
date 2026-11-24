#pragma once

#include "board/board.h"
#include <stdlib.h>

#define MAX_TT_AGE 127
#define TT_AGE_MASK 0x7F
typedef struct tt_entry {
	union {
		uint64_t data;
		struct {
			uint16_t	hash_msb;
			int16_t	 	evaluation;
			move		best_move;
			uint16_t	age		 : 7;
			uint16_t	depth	   : 6; // Max Ply == 60 <= 2 ^ 6 - 1 == 63
			uint16_t	node_type	: 3;
		};
	};
} tt_entry;

#define TT_ENTRIES_PER_BUCKET 4
typedef struct __attribute__((aligned(32))) tt_bucket  {
	tt_entry entries[TT_ENTRIES_PER_BUCKET];
} tt_bucket;

typedef struct hashtable_tt {
	tt_bucket *array;
	size_t capacity;
	uint16_t age : 7;

	#ifdef SEARCHINFO
	uint64_t overwrites; //writing new data for same position
	uint64_t collisions; //replacing old data from different position with new data from new position
	uint64_t shallow_collisions; //the new collision data is shallow
	uint64_t fills; //writing to an empty slot
	#endif
} hashtable_tt;

extern uint64_t tt_bytes;

#define NODE_BOUND_UNKNOWN 0 //Depth = 0
#define NODE_BOUND_EXACT 1
#define NODE_BOUND_UPPER 2
#define NODE_BOUND_LOWER 3

extern hashtable_tt *transposition_table;

hashtable_tt *create_tt_table(void);
void destroy_tt_table(hashtable_tt *ht);
void clear_tt(hashtable_tt *tt);

int get_tt_hashfull(hashtable_tt *ht);
void tt_age(hashtable_tt *tt);
void tt_prefetch(hashtable_tt *tt, uint64_t hash_code);

tt_entry tt_get(struct bitboard *board, hashtable_tt *tt, uint8_t *hit, int ply);
void tt_set(hashtable_tt *tt, tt_entry entry, uint64_t hash_code64, int ply);
