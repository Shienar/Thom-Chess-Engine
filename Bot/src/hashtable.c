#include "../include/hashtable.h"
#include "../include/bitboard.h"
#include "../include/debug.h"
#include <string.h>

uint64_t getHashCode(const char* key)
{
    if(!key)  return 0;

    uint64_t hashCode = FNV_OFFSET_BASIS;
    for(int i = 0; i < 64; i++)
    {
        hashCode *= FNV_PRIME;
        hashCode ^= (uint64_t)(unsigned char)(key[i]);
    }
    return hashCode;
}

void hashKey(bitboard* board, char* key)
{
    if(!board) return;

    memset(key, 'e', 64);
    key[64] = '\0';

    uint64_t mask = 1;
    for(int currentSquare = 0; currentSquare < 64; currentSquare++)
    {
        if(board->pawn_w&mask) key[currentSquare] = 'p';
        else if(board->pawn_b&mask) key[currentSquare] = 'P';
        else if(board->rook_w&mask) key[currentSquare] = 'r';
        else if(board->rook_b&mask) key[currentSquare] = 'R';
        else if(board->knight_w&mask) key[currentSquare] = 'n';
        else if(board->knight_b&mask) key[currentSquare] = 'N';
        else if(board->bishop_w&mask) key[currentSquare] = 'b';
        else if(board->bishop_b&mask) key[currentSquare] = 'B';
        else if(board->queen_w&mask) key[currentSquare] = 'q';
        else if(board->queen_b&mask) key[currentSquare] = 'Q';
        else if(board->king_w&mask) key[currentSquare] = 'k';
        else if(board->king_b&mask) key[currentSquare] = 'K';
        mask = mask<<1;
    }
}