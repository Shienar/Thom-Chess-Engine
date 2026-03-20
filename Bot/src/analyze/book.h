#ifndef BOOK
#define BOOK

#include "../structs.h"

extern uint64_t entryCount;
extern polyglot_book_entry *entries;

void loadBook();
void unloadBook();

//Returns a newly allocated move struct.
move* getBookMove(bitboard* board);

#endif