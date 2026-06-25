#ifndef TRANSPOSITIONTABLE
#define TRANSPOSITIONTABLE

#include "hashtables/hashtable.h"

extern uint64_t tt_size_entries;

//The searched node's evaluation falls in the range of (alpha, beta)
//The saved score is exact for the given depth.
#define NODE_TYPE_PV 1 

//The score failed-low. Score <= alpha.
//The saved score is an upper bound for the real score.
#define NODE_TYPE_ALL 2

//The score failed-high. Score >= beta.
//The saved score is a lower bound for the real score.
#define NODE_TYPE_CUT 3

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