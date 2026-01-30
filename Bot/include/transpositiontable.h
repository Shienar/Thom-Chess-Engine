#ifndef TRANSPOSITIONTABLE
#define TRANSPOSITIONTABLE

#include "hashtable.h"

#define MAX_TABLE_SIZE 65536
#define NODE_TYPE_PV 1
#define NODE_TYPE_ALL 2
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

table_entry_tt* transposition_table_get(struct bitboard* board, hashtable_tt* tt);

void transposition_table_set(hashtable_tt* tt, table_entry_tt entry);

#endif