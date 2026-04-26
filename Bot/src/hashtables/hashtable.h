#ifndef HASHTABLE
#define HASHTABLE

#include "../types.h"
#include <stdint.h>

/**
 * I'm replacing my old zobrist hashing value generation with polyglot's for convenience when
 * accessing opening books.
 * 
 * Documentation is at http://hgm.nubati.net/book_format.html
 */
extern const uint64_t zobrist_keys[781];
extern uint64_t zobrist_piece_keys[PIECE_COUNT][64];

void initZobristPieceKeys();

uint64_t getEnPassantHash(bitboard* board);
uint64_t getHashCode(bitboard* board);

#endif