#ifndef BITBOARD_H

#define BITBOARD_H

#include "../types.h"
#include "../hashtables/hashtable.h"
#include "../debug.h"

#define WHITE 0
#define BLACK 1

#define PIECE(x) (x&(~1))
#define COLOR(x) (x&1)
#define FLIP_COLOR(x) (x^1)

#define ISWHITE(piece) ((piece&1) == WHITE)
#define ISBLACK(piece) ((piece&1) == BLACK)

#define PAWN 0
#define KNIGHT 2
#define BISHOP 4
#define ROOK 6
#define QUEEN 8
#define KING 10

#define WHITE_PAWN 0
#define WHITE_KNIGHT 2
#define WHITE_BISHOP 4
#define WHITE_ROOK 6
#define WHITE_QUEEN 8
#define WHITE_KING 10

#define BLACK_PAWN 1
#define BLACK_KNIGHT 3
#define BLACK_BISHOP 5
#define BLACK_ROOK 7
#define BLACK_QUEEN 9
#define BLACK_KING 11

#define EMPTY_PIECE 14

#define ISPAWN(piece) ((piece&0xE) == PAWN)
#define ISKNIGHT(piece) ((piece&0xE) == KNIGHT)
#define ISBISHOP(piece) ((piece&0xE) == BISHOP)
#define ISROOK(piece) ((piece&0xE) == ROOK)
#define ISQUEEN(piece) ((piece&0xE) == QUEEN)
#define ISKING(piece) ((piece&0xE) == KING)

/**
 * flags&1 == canKingsideCastle_w
 * flags&2 == canQueensideCastle_w 
 * flags&4 == canKingsideCastle_b
 * flags&8 == canQueensideCastle_b
 * flags&16 == in_check_w
 * flags&32 == in_check_b
 */
#define KINGSIDE_CASTLE_WHITE(flag) (flag&1)
#define QUEENSIDE_CASTLE_WHITE(flag) ((flag&2)>>1)
#define KINGSIDE_CASTLE_BLACK(flag) ((flag&4)>>2)
#define QUEENSIDE_CASTLE_BLACK(flag) ((flag&8)>>3)
#define BAN_KINGCASTLE_W(flag) (flag&=(~1))
#define BAN_QUEENCASTLE_W(flag) (flag&=(~2))
#define BAN_KINGCASTLE_B(flag) (flag&=(~4))
#define BAN_QUEENCASTLE_B(flag) (flag&=(~8))

#define IS_IN_CHECK_W(flag) ((flag&16)>>4)
#define IS_IN_CHECK_B(flag) ((flag&32)>>5)
#define CHECK_W(flag) (flag|=16)
#define CHECK_B(flag) (flag|=32)
#define UNCHECK_W(flag) (flag&=(~16))
#define UNCHECK_B(flag) (flag&=(~32))

#define VICTOR_WHITE 0x1
#define VICTOR_BLACK 0x2
#define VICTOR_DRAW_INSUFFICIENT_MATERIAL 0x4
#define VICTOR_DRAW_THREEFOLD 0x8
#define VICTOR_DRAW_STALEMATE_WHITE 0x10
#define VICTOR_DRAW_STALEMATE_BLACK 0x20
#define VICTOR_DRAW_FIFTY_MOVE_RULE 0x40

#define columnNames "abcdefgh"

#define DARK_SQUARES 0xAA55AA55AA55AA55
#define LIGHT_SQUARES 0x55AA55AA55AA55AA

//Background = 48; Foreground = 38
//RGB values follow the 2;
#define TEXT_BOLD "\033[1m"
#define TEXT_COLOR_LIGHT_SQUARE_WHITE_PIECE "\033[48;2;115;101;80;38;2;255;255;255m"
#define TEXT_COLOR_LIGHT_SQUARE_BLACK_PIECE "\033[48;2;115;101;80;38;2;255;100;100m"
#define TEXT_COLOR_DARK_SQUARE_WHITE_PIECE "\033[48;2;59;37;23;38;2;255;255;255m"
#define TEXT_COLOR_DARK_SQUARE_BLACK_PIECE "\033[48;2;59;37;23;38;2;255;100;100m"
#define TEXT_COLOR_DESTINATION_WHITE_PIECE "\033[48;2;180;180;50;38;2;255;255;255m"
#define TEXT_COLOR_DESTINATION_BLACK_PIECE "\033[48;2;180;180;50;38;2;255;100;100m"
#define TEXT_COLOR_SOURCE_WHITE_PIECE "\033[48;2;130;130;50;38;2;255;255;255m"
#define TEXT_COLOR_SOURCE_BLACK_PIECE "\033[48;2;130;130;50;38;2;255;100;100m"
#define TEXT_NONE "\033[0m"

#define getColumn(square) (square%8)
#define getRow(square) (square/8)

void getSquareName(int square, char* target);
int getSquareNumber(char* squareName);
#define findPieceOnSquare(board, square) (board->pieceArr[square])

//Required by pyrrhic.
//Return index of popped bit in range [0,63]
int popLSB(uint64_t *bitboard);

bitboard* create_board();
void export_fen_from_board(bitboard* board, char* outputFenString);
void load_fen_string_to_board(bitboard* board, const char* fenString);

static inline void board_clear_square(bitboard* board, int square)
{
    assert(board);
    assert(square >= 0 && square <= 63);

    int pieceType = findPieceOnSquare(board, square);
    if(pieceType == EMPTY_PIECE) return;

    board->pieces[pieceType] &= ~singleBitMask(square);
    board->pieces_side[COLOR(pieceType)] &= ~singleBitMask(square);
    board->pieces_all &= ~singleBitMask(square);
    board->pieceArr[square] = EMPTY_PIECE;
    board->hashCode ^= zobrist_piece_keys[pieceType][square];

}

//Assumes the target square is empty.
static inline void board_set(bitboard* board, int square, int piece)
{
    assert(board);
    assert(square >= 0 && square <= 63);
    assert(piece != EMPTY_PIECE);
    
    board->pieces[piece] |= singleBitMask(square);
    board->pieces_side[COLOR(piece)] |= singleBitMask(square);
    board->pieces_all |= singleBitMask(square);
    board->pieceArr[square] = piece;
    board->hashCode ^= zobrist_piece_keys[piece][square];
}

//Allows for capture.
static inline void board_move_piece(bitboard* board, int from, int to)
{
    int targetPiece = findPieceOnSquare(board, to);
    int piece = findPieceOnSquare(board, from);
    assert(piece != EMPTY_PIECE);

    board->hashCode ^= zobrist_piece_keys[piece][from] ^ zobrist_piece_keys[piece][to];
    
    uint64_t mask = singleBitMask(from) | singleBitMask(to);
    board->pieces[piece] ^= mask;
    board->pieces_side[COLOR(piece)] ^= mask;

    board->pieces_all &= ~singleBitMask(from);
    board->pieces_all |= singleBitMask(to);
    
    if(targetPiece != EMPTY_PIECE)
    {
        board->hashCode ^= zobrist_piece_keys[targetPiece][to];
        board->pieces_side[COLOR(targetPiece)] &= ~singleBitMask(to);
        board->pieces[targetPiece] &= ~singleBitMask(to);
    }   
    
    board->pieceArr[to] = piece;
    board->pieceArr[from] = EMPTY_PIECE;
}

//We know that some moves aren't captures. (e.g. castle, pawn pushes, unmoves) 
static inline void board_move_piece_quietly(bitboard* board, int from, int to)
{
    int piece = findPieceOnSquare(board, from);
    assert(piece != EMPTY_PIECE);

    board->hashCode ^= zobrist_piece_keys[piece][from] ^ zobrist_piece_keys[piece][to];
    
    uint64_t mask = singleBitMask(from) | singleBitMask(to);
    board->pieces[piece] ^= mask;
    board->pieces_side[COLOR(piece)] ^= mask;
    board->pieces_all ^= mask;
    
    board->pieceArr[to] = piece;
    board->pieceArr[from] = EMPTY_PIECE;
}

void piece_print(char boardArray[8][9], uint64_t piece, char printChar);
void board_print(bitboard* board, int printValues);
void values_print(bitboard* board);
void bitmask_print(uint64_t mask, char fill);

int moves_push(bitboard* board, move m);
move moves_pop(bitboard* board);
static inline void createMove(move* m, int startSquare, int endSquare, int promoteTo, int piece, bitboard* board)
{
    assert(m);
    assert(startSquare >= 0 && startSquare <= 63);
    assert(endSquare >= 0 && endSquare <= 63);

    m->startSquare = startSquare;
    m->endSquare = endSquare;
    m->promoteTo = promoteTo;
    m->piece = piece;
    m->capturedPiece = EMPTY_PIECE;
    m->capturedPieceSquare = endSquare;

    m->previousMovesSinceLastChange = board->movesSinceLastChange;
    m->prevEnPassantSquare = board->enPassantSquare;
    m->prevFlags = board->flags;
}

int containsRepetition(bitboard* board);
#endif