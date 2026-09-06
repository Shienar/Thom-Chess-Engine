#ifndef TRANSPOSITIONTABLE
#define TRANSPOSITIONTABLE

#include "hashtables/hash.h"

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


tt_entry transposition_table_get(struct bitboard* board, hashtable_tt* tt, uint8_t* hit, int ply);
void transposition_table_set(hashtable_tt* tt, tt_entry entry, uint64_t hashCode64, int ply);


#endif