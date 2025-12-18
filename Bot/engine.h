#ifndef ENGINE
#define ENGINE

#include "bitboard.h"
#include "moves.h"

double evaluate(bitboard* board);
move calculateNextMove(bitboard* board, double alpha, double beta);

#endif