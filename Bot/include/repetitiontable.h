#ifndef REPETITIONTABLE
#define REPETITIONTABLE

#include "hashtable.h"

/**
 * Allocates hash table and corresponding array
 */
hashtable_pos* create_hashTable_pos();

hashtable_pos* copy_hashTable_pos(hashtable_pos* src);

/**
 * Frees hash table and corresponding array + allocated array elements.
 */
void destroy_hashTable_pos(hashtable_pos* ht);

/**
 * increments a table value and returns the new value.
 * Inserts the value into the table if it doesn't exist.
 * Either frees or assigns the key.
 * returns DBL_MIN on error
 */
double increment_table_value(hashtable_pos* ht, const char* key, double amount);

/**
 * decrements a tablue value and returns the new value.
 * Removes an entry from the table if the value is zero.
 * Frees the temporary key that it gets passed.
 * returns DBL_MIN on error
 */
double decrement_table_value(hashtable_pos* ht, const char* key);

#endif