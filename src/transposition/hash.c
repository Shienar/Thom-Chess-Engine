#include "transposition/hash.h"
#include "board/bitboard.h"
#include <stdint.h>

/**
 * - Indexes 0-767 are for piece-square values.
 *  - Index = 64 * piece type + square
 * - Index 768 = white can kingside castle
 * - Index 769 = white can queenside castle
 * - Index 770 = black can kingside castle
 * - Index 771 = black can queenside castle
 * - Index 772-779 = En passant file.
 *  - 772 + file in range 0-7
 * - Index 780 = is white's turn
 */
uint64_t zobrist_keys[781] = { 0 };

//https://en.wikipedia.org/wiki/Xorshift#xorshift*
uint64_t rng_xorshift64(uint64_t* seed) 
{
    uint64_t x = *seed;
    x ^= x << 12;
    x ^= x >> 25;
    x ^= x << 27;
    *seed = x;
    return x * 0x2545F4914F6CDD1DULL;
}

uint64_t zobrist_piece_keys[PIECE_COUNT][64];
void initZobristPieceKeys()
{
    uint64_t zobrist_seed = 0x314023;

    for(int i = 0; i < 781; i++)
        zobrist_keys[i] = rng_xorshift64(&zobrist_seed);
 
    for(int piece = 0; piece < PIECE_COUNT; piece++)
        for(int square = 0; square < 64; square++)
            zobrist_piece_keys[piece][square] = zobrist_keys[64 * piece + square];
}

uint64_t getEnPassantHash(bitboard* board)
{
    if(board->enPassantSquare == NO_EP_SQUARE) 
        return 0;

    int epSquare = board->enPassantSquare;
    int epRow = getRow(epSquare);
    uint64_t borderingPawnMask = 0;

    int capturedPawnSquare = (epRow == 5) ? epSquare - 8 : epSquare + 8;

    if(getColumn(capturedPawnSquare) > 0) borderingPawnMask |= (1ull << (capturedPawnSquare - 1));
    if(getColumn(capturedPawnSquare) < 7) borderingPawnMask |= (1ull << (capturedPawnSquare + 1));

    if(epRow == 5)
    {
        if(borderingPawnMask & board->pieces[WHITE_PAWN]) 
            return zobrist_keys[772 + getColumn(epSquare)]; 
    } 
    else
    {
        if(borderingPawnMask & board->pieces[BLACK_PAWN]) 
            return zobrist_keys[772 + getColumn(epSquare)];
    }

    return 0;
}

void generateHashCode(bitboard* board)
{
    if(!board) return;

    board->hashCode = 0;
    board->pawnHash = 0;
    board->nonPawnHash[WHITE] = 0;
    board->nonPawnHash[BLACK] = 0;

    int piece;
    uint64_t mask = board->pieces_all;
    while(mask)
    {
        int square = __builtin_ctzll(mask);

        piece = findPieceOnSquare(board, square);

        if(piece != EMPTY_PIECE)
        {
            board->hashCode^=zobrist_piece_keys[piece][square];

            if(ISPAWN(piece))
                board->pawnHash ^= zobrist_piece_keys[piece][square];
            else
                board->nonPawnHash[COLOR(piece)] ^= zobrist_piece_keys[piece][square];
        }

        mask&=(mask - 1);
    }
    
    if(board->canKingsideCastle_w)  board->hashCode^=zobrist_keys[768];
    if(board->canQueensideCastle_w) board->hashCode^=zobrist_keys[769];
    if(board->canKingsideCastle_b)  board->hashCode^=zobrist_keys[770];
    if(board->canQueensideCastle_b) board->hashCode^=zobrist_keys[771];


    board->hashCode ^= getEnPassantHash(board);

    if(ISWHITE(board->turn)) board->hashCode^=zobrist_keys[780];
}