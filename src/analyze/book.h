#ifndef BOOK
#define BOOK

#include "types.h"

#define MAX_BOOK_PLY 6

typedef struct polyglot_book_entry {
    uint64_t hashKey;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
} polyglot_book_entry;

extern const unsigned char book_bin_start[];
extern const unsigned char book_bin_end[];

extern int useBook;
extern uint64_t entryCount;
extern polyglot_book_entry *entries;

void loadBook();

//Returns a newly allocated move struct.
move_c getBookMove(bitboard* board);

#endif