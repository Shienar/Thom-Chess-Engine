#ifndef BINPACK
#define BINPACK

#include "types.h"
#include "debug.h"

//Reusing the format from https://github.com/cosmobobak/viriformat

typedef int16_t Viri_Score; //White-relative

#define TRAINING_DATA_PATH PROJECT_CWD "/import/trainingData.viri"
#define VALIDATION_DATA_PATH PROJECT_CWD "/import/validationData.viri"

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

typedef union {
    uint32_t raw;
    struct {
        Viri_Move move;
        Viri_Score score;
    };
} Viri_MoveScorePair;
#pragma pack(pop)

//Each reader gets 1 / N of the binpack to themselves. Used for training, not for binpackinfo.
typedef struct {
    bitboard* board;
    Viri_PackedBoard* packedBoard;
    uint8_t currentGameWinner;
    
    uint8_t* start_section;
    uint8_t* current_ptr;
    uint8_t* end_section;
} readerDetails;

typedef struct {
    uint8_t numReaders; // A writer has 0 (or negative) readers.
    readerDetails* readerInfo;

    //Reading (Global)
    mmap_handle_t mmap_file;
    uint8_t* start_ptr;
    uint8_t* end_ptr;
    int64_t* headerOffsets;
    int headerEntries; //To save space, every 50th entry is saved.

    //Writing
    FILE* binpack; 
    mutex_t lock;
    int writtenThisSession;
    clock_t lastPrintTime;
    clock_t startTime;
} binpackDetails;

extern uint8_t pieceMappingsFromViri[16];
extern uint8_t pieceMappingsToViri[16];
extern uint8_t promoteMappingsFromViri[4];
extern uint8_t promoteMappingsToViri[10];
extern uint8_t rookSqToFlag[64];

//Same file pointer for all functions. The assumption is that you will either only read or only write.
binpackDetails binpack_open(const char* fileName, int numReaders);
void binpack_close(binpackDetails* details);
int binpack_next(binpackDetails* details, int readerIndex, bitboard* brd, Viri_Score* eval, uint8_t* result, int loop, int minimumFENSkips);

int readPackedBoard(binpackDetails* details, int readerIndex);
void boardToPackedBoard(bitboard* board, Viri_PackedBoard* packedBoard);
void binpack_writeGame(binpackDetails* details, Viri_PackedBoard* packedBoard, Viri_MoveScorePair* pairList, int count);

void binpackPrintInfo(const char* fileName);
int64_t* binpack_acquireHeaderIndices(const char* fileName, int* headerEntries);
#endif