#ifndef TRANSPOSITIONTABLE
#define TRANSPOSITIONTABLE

#include "hashtable.h"

#define NODE_TYPE_PV 1
#define NODE_TYPE_ALL 1
#define NODE_TYPE_CUT 1

/**
 * Allocates hash table and corresponding array
 */
hashtable_tt* create_hashTable_tt();

hashtable_tt* copy_hashTable_tt(hashtable_tt* src);

/**
 * Frees hash table and corresponding array + allocated array elements.
 */
void destroy_hashTable_tt(hashtable_tt* ht);

/**
 * Evaluates a position and saves the evaluation to the hash table.
 * OR
 * returns saved evaluation
 */
double transposition_table_evaluate(struct bitboard* board, engine* engine);

#endif