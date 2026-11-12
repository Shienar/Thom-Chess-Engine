// Shared datatypes between board.* and moves.*

#pragma once

#include <stdint.h>

#define MAX_MOVES 218
#define PIECE_COUNT 12
#define PIECE_TYPE_COUNT 6

typedef union move {
    uint16_t raw;
    struct {
        uint16_t startSquare : 6;
        uint16_t endSquare   : 6;
        uint16_t promoteTo   : 4;
    };
} move;

typedef struct moveIterator {
    move moveList[MAX_MOVES];
    int16_t moveScores[MAX_MOVES];
    uint8_t count;
    
    int16_t selectedIndex;
    uint8_t quietIndex;
    uint8_t badCaptureIndex;
    uint8_t badCaptureCount;

    uint8_t visitedGood;
    uint8_t visitedQuiet;
    uint8_t visitedBad;

    struct bitboard* board;

    uint8_t stage;
    uint8_t type; 

    move excludedMove;
    move ttMove;
    move killer1Move;
    move killer2Move;
    move counterMove;
    move followUpMove;

    struct threadContext* context;
} moveIterator;

typedef struct repetitionVector {
    uint64_t* hashCodes;
    int capacity;
} repetitionVector;

//sq = 8 * row + column; row, column in [0, 7]
typedef struct bitboard {
    uint64_t hashCode;
    uint64_t pawnHash;
    uint64_t nonPawnHash[2];

    uint64_t pieces[PIECE_COUNT];
    uint64_t pieces_side[2];
    uint64_t pieces_all;

    uint16_t halfMoveCount;
    uint16_t pliesFromNull;

    uint8_t pieceArr[64];
    uint8_t kingSquare[2];
    uint8_t halfmoveClock;
    int8_t enPassantSquare;

    uint8_t canKingsideCastle_w  : 1;
    uint8_t canQueensideCastle_w : 1;
    uint8_t canKingsideCastle_b  : 1;
    uint8_t canQueensideCastle_b : 1;
    uint8_t in_check             : 1;
    uint8_t turn                 : 1;
} bitboard;