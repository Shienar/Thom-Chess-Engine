#ifndef HASHTABLE
#define HASHTABLE

#include <stdint.h>
struct bitboard;
#include "bitboard.h"

#define STARTING_CAPACITY 32

#define FNV_OFFSET_BASIS 14695981039346656037ull
#define FNV_PRIME 1099511628211ull

typedef struct table_entry {
    char* key;
    int count;
} table_entry;

typedef struct hashtable {
    table_entry* array;
    size_t capacity;
    size_t size;
} hashtable;

/**
 * Allocates hash table and corresponding array
 */
hashtable* create_hashTable();

hashtable* copy_hashTable(hashtable* src);

/**
 * Frees hash table and corresponding array + allocated array elements.
 */
void destroy_hashTable(hashtable* ht);

/**
 * Fowler–Noll–Vo hash function
 * https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
 */
uint64_t getHashCode(const char* key);

/**
 * Converts a bitboard to a hash key.
 * Allocates the memory required.
 */
void hashKey(struct bitboard* board, char* target);

/**
 * increments a tablue value and returns the new value.
 * Inserts the value into the table if it doesn't exist.
 * Either frees or assigns the key.
 * returns INT32_MIN on error
 */
int increment_table_value(hashtable* ht, const char* key, int amount);

/**
 * decrements a tablue value and returns the new value.
 * Removes an entry from the table if the value is zero.
 * Frees the temporary key that it gets passed.
 * returns INT32_MIN on error
 */
int decrement_table_value(hashtable* ht, const char* key);

#endif