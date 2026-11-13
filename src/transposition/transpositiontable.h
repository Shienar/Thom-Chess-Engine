#pragma once

#include "board/board.h"
#include <stdlib.h>

#define MAX_TT_AGE 127
#define TT_AGE_MASK 0x7F
typedef struct tt_entry {
    union {
        uint64_t data;
        struct {
            uint16_t    hashMSB;
            int16_t     evaluation;
            move        bestMove;
            uint16_t    age         : 7;
            uint16_t    depth       : 6; // Max Ply == 60 <= 2 ^ 6 - 1 == 63
            uint16_t    nodeType    : 3;
        };
    };
} tt_entry;

#define TT_ENTRIES_PER_BUCKET 4
typedef struct __attribute__((aligned(32))) tt_bucket  {
    tt_entry entries[TT_ENTRIES_PER_BUCKET];
} tt_bucket;

typedef struct hashtable_tt {
    tt_bucket* array;
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

extern hashtable_tt* transpositionTable;

hashtable_tt* create_hashTable_tt();
void destroy_hashTable_tt(hashtable_tt* ht);
void clear_tt(hashtable_tt* tt);

int getHashFull(hashtable_tt* ht);
void tt_age(hashtable_tt* tt);
void tt_prefetch(hashtable_tt* tt, uint64_t hashCode);

tt_entry transposition_table_get(struct bitboard* board, hashtable_tt* tt, uint8_t* hit, int ply);
void transposition_table_set(hashtable_tt* tt, tt_entry entry, uint64_t hashCode64, int ply);