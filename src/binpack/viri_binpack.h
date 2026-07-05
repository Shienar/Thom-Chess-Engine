#ifndef BINPACK
#define BINPACK

#include "types.h"
#include "debug.h"

//Reusing the format from https://github.com/cosmobobak/viriformat

typedef int16_t Viri_Score; //White-relative

#define VIRI_PAWN 0
#define VIRI_KNIGHT 1
#define VIRI_BISHOP 2
#define VIRI_ROOK 3
#define VIRI_QUEEN 4
#define VIRI_KING 5
#define VIRI_UNMOVED_ROOK 6
#define VIRI_BLACK_PIECE 8
#define VIRI_BLACK_STM 1
#define VIRI_BLACK_WIN 0
#define VIRI_DRAW 1
#define VIRI_WHITE_WIN 2

#define VIRI_MOVE_TYPE_NONE 0
#define VIRI_MOVE_TYPE_ENPASSANT 1
#define VIRI_MOVE_TYPE_CASTLING 2 
#define VIRI_MOVE_TYPE_PROMOTIONS 3 //includes captures 
#define VIRI_PROMOTETO_KNIGHT 0
#define VIRI_PROMOTETO_BISHOP 1
#define VIRI_PROMOTETO_ROOK 2
#define VIRI_PROMOTETO_QUEEN 3

#pragma pack(push, 1)
typedef struct {
    uint64_t occupancy;
    uint8_t pieces[16];
    uint8_t ep : 7;
    uint8_t stm  : 1;
    uint8_t halfmoveClock; //50-move rule tracking
    uint16_t fullMoveCounter;
    Viri_Score score;
    uint8_t result;
    uint8_t padding;
} Viri_PackedBoard;
typedef union {
    uint16_t raw;
    struct {
        uint16_t startSquare : 6;
        uint16_t endSquare : 6;
        uint16_t promotePiece : 2;
        uint16_t moveType : 2;
    };
} Viri_Move;

typedef struct {
    Viri_Move move;
    Viri_Score score;
} Viri_MoveScorePair;
#pragma pack(pop);

extern uint8_t pieceMappingsFromViri[16];
extern uint8_t pieceMappingsToViri[16];
extern uint8_t promoteMappingsFromViri[4];
extern uint8_t promoteMappingsToViri[10];
extern uint8_t rookSqToFlag[64];

//Same file pointer for all functions. The assumption is that you will either only read or only write.
void binpack_open(const char* fileName, int writer);
void binpack_close();
void binpack_next(bitboard* brd, Viri_Score* eval, uint8_t* result);

void boardToPackedBoard(bitboard* board, Viri_PackedBoard* packedBoard);
void binpack_writeGame(Viri_PackedBoard* packedBoard, Viri_MoveScorePair* pairList, int count);
#endif