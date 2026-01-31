#include "../include/hashtable.h"
#include "../include/bitboard.h"
#include "../include/debug.h"
#include <string.h>

uint64_t zobrist_pieceSquareValues[64][12];
uint64_t zobrist_blackToMove;
uint64_t zobrist_whiteToMove;
uint64_t zobrist_castle_wk;
uint64_t zobrist_castle_wq;
uint64_t zobrist_castle_bk;
uint64_t zobrist_castle_bq;
uint64_t zobrist_enPassantFile[8];

uint64_t sixty_four_bit_rand()
{
    uint64_t returnValue = 0;
    returnValue|= (uint64_t) rand();
    returnValue|= ((uint64_t) rand())<<16;
    returnValue|= ((uint64_t) rand())<<32;
    returnValue|= ((uint64_t) rand())<<48;
    return returnValue;
}

void generateZobristRandoms()
{
    for(int i = 0; i < 12; i++)
    {
        for(int j = 0; j < 64; j++)
        {
            zobrist_pieceSquareValues[j][i] = sixty_four_bit_rand();
        }
    }

    zobrist_blackToMove = sixty_four_bit_rand();
    zobrist_whiteToMove = sixty_four_bit_rand();
    zobrist_castle_bk = sixty_four_bit_rand();
    zobrist_castle_wk = sixty_four_bit_rand();
    zobrist_castle_bq = sixty_four_bit_rand();
    zobrist_castle_wq = sixty_four_bit_rand();

    for(int i = 0; i < 8; i++)
    {
        zobrist_enPassantFile[i] = sixty_four_bit_rand();
    }
}

uint64_t getHashCode(bitboard* board)
{
    if(!board)  return 0;

    uint64_t returnValue = 0;

    int piece, index;
    for(int i = 0; i < 64; i++)
    {
        piece = findPieceOnSquare(board, i);
        if(piece)
        {
            if(ISWHITE(piece)) index = 0;
            else index = 6;

            index += (piece&0xF) - 1;

            returnValue^=zobrist_pieceSquareValues[i][index];
        }
    }

    if(board->flags&1) returnValue^=zobrist_castle_wk;
    if(board->flags&2) returnValue^=zobrist_castle_wq;
    if(board->flags&4) returnValue^=zobrist_castle_bk;
    if(board->flags&8) returnValue^=zobrist_castle_bq;

    if(ISWHITE(board->turn)) returnValue^=zobrist_whiteToMove;
    else returnValue^=zobrist_blackToMove;

    move* m = board->moveStackTop;
    if(m && ISPAWN(m->piece) &&
        ((ISWHITE(m->piece) && m->endSquare - m->startSquare == 16) ||
        (ISBLACK(m->piece) && m->startSquare - m->endSquare == 16)))
    {
        returnValue^=zobrist_enPassantFile[getColumn(m->endSquare) - 1];
    }

    return returnValue;
}