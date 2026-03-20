#ifndef BITBOARD_H

#define BITBOARD_H

#include "../structs.h"
#include "../hashtables/repetitiontable.h"

#define WHITE 0x10
#define BLACK 0x20

#define FLIP_COLOR(x) (x^(WHITE|BLACK))

#define DRAW 0x30
#define STALEMATED_WHITE 0x40
#define STALEMATED_BLACK 0x80
#define THREEFOLD 0x100
#define FIFTYMOVERULE 0x200
#define INSUFFICIENT_MATERIAL 0x400

#define ISDRAW(victor) ((victor&DRAW) == DRAW)

#define ISWHITE(piece) ((piece&WHITE) == WHITE)
#define ISBLACK(piece) ((piece&BLACK) == BLACK)

#define PAWN 0x1
#define KNIGHT 0x2
#define BISHOP 0x3
#define ROOK 0x4
#define QUEEN 0x5
#define KING 0x6

#define ISPAWN(piece) ((piece&0xF) == PAWN)
#define ISKNIGHT(piece) ((piece&0xF) == KNIGHT)
#define ISBISHOP(piece) ((piece&0xF) == BISHOP)
#define ISROOK(piece) ((piece&0xF) == ROOK)
#define ISQUEEN(piece) ((piece&0xF) == QUEEN)
#define ISKING(piece) ((piece&0xF) == KING)

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

#define INCHECK_W(flag) ((flag&16)>>4)
#define INCHECK_B(flag) ((flag&32)>>5)
#define CHECK_W(flag) (flag|=16)
#define CHECK_B(flag) (flag|=32)
#define UNCHECK_W(flag) (flag&=(~16))
#define UNCHECK_B(flag) (flag&=(~32))

#define REMOVE = 0x1000
#define SHOULDREMOVE(piece) (piece&REMOVE == REMOVE)

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

int getColumn(int square);
int getRow(int square);
char getColumnChar(int x, int isSquare);
void getSquareName(int square, char* target);
int getSquareNumber(char* squareName);
int findPieceOnSquare(bitboard* board, int square);

//Required by pyrrhic.
//Return index of popped bit in range [0,63]
int popLSB(uint64_t *bitboard);

bitboard* create_board();
bitboard* create_board_from_fen(const char* fileName, int lineNumber);
void load_fen_to_board(bitboard* board, const char* fileName, int lineNumber);
void destroy_board(bitboard* board);
void copy_board(bitboard* dest, bitboard* source, int copyHT);

void board_clear_square(bitboard* board, int square, int pieceType);
void board_set(bitboard* board, int square, int piece);

void piece_print(char boardArray[8][9], uint64_t piece, char printChar);
void board_print(bitboard* board, int printValues, int printHistory);
void values_print(bitboard* board);
void bitmask_print(uint64_t mask, char fill);

int moves_push(bitboard* board, move* m);
move* moves_pop(bitboard* board);
move* createMove(int startSquare, int endSquare, int promoteTo, int piece, int capturedPiece, int capturedPieceSquare, bitboard* prevBoard);

void dumpMoves(bitboard* board);
#endif