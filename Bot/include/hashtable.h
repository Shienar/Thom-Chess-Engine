#ifndef HASHTABLE
#define HASHTABLE

#include "structs.h"
#include "engine.h"
#include <stdint.h>

#define STARTING_CAPACITY 1024

/**
 * I'm replacing my old zobrist hashing value generation with polyglot's for convenience when
 * accessing opening books.
 * 
 * Documentation is at http://hgm.nubati.net/book_format.html
 */
extern const uint64_t zobrist_keys[781];

uint64_t getHashCode(bitboard* board);

#endif