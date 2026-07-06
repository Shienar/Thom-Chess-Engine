#include "binpack/viri_binpack.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "debug.h"

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE
#include <io.h>

uint8_t pieceMappingsFromViri[16];
uint8_t pieceMappingsToViri[16];
uint8_t promoteMappingsFromViri[4];
uint8_t promoteMappingsToViri[10];
uint8_t rookSqToFlag[64];

int readPackedBoard(binpackDetails* details)
{
    fread(&details->packedBoard.occupancy, sizeof(details->packedBoard.occupancy), 1, details->binpack);

    if(details->packedBoard.occupancy)
        fread((uint8_t*) &details->packedBoard + sizeof(details->packedBoard.occupancy), sizeof(details->packedBoard) - sizeof(details->packedBoard.occupancy), 1, details->binpack);
    else
    {
        uint16_t extension_id = 0;
        uint16_t payload_length = 0;

        fread(&extension_id, sizeof(extension_id), 1, details->binpack);
        fread(&payload_length, sizeof(payload_length), 1, details->binpack);
        
        #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
        extension_id = __builtin_bswap16(extension_id);
        payload_length = __builtin_bswap16(payload_length);
        #endif

        long bytes_to_skip = (long)payload_length + 4;

        //Skip past the reserved extensions.
        fseek_64(details->binpack, bytes_to_skip, SEEK_CUR);

        fread(&details->packedBoard, sizeof(Viri_PackedBoard), 1, details->binpack);
    }

    #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    packedBoard.occupancy = __builtin_bswap64(packedBoard.occupancy);
    packedBoard.fullMoveCounter = __bultin_bswap16(packedBoard.fullMoveCounter);
    packedBoard.score = __bultin_bswap16(packedBoard.score);
    #endif

    details->board.turn = details->packedBoard.stm == VIRI_BLACK_STM;

    if(details->packedBoard.result == VIRI_WHITE_WIN) details->currentGameWinner = VICTOR_WHITE;
    else if(details->packedBoard.result == VIRI_BLACK_WIN) details->currentGameWinner = VICTOR_BLACK;
    else details->currentGameWinner = VICTOR_DRAW_GENERIC;

    details->board.halfMoveCount = 2 * (details->packedBoard.fullMoveCounter) + details->board.turn;
    details->board.movesSinceLastChange = details->packedBoard.halfmoveClock;

    //Clear board
    memset(&details->board.pieceArr, EMPTY_PIECE, 64 * sizeof(uint8_t));
    memset(&details->board.pieces, 0, PIECE_COUNT * sizeof(uint64_t));
    memset(&details->board.pieces_side, 0, 2 * sizeof(uint64_t));
    details->board.pieces_all = 0;

    uint64_t mask = details->packedBoard.occupancy;
    int offset = 0;
    while(mask)
    {
        int sq = __builtin_ctzll(mask);
        int byteIndex = offset / 2;
        int piece = details->packedBoard.pieces[byteIndex];
        if(offset % 2) piece = piece&0xF;
        else piece >>= 4;

        if(piece&6)
            details->board.flags |= rookSqToFlag[sq];

        piece = pieceMappingsFromViri[piece];

        uint64_t sqMask = singleBitMask(sq);
        details->board.pieces_all |= sqMask;
        details->board.pieces_side[COLOR(piece)] |= sqMask;
        details->board.pieces[piece] |= sqMask;
        details->board.pieceArr[sq] = piece;

        if(ISKING(piece))
        {
            if(ISBLACK(piece)) details->board.kingSquare_b = sq;
            else details->board.kingSquare_w = sq;
        }

        offset++;
        mask &= (mask - 1);
    }
    
    if(isThreatened(&details->board, details->board.kingSquare_w, WHITE)) details->board.flags|=16;
    else if(isThreatened(&details->board, details->board.kingSquare_b, BLACK)) details->board.flags|=32;

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
void clearCorruptedGame(binpackDetails* details)
{
    fseek(details->binpack, 0, SEEK_END);
    long file_size = ftell(details->binpack);
    if(file_size < 4) return;

    uint32_t tail;
    fseek(details->binpack, -4, SEEK_END);
    fread(&tail, sizeof(uint32_t), 1, details->binpack);

    //File terminates correctly
    if(!tail) return;

    long search_pos = file_size - 4;
    long clean_offset = 0;
    uint32_t window = 0xFFFFFFFF;

    while(search_pos >= 0) 
    {
        fseek(details->binpack, search_pos, SEEK_SET);
        uint8_t byte;
        fread(&byte, 1, 1, details->binpack);
        
        window = (window << 8) | byte;
        
        if(!window)
        {
            clean_offset = search_pos + 4; 
            break;
        }
        search_pos--;
    }

    #ifdef _WIN32
        _chsize_s(_fileno(details->binpack), clean_offset);
    #else
        ftruncate(fileno(details->binpack), clean_offset);
    #endif
}

binpackDetails binpack_open(const char* fileName, int writer)
{
    FILE* binpackFile = fopen(fileName, "rb+");
    if(!binpackFile)
    {
        binpackFile = fopen(fileName, "wb+");
        if(!binpackFile)
        {
            printf("Failed to open binpack file %s", fileName);
            exit(1);
        }
    }
    mutex_t mutex = THREAD_INIT;
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

    binpackDetails d = {0};
    d.binpack = binpackFile;
    d.lock = mutex;

    clearCorruptedGame(&d);
    if(!writer)
    {
        rewind(binpackFile);
        readPackedBoard(&d);
    }

    return d;
}

void binpack_close(binpackDetails* details)
{
    if(details->binpack) 
    {
        fclose(details->binpack);
        details->binpack = NULL;

        DESTROY_MUTEX(details->lock);
        details->lock = THREAD_INIT;

        if(details->writtenThisSession)
        {
            details->lastPrintTime = clock();
            double duration = (double) ((details->lastPrintTime - details->startTime) / CLOCKS_PER_SEC);
            printf("\rGenerated %d positions this session (%.2f pos/sec)\n", details->writtenThisSession, details->writtenThisSession / duration);
            details->writtenThisSession = 0;
        }
    }
}

int binpack_next(binpackDetails* details, bitboard* brd, Viri_Score* eval, uint8_t* result, int loop)
{
    Viri_MoveScorePair pair = {0};
    int skippedCount = 0;

    //Break once we find a good move.
    while(1)
    {
        if(fread(&pair, sizeof(Viri_MoveScorePair), 1, details->binpack) < 1)
        {
            if(loop)
            {
                fseek_64(details->binpack, 0, SEEK_SET);
                readPackedBoard(details);
            }
            else 
                return (skippedCount > 0) ? skippedCount : -1;
        }
        else
        {
            if(pair.move.raw == 0)
            {
                readPackedBoard(details);
            }
            else
            {

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
                    int fromPc = findPieceOnSquare((&details->board), m.startSquare);
                    int toPc = findPieceOnSquare((&details->board), m.endSquare);

                    //Viriformat castling is king takes rook
                    if(toPc != EMPTY_PIECE && COLOR(fromPc) == COLOR(toPc))
                    {
                        if(m.endSquare < m.startSquare)
                            m.endSquare += 2 - (m.endSquare & 0x7);
                        else
                            m.endSquare += 6 - (m.endSquare & 0x7);
                    }
                }

                details->board.repetitionIndex = 0;
                details->board.historyIndex = 0;
                if(movePiece(&details->board, m)) continue;;
            }
        }

        if(abs(pair.score) <= 10000 && 
            pair.move.moveType == 0 &&
            !IS_IN_CHECK_ANY(details->board.flags) &&
            details->board.halfMoveCount > 6)
                break;
        
        skippedCount++;
    }

    *result = details->currentGameWinner;
    *eval = (ISWHITE(details->board.turn)) ? pair.score : -pair.score;
    *brd =  details->board;
    return skippedCount;
}

void binpack_writeGame(binpackDetails* details, Viri_PackedBoard* packedBoard, Viri_MoveScorePair* pairList, int count)
{

    #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    packedBoard->occupancy = __builtin_bswap64(packedBoard->occupancy);
    packedBoard->fullMoveCounter = __bultin_bswap16(packedBoard->fullMoveCounter);
    packedBoard->score = __bultin_bswap16(packedBoard->score);
    #endif

    LOCK_MUTEX(details->lock);

    fwrite(packedBoard, sizeof(Viri_PackedBoard), 1, details->binpack);
    #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
        for(int i = 0; i < count; i++)
        {
            pairList[i].move = __builtin_bswap16(pairList[i].move);
            pairList[i].score = __bultin_bswap16(pairList[i].score);
        }
    #endif

    fwrite(pairList, sizeof(Viri_MoveScorePair), count, details->binpack);

    uint32_t zero = 0;
    fwrite(&zero, 4, 1, details->binpack);

    details->writtenThisSession+=count;

    if((clock() - details->lastPrintTime) / CLOCKS_PER_SEC > 10)
    {
        details->lastPrintTime = clock();
        double duration = (double) ((details->lastPrintTime - details->startTime) / CLOCKS_PER_SEC);
        printf("\rGenerated %d positions this session (%.2f pos/sec)", details->writtenThisSession, details->writtenThisSession / duration);
    }

    UNLOCK_MUTEX(details->lock);
}

//Normally 21-22m positions per second (mostly usable)/
//GPU is the biggest bottleneck, followed by CPU input proccessing, followed by CPU binpack I/O
void binpackPrintInfo(const char* fileName)
{
    binpackDetails details = binpack_open(fileName, 0);
    uint64_t usedCount = 1;
    uint64_t skippedCount = 0;
    
    int temp;
    bitboard b;
    Viri_Score s;
    uint8_t r;

    printf("Binpack statistics:\n");
    printf("\tUsable: %llu\n", usedCount);
    printf("\tSkipped: %llu\n", skippedCount);
    printf("\tTotal: %llu\n", usedCount + skippedCount);
    fflush(stdout);

    int i = 0;
    clock_t readStartTime = clock();
    while((temp = binpack_next(&details, &b, &s, &r, 0)) != -1)
    {
        skippedCount += temp;
        usedCount++;

        //Printout every 1,000,000 usable entries.
        if((i = i + 1) % 1000000 == 0)
        {
            printf("\033[4A");
            printf("\033[2K\rBinpack statistics:\n");
            printf("\033[2K\r\tTotal: %llu\n", usedCount + skippedCount);
            printf("\033[2K\r\tUsable: %llu\n", usedCount);
            printf("\033[2K\r\tSkipped: %llu\n", skippedCount);
            fflush(stdout);
        }
    }
    clock_t readEndTime = clock();
    double duration = (readEndTime - readStartTime) /  (double) CLOCKS_PER_SEC;
    uint64_t totalCount = usedCount + skippedCount;
    double positionsPerSecond = totalCount / duration;

    uint64_t byteLength = ftell_64(details.binpack);
    float positionsPerByte = (float) byteLength / totalCount;

    printf("\033[4A");
    printf("\033[2K\rBinpack statistics:\n");
    printf("\033[2K\r\tTotal: %llu\n", totalCount);
    printf("\033[2K\r\tUsable: %llu\n", usedCount);
    printf("\033[2K\r\tSkipped: %llu\n", skippedCount);
    printf("\033[2K\r\tDuration: %.4f\n", duration);
    printf("\033[2K\r\tPositions per second: %.4f\n", positionsPerSecond);
    printf("\033[2K\r\tByte Size: %llu\n", (unsigned long long)byteLength);
    printf("\033[2K\r\tPositions per Byte: %.4f\n", positionsPerByte);

    binpack_close(&details);
}