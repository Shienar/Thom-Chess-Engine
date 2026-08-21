#ifndef TRANSPOSITIONTABLE
#define TRANSPOSITIONTABLE

#include "hashtables/hash.h"

extern uint64_t tt_size_entries;

//0-depth evaluation.
#define NODE_BOUND_UNKNOWN 0

//The searched node's evaluation falls in the range of (alpha, beta)
#define NODE_BOUND_EXACT 1 

//The score failed-low. Score <= alpha.
#define NODE_BOUND_UPPER 2

//The score failed-high. Score >= beta.
#define NODE_BOUND_LOWER 3

extern hashtable_tt* transpositionTable;

/**
 * Allocates hash table and corresponding array
 */
hashtable_tt* create_hashTable_tt();

/**
 * Frees hash table and corresponding array + allocated array elements.
 */
void destroy_hashTable_tt(hashtable_tt* ht);

void clear_tt(hashtable_tt* tt);

table_entry_tt transposition_table_get(struct bitboard* board, hashtable_tt* tt, uint8_t* hit, int ply);

void transposition_table_set(hashtable_tt* tt, table_entry_tt entry, int ply);

void clear_tt(hashtable_tt* tt);

#endif