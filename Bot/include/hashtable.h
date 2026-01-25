#ifndef HASHTABLE
#define HASHTABLE

#include "structs.h"
#include "engine.h"

#define STARTING_CAPACITY 32

#define FNV_OFFSET_BASIS 14695981039346656037ull
#define FNV_PRIME 1099511628211ull
#define HASHKEY_STRING_LENGTH 65

/**
 * Fowler–Noll–Vo hash function
 * https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
 */
uint64_t getHashCode(const char* key);

/**
 * Converts a bitboard to a hash key.
 */
void hashKey(struct bitboard* board, char* target);

#endif