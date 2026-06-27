#ifndef BOOK
#define BOOK

#include "types.h"

typedef struct polyglot_book_entry {
    uint64_t hashKey;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
} polyglot_book_entry;

extern uint64_t entryCount;
extern polyglot_book_entry *entries;

void loadBook();
void unloadBook();

//Returns a newly allocated move struct.
move_c getBookMove(bitboard* board);

#endif