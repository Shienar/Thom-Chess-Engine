#include "src/binpack/viri_binpack.h"
#include "src/board/bitboard.h"
#include "src/board/moves.h"
#include "debug.h"
#include <io.h>

mutex_t mutex = THREAD_INIT;
FILE* binpackFile = NULL;
bitboard board;

Viri_PackedBoard packedBoard;
uint8_t currentGameWinner;
uint8_t pieceMappingsFromViri[16];
uint8_t pieceMappingsToViri[16];
uint8_t promoteMappingsFromViri[4];
uint8_t promoteMappingsToViri[10];
uint8_t rookSqToFlag[64];
int writtenThisSession = 0;
clock_t lastPrintTime;
clock_t startTime;

int readPackedBoard()
{
    fread(&packedBoard.occupancy, sizeof(packedBoard.occupancy), 1, binpackFile);

    if(packedBoard.occupancy)
        fread((uint8_t*) &packedBoard + sizeof(packedBoard.occupancy), sizeof(packedBoard) - sizeof(packedBoard.occupancy), 1, binpackFile);
    else
    {
        uint16_t extension_id = 0;
        uint16_t payload_length = 0;

        fread(&extension_id, sizeof(extension_id), 1, binpackFile);
        fread(&payload_length, sizeof(payload_length), 1, binpackFile);
        
        #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
        extension_id = __builtin_bswap16(extension_id);
        payload_length = __builtin_bswap16(payload_length);
        #endif

        long bytes_to_skip = (long)payload_length + 4;

        //Skip past the reserved extensions.
        fseek_64(binpackFile, bytes_to_skip, SEEK_CUR);

        fread(&packedBoard, sizeof(Viri_PackedBoard), 1, binpackFile);
    }

    #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    packedBoard.occupancy = __builtin_bswap64(packedBoard.occupancy);
    packedBoard.fullMoveCounter = __bultin_bswap16(packedBoard.fullMoveCounter);
    packedBoard.score = __bultin_bswap16(packedBoard.score);
    #endif

    board.turn = packedBoard.stm == VIRI_BLACK_STM;

    if(packedBoard.result == VIRI_WHITE_WIN) currentGameWinner = VICTOR_WHITE;
    else if(packedBoard.result == VIRI_BLACK_WIN) currentGameWinner = VICTOR_BLACK;
    else currentGameWinner = VICTOR_DRAW_GENERIC;

    board.halfMoveCount = 2 * (packedBoard.fullMoveCounter) + board.turn;
    board.movesSinceLastChange = packedBoard.halfmoveClock;

    //Clear board
    memset(&board.pieceArr, EMPTY_PIECE, 64 * sizeof(uint8_t));
    memset(&board.pieces, 0, PIECE_COUNT * sizeof(uint64_t));
    memset(&board.pieces_side, 0, 2 * sizeof(uint64_t));
    board.pieces_all = 0;

    uint64_t mask = packedBoard.occupancy;
    int offset = 0;
    while(mask)
    {
        int sq = __builtin_ctzll(mask);
        int byteIndex = offset / 2;
        int piece = packedBoard.pieces[byteIndex];
        if(offset % 2) piece = piece&0xF;
        else piece >>= 4;

        if(piece&6)
            board.flags |= rookSqToFlag[sq];

        piece = pieceMappingsFromViri[piece];

        uint64_t sqMask = singleBitMask(sq);
        board.pieces_all |= sqMask;
        board.pieces_side[COLOR(piece)] |= sqMask;
        board.pieces[piece] |= sqMask;
        board.pieceArr[sq] = piece;

        if(ISKING(piece))
        {
            if(ISBLACK(piece)) board.kingSquare_b = sq;
            else board.kingSquare_w = sq;
        }

        offset++;
        mask &= (mask - 1);
    }
    
    if(isThreatened(&board, board.kingSquare_w, WHITE)) board.flags|=16;
    else if(isThreatened(&board, board.kingSquare_b, BLACK)) board.flags|=32;

    return 0;
}

//Doesn't set score or game result, that can be done later by the caller.
void boardToPackedBoard(bitboard* board, Viri_PackedBoard* packedBoard)
{
    packedBoard->occupancy = board->pieces_all;

    memset(packedBoard->pieces, 0, 16 * sizeof(uint8_t));
    uint64_t mask = packedBoard->occupancy;
    int insertIndex = 0;
    while(mask)
    {
        int sq = __builtin_ctzll(mask);
        int pc = findPieceOnSquare(board, sq);
        int piece = pieceMappingsToViri[pc];
        if(ISROOK(pc))
        {
            if(ISWHITE(pc) && 
                ((KINGSIDE_CASTLE_WHITE(board->flags) && sq > board->kingSquare_w) ||
                (QUEENSIDE_CASTLE_WHITE(board->flags) && sq < board->kingSquare_w)))
                    piece = VIRI_UNMOVED_ROOK;
            else if(ISBLACK(pc) && 
                    ((KINGSIDE_CASTLE_BLACK(board->flags) && sq > board->kingSquare_b) ||
                    (QUEENSIDE_CASTLE_BLACK(board->flags) && sq < board->kingSquare_b)))
                        piece = VIRI_UNMOVED_ROOK | VIRI_BLACK_PIECE;
        }
    
        if((insertIndex&1) == 0)
            piece <<= 4;
        
        packedBoard->pieces[insertIndex / 2] |= piece;
        insertIndex++;

        mask &= (mask-1);
    }

    packedBoard->stm = board->turn;
    packedBoard->ep = board->enPassantSquare;

    packedBoard->halfmoveClock = board->movesSinceLastChange;
    packedBoard->fullMoveCounter = board->halfMoveCount / 2;
}

//Truncate the newly opened file if it doesn't end in four zero bytes.
void clearCorruptedGame()
{
    fseek(binpackFile, 0, SEEK_END);
    long file_size = ftell(binpackFile);
    if(file_size < 4) return;

    uint32_t tail;
    fseek(binpackFile, -4, SEEK_END);
    fread(&tail, sizeof(uint32_t), 1, binpackFile);

    //File terminates correctly
    if(!tail) return;

    long search_pos = file_size - 4;
    long clean_offset = 0;
    uint32_t window = 0xFFFFFFFF;

    while(search_pos >= 0) 
    {
        fseek(binpackFile, search_pos, SEEK_SET);
        uint8_t byte;
        fread(&byte, 1, 1, binpackFile);
        
        window = (window << 8) | byte;
        
        if(!window)
        {
            clean_offset = search_pos + 4; 
            break;
        }
        search_pos--;
    }

    #ifdef _WIN32
        _chsize_s(_fileno(binpackFile), clean_offset);
    #else
        ftruncate(fileno(binpackFile), clean_offset);
    #endif
}

void binpack_open(const char* fileName, int writer)
{
    binpackFile = fopen(fileName, "rb+");
    if(!binpackFile)
    {
        binpackFile = fopen(fileName, "wb+");
        if(!binpackFile)
        {
            printf("Failed to open binpack file %s", fileName);
            exit(1);
        }
    }
    CREATE_MUTEX(mutex);

    pieceMappingsFromViri[VIRI_PAWN] = WHITE_PAWN;
    pieceMappingsFromViri[VIRI_PAWN | VIRI_BLACK_PIECE] = BLACK_PAWN;

    pieceMappingsFromViri[VIRI_KNIGHT] = WHITE_KNIGHT;
    pieceMappingsFromViri[VIRI_KNIGHT | VIRI_BLACK_PIECE] = BLACK_KNIGHT;

    pieceMappingsFromViri[VIRI_BISHOP] = WHITE_BISHOP;
    pieceMappingsFromViri[VIRI_BISHOP | VIRI_BLACK_PIECE] = BLACK_BISHOP;

    pieceMappingsFromViri[VIRI_ROOK] = WHITE_ROOK;
    pieceMappingsFromViri[VIRI_ROOK | VIRI_BLACK_PIECE] = BLACK_ROOK;
    
    pieceMappingsFromViri[VIRI_UNMOVED_ROOK] = WHITE_ROOK;
    pieceMappingsFromViri[VIRI_UNMOVED_ROOK | VIRI_BLACK_PIECE] = BLACK_ROOK;

    pieceMappingsFromViri[VIRI_QUEEN] = WHITE_QUEEN;
    pieceMappingsFromViri[VIRI_QUEEN | VIRI_BLACK_PIECE] = BLACK_QUEEN;

    pieceMappingsFromViri[VIRI_KING] = WHITE_KING;
    pieceMappingsFromViri[VIRI_KING | VIRI_BLACK_PIECE] = BLACK_KING;

    pieceMappingsToViri[WHITE_PAWN] = VIRI_PAWN;
    pieceMappingsToViri[BLACK_PAWN] = VIRI_PAWN | VIRI_BLACK_PIECE;

    pieceMappingsToViri[WHITE_KNIGHT] = VIRI_KNIGHT;
    pieceMappingsToViri[BLACK_KNIGHT] = VIRI_KNIGHT | VIRI_BLACK_PIECE;

    pieceMappingsToViri[WHITE_BISHOP] = VIRI_BISHOP;
    pieceMappingsToViri[BLACK_BISHOP] = VIRI_BISHOP | VIRI_BLACK_PIECE;

    pieceMappingsToViri[WHITE_ROOK] = VIRI_ROOK;
    pieceMappingsToViri[BLACK_ROOK] = VIRI_ROOK | VIRI_BLACK_PIECE;

    pieceMappingsToViri[WHITE_QUEEN] = VIRI_QUEEN;
    pieceMappingsToViri[BLACK_QUEEN] = VIRI_QUEEN | VIRI_BLACK_PIECE;

    pieceMappingsToViri[WHITE_KING] = VIRI_KING;
    pieceMappingsToViri[BLACK_KING] = VIRI_KING | VIRI_BLACK_PIECE;

    promoteMappingsFromViri[VIRI_PROMOTETO_KNIGHT] = KNIGHT;
    promoteMappingsFromViri[VIRI_PROMOTETO_BISHOP] = BISHOP;
    promoteMappingsFromViri[VIRI_PROMOTETO_ROOK] = ROOK;
    promoteMappingsFromViri[VIRI_PROMOTETO_QUEEN] = QUEEN;

    promoteMappingsToViri[0] = 0;
    promoteMappingsToViri[WHITE_KNIGHT] = KNIGHT;
    promoteMappingsToViri[BLACK_KNIGHT] = KNIGHT;
    promoteMappingsToViri[WHITE_BISHOP] = BISHOP;
    promoteMappingsToViri[BLACK_BISHOP] = BISHOP;
    promoteMappingsToViri[WHITE_ROOK] = ROOK;
    promoteMappingsToViri[BLACK_ROOK] = ROOK;
    promoteMappingsToViri[WHITE_QUEEN] = QUEEN;
    promoteMappingsToViri[BLACK_QUEEN] = QUEEN;
    
    rookSqToFlag[0] = 2;
    rookSqToFlag[7] = 1;
    rookSqToFlag[56] = 8;
    rookSqToFlag[63] = 4;

    writtenThisSession = 0;
    startTime = clock();

    clearCorruptedGame();
    if(!writer)
    {
        rewind(binpackFile);
        readPackedBoard();
    }
}

void binpack_close()
{
    if(binpackFile) 
    {
        fclose(binpackFile);
        binpackFile = NULL;

        DESTROY_MUTEX(mutex);
        mutex = THREAD_INIT;

        lastPrintTime = clock();
        double duration = (double) ((lastPrintTime - startTime) / CLOCKS_PER_SEC);
        printf("\rGenerated %d positions this session (%.2f pos/sec)\n", writtenThisSession, writtenThisSession / duration);
    }
}

void binpack_next(bitboard* brd, Viri_Score* eval, uint8_t* result)
{
    assert(binpackFile);
    Viri_MoveScorePair pair = {0};

    //Break once we find a good move.
    while(1)
    {
        if(fread(&pair, sizeof(Viri_MoveScorePair), 1, binpackFile) < 1)
        {
            fseek_64(binpackFile, 0, SEEK_SET);
            readPackedBoard();
        }
        else
        {

            if(pair.move.raw == 0)
            {
                readPackedBoard();
                continue;
            }

            #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
            pair.move.raw = __builtin_bswap16(pair.move.raw);
            #endif


            //For the purpose of speed, bypass the legal-move checks in moveFromStruct
            move_c m = {
                .startSquare = pair.move.startSquare,
                .endSquare = pair.move.endSquare
            };
            if(pair.move.moveType == VIRI_MOVE_TYPE_PROMOTIONS)
                m.promoteTo = promoteMappingsToViri[pair.move.promotePiece];
            else 
            {
                int fromPc = findPieceOnSquare((&board), m.startSquare);
                int toPc = findPieceOnSquare((&board), m.endSquare);

                //Viriformat castling is king takes rook
                if(toPc != EMPTY_PIECE && COLOR(fromPc) == COLOR(toPc))
                {
                    if(m.endSquare < m.startSquare)
                        m.endSquare += 2 - (m.endSquare & 0x7);
                    else
                        m.endSquare += 6 - (m.endSquare & 0x7);
                }
            }

            board.repetitionIndex = 0;
            board.historyIndex = 0;
        }

        if(abs(pair.score) <= 10000 && 
            pair.move.moveType == 0 &&
            !IS_IN_CHECK_ANY(board.flags) &&
            board.halfMoveCount > 12)
                break;
    }

    *result = currentGameWinner;
    *eval = pair.score;
    *brd =  board;
}

void binpack_writeGame(Viri_PackedBoard* packedBoard, Viri_MoveScorePair* pairList, int count)
{

    #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    packedBoard->occupancy = __builtin_bswap64(packedBoard->occupancy);
    packedBoard->fullMoveCounter = __bultin_bswap16(packedBoard->fullMoveCounter);
    packedBoard->score = __bultin_bswap16(packedBoard->score);
    #endif

    LOCK_MUTEX(mutex);

    fwrite(packedBoard, sizeof(Viri_PackedBoard), 1, binpackFile);
    #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
        for(int i = 0; i < count; i++)
        {
            pairList[i].move = __builtin_bswap16(pairList[i].move);
            pairList[i].score = __bultin_bswap16(pairList[i].score);
        }
    #endif

    fwrite(pairList, sizeof(Viri_MoveScorePair), count, binpackFile);

    uint32_t zero = 0;
    fwrite(&zero, 4, 1, binpackFile);

    writtenThisSession+=count;

    if((clock() - lastPrintTime) / CLOCKS_PER_SEC > 10)
    {
        lastPrintTime = clock();
        double duration = (double) ((lastPrintTime - startTime) / CLOCKS_PER_SEC);
        printf("\rGenerated %d positions this session (%.2f pos/sec)", writtenThisSession, writtenThisSession / duration);
    }

    UNLOCK_MUTEX(mutex);
}