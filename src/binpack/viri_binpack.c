#include "binpack/viri_binpack.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "debug.h"


#if defined(_WIN32) || defined(_WIN64)

//Readers only.
int mmap_open(const char* filename, mmap_handle_t* handle) 
{
    handle->data = NULL;
    handle->size = 0;

    handle->file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
    if(handle->file_handle == INVALID_HANDLE_VALUE) return -1;

    LARGE_INTEGER file_size;
    if(!GetFileSizeEx(handle->file_handle, &file_size)) 
    {
        CloseHandle(handle->file_handle);
        return -1;
    }
    handle->size = (size_t)file_size.QuadPart;

    handle->mapping_handle = CreateFileMappingA(handle->file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
    if(!handle->mapping_handle) 
    {
        CloseHandle(handle->file_handle);
        return -1;
    }

    handle->data = MapViewOfFile(handle->mapping_handle, FILE_MAP_READ, 0, 0, 0);
    if(!handle->data) 
    {
        CloseHandle(handle->mapping_handle);
        CloseHandle(handle->file_handle);
        return -1;
    }
    return 0;
}

void mmap_close(mmap_handle_t* handle) 
{
    if(!handle->data) return;
    UnmapViewOfFile(handle->data);
    CloseHandle(handle->mapping_handle);
    CloseHandle(handle->file_handle);
    handle->data = NULL;
}
#else
int mmap_open(const char* filename, mmap_handle_t* handle) 
{
    handle->data = NULL;
    handle->size = 0;

    handle->fd = open(filename, O_RDONLY);
    if(handle->fd == -1) return -1;

    struct stat sb;
    if(fstat(handle->fd, &sb) == -1) 
    {
        close(handle->fd);
        return -1;
    }
    handle->size = (size_t)sb.st_size;

    handle->data = mmap(NULL, handle->size, PROT_READ, MAP_SHARED, handle->fd, 0);
    if(handle->data == MAP_FAILED) 
    {
        close(handle->fd);
        return -1;
    }
    madvise(handle->data, handle->size, MADV_RANDOM);

    return 0;
}

void mmap_close(mmap_handle_t* handle) {
    if(!handle->data) return;
    munmap(handle->data, handle->size);
    close(handle->fd);
    handle->data = NULL;
}

#endif


uint8_t pieceMappingsFromViri[16];
uint8_t pieceMappingsToViri[16];
uint8_t promoteMappingsFromViri[4];
uint8_t promoteMappingsToViri[10];
uint8_t rookSqToFlag[64];
int init = 0;

void initViriTables()
{
    if(init) return;
    init = 1;

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
                ((KINGSIDE_CASTLE_WHITE(board->flags) && sq > board->kingSquare[WHITE]) ||
                (QUEENSIDE_CASTLE_WHITE(board->flags) && sq < board->kingSquare[WHITE])))
                    piece = VIRI_UNMOVED_ROOK;
            else if(ISBLACK(pc) && 
                    ((KINGSIDE_CASTLE_BLACK(board->flags) && sq > board->kingSquare[BLACK]) ||
                    (QUEENSIDE_CASTLE_BLACK(board->flags) && sq < board->kingSquare[BLACK])))
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

void packedBoardToBoard(bitboard* board, Viri_PackedBoard* packedBoard)
{
    board->turn = packedBoard->stm == VIRI_BLACK_STM;

    board->halfMoveCount = 2 * (packedBoard->fullMoveCounter) + board->turn;
    board->movesSinceLastChange = packedBoard->halfmoveClock;

    //Clear board
    memset(&board->pieceArr, EMPTY_PIECE, 64 * sizeof(uint8_t));
    memset(&board->pieces, 0, PIECE_COUNT * sizeof(uint64_t));
    memset(&board->pieces_side, 0, 2 * sizeof(uint64_t));
    board->pieces_all = 0;

    board->kingSquare[WHITE] = 64;
    board->kingSquare[BLACK] = 64;

    uint64_t mask = packedBoard->occupancy;
    int offset = 0;
    while(mask)
    {
        int sq = __builtin_ctzll(mask);
        int byteIndex = offset / 2;
        int piece = packedBoard->pieces[byteIndex];
        if(offset % 2) piece = piece&0xF;
        else piece >>= 4;

        if(piece&6)
            board->flags |= rookSqToFlag[sq];

        piece = pieceMappingsFromViri[piece];

        uint64_t sqMask = singleBitMask(sq);
        board->pieces_all |= sqMask;
        board->pieces_side[COLOR(piece)] |= sqMask;
        board->pieces[piece] |= sqMask;
        board->pieceArr[sq] = piece;

        if(ISKING(piece))
            board->kingSquare[COLOR(piece)] = sq;

        offset++;
        mask &= (mask - 1);
    }
    
    //IsThreatened can attempt to index out of bounds and segfault on invalid king squares.
    assert(board->kingSquare[WHITE] != 64);
    assert(board->kingSquare[BLACK] != 64);

    if(isThreatened(board, board->kingSquare[WHITE], WHITE)) board->flags|=16;
    else if(isThreatened(board, board->kingSquare[BLACK], BLACK)) board->flags|=32;
}

int readPackedBoard(binpackDetails* details, int readerIndex)
{
    assert(readerIndex < details->numReaders);
    readerDetails* rDetails = &details->readerInfo[readerIndex];

    if(rDetails->current_ptr + sizeof(rDetails->packedBoard->occupancy) > details->end_ptr)
        return -1;
        
    memcpy(&rDetails->packedBoard->occupancy, rDetails->current_ptr, sizeof(rDetails->packedBoard->occupancy));
    
    if(rDetails->packedBoard->occupancy)
    {
        if(rDetails->current_ptr + sizeof(*rDetails->packedBoard) > rDetails->end_section)
            return -1;

        memcpy((uint8_t*)rDetails->packedBoard + sizeof(rDetails->packedBoard->occupancy), 
               rDetails->current_ptr + sizeof(rDetails->packedBoard->occupancy), 
               sizeof(*rDetails->packedBoard) - sizeof(rDetails->packedBoard->occupancy));
               
        rDetails->current_ptr += sizeof(*rDetails->packedBoard);
    }
    else
    {
        uint16_t extension_id = 0;
        uint16_t payload_length = 0;

        memcpy(&extension_id, rDetails->current_ptr + sizeof(rDetails->packedBoard->occupancy), sizeof(extension_id));
        memcpy(&payload_length, rDetails->current_ptr + sizeof(rDetails->packedBoard->occupancy) + sizeof(extension_id), sizeof(payload_length));

        #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
        extension_id = __builtin_bswap16(extension_id);
        payload_length = __builtin_bswap16(payload_length);
        #endif

        uint64_t total_extension_bytes = sizeof(rDetails->packedBoard->occupancy) + 4 + payload_length;
        
        if(rDetails->current_ptr + total_extension_bytes + sizeof(Viri_PackedBoard) > details->end_ptr) return -1;

        rDetails->current_ptr += total_extension_bytes;
        memcpy(rDetails->packedBoard, rDetails->current_ptr, sizeof(Viri_PackedBoard));
        rDetails->current_ptr += sizeof(Viri_PackedBoard);
    }

    #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    packedBoard->occupancy = __builtin_bswap64(packedBoard->occupancy);
    packedBoard->fullMoveCounter = __bultin_bswap16(packedBoard->fullMoveCounter);
    packedBoard->score = __bultin_bswap16(packedBoard->score);
    #endif
    
    if(rDetails->packedBoard->result == VIRI_WHITE_WIN) rDetails->currentGameWinner = VICTOR_WHITE;
    else if(rDetails->packedBoard->result == VIRI_BLACK_WIN) rDetails->currentGameWinner = VICTOR_BLACK;
    else rDetails->currentGameWinner = VICTOR_DRAW_GENERIC;

    packedBoardToBoard(rDetails->board, rDetails->packedBoard);

    return 0;
}

//Truncate the newly opened file if it doesn't end in four zero bytes.
void clearCorruptedGame(const char* fileName)
{
    FILE* file = fopen(fileName, "rb+");
    if(!file) return;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if(file_size < 4) return;

    uint32_t tail;
    fseek(file, -4, SEEK_END);
    fread(&tail, sizeof(uint32_t), 1, file);

    //File terminates correctly
    if(!tail) return;

    long search_pos = file_size - 4;
    long clean_offset = 0;
    uint32_t window = 0xFFFFFFFF;

    while(search_pos >= 0) 
    {
        fseek(file, search_pos, SEEK_SET);
        uint8_t byte;
        fread(&byte, 1, 1, file);
        
        window = (window << 8) | byte;
        
        if(!window)
        {
            clean_offset = search_pos + 4; 
            break;
        }
        search_pos--;
    }

    #if defined(_WIN32) || defined(_WIN64)
        _chsize_s(_fileno(file), clean_offset);
    #else
        ftruncate(fileno(file), clean_offset);
    #endif

    fclose(file);
}

binpackDetails binpack_open(const char* fileName, int numReaders)
{
    initViriTables();

    binpackDetails details = {0};
    details.numReaders = numReaders;

    clearCorruptedGame(fileName);

    if(numReaders <= 0) 
    {
        details.binpack = fopen(fileName, "ab+");
        if(!details.binpack) 
        {
            printf("Failed to open binpack file for writing: %s\n", fileName);
            exit(1);
        }

        details.readerInfo = NULL;
    } 
    else
    {
        if(mmap_open(fileName, &details.mmap_file)) 
        {
            printf("Failed to memory map binpack file: %s\n", fileName);
            exit(1);
        }
        details.start_ptr = (uint8_t*)details.mmap_file.data;
        details.end_ptr = details.start_ptr + details.mmap_file.size;

        details.headerOffsets = binpack_acquireHeaderIndices(fileName, &details.headerEntries);

        details.readerInfo = calloc(numReaders, sizeof(readerDetails));

        size_t sectionSize = details.headerEntries / numReaders;

        for(int i = 0; i < numReaders; i++)
        {
            details.readerInfo[i].board = calloc(1, sizeof(bitboard));
            details.readerInfo[i].packedBoard = calloc(1, sizeof(Viri_PackedBoard));
            details.readerInfo[i].start_section = details.start_ptr + details.headerOffsets[i * sectionSize];
            details.readerInfo[i].current_ptr = details.readerInfo[i].start_section;
            details.readerInfo[i].end_section = details.start_ptr + details.headerOffsets[(i + 1) * sectionSize - 1];

            readPackedBoard(&details, i);
        }
    }

    CREATE_MUTEX(details.lock);

    return details;
}

void binpack_close(binpackDetails* details)
{
    if(details->numReaders <= 0 && details->binpack)
    {
        fclose(details->binpack);
        details->binpack = NULL;
    }
    else 
    {
        mmap_close(&details->mmap_file);
        for(int i = 0; i < details->numReaders; i++)
        {
            free(details->readerInfo[i].board);
            free(details->readerInfo[i].packedBoard);
        }
        free(details->readerInfo);
        free(details->headerOffsets);
    }

    DESTROY_MUTEX(details->lock);

    if(details->writtenThisSession)
    {
        details->lastPrintTime = clock();
        double duration = (double) ((details->lastPrintTime - details->startTime) / CLOCKS_PER_SEC);
        printf("\rGenerated %d positions this session (%.2f pos/sec)\n", details->writtenThisSession, details->writtenThisSession / duration);
        details->writtenThisSession = 0;
    }
}

int binpack_next(binpackDetails* details, int readerIndex, bitboard* brd, Viri_Score* eval, uint8_t* result, int loop, int minimumFENSkips)
{
    assert(readerIndex < details->numReaders);
    readerDetails* rDetails = &details->readerInfo[readerIndex];

    Viri_MoveScorePair pair = {0};
    int skippedUnusable = 0;
    int skippedUsable = 0;

    //Break once we find a good move.
    while(1)
    {
        int isCapture = 0;
        if(rDetails->current_ptr + sizeof(Viri_MoveScorePair) > rDetails->end_section)
        {
            if(loop)
            {
                rDetails->current_ptr = rDetails->start_section;
                readPackedBoard(details, readerIndex);
            }
            else
                return (skippedUnusable + skippedUsable > 0) ? skippedUnusable + skippedUsable : -1;
        }
        else
        {
            memcpy(&pair, rDetails->current_ptr, sizeof(Viri_MoveScorePair));
            rDetails->current_ptr += sizeof(Viri_MoveScorePair);

            #if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
            pair.move.raw = __builtin_bswap16(pair.move.raw);
            #endif

            if(pair.raw == 0)
            {
                if(readPackedBoard(details, readerIndex))
                    return (skippedUnusable + skippedUsable > 0) ? skippedUnusable + skippedUsable : -1;
            }
            else
            {

                //For the purpose of speed, bypass the legal-move checks in moveFromStruct
                move_c m = {
                    .startSquare = pair.move.startSquare,
                    .endSquare = pair.move.endSquare
                };
                
                if(pair.move.moveType == VIRI_MOVE_TYPE_PROMOTIONS)
                    m.promoteTo = promoteMappingsToViri[pair.move.promotePiece];

                int toPc = findPieceOnSquare(rDetails->board, m.endSquare);
                int fromPc = findPieceOnSquare(rDetails->board, m.startSquare);

                if(toPc != EMPTY_PIECE)
                {
                    //Viriformat castling is king takes rook
                    if(ISKING(fromPc) && COLOR(fromPc) == COLOR(toPc))
                    {
                        if(m.endSquare < m.startSquare)
                            m.endSquare += 2 - (m.endSquare & 0x7);
                        else
                            m.endSquare += 6 - (m.endSquare & 0x7);
                    }
                    else
                        isCapture = 1; //Doesn't include en passant, but that is handled by move type.
                }
                
                    

                rDetails->board->repetitionIndex = 0;
                rDetails->board->historyIndex = 0;
                
                if(movePiece(rDetails->board, m))
                {
                    //Skip to end of game.
                    uint8_t* start = rDetails->current_ptr;
                    while(rDetails->current_ptr + 4 <= rDetails->end_section) 
                    {
                        uint32_t value;
                        memcpy(&value, rDetails->current_ptr, 4);
                        
                        if(value == 0) 
                            break;
                        rDetails->current_ptr++;
                    }
                    skippedUnusable += 1 + ((rDetails->current_ptr - start) / 4);
                    continue;
                }
            }
        }

        if(!isCapture &&
            abs(pair.score) <= 2000 && 
            pair.move.moveType == 0 &&
            !IS_IN_CHECK_ANY(rDetails->board->flags) &&
            rDetails->board->halfMoveCount >= 8)
            {
                if(skippedUsable >= minimumFENSkips)
                    break;
                else
                    skippedUsable++;
            }
            else
                skippedUnusable++;
    }

    *result = rDetails->currentGameWinner;
    *eval = (ISWHITE(rDetails->board->turn)) ? pair.score : -pair.score;
    *brd =  *rDetails->board;
    return skippedUnusable + skippedUsable;
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

void binpackPrintInfo(const char* fileName)
{
    binpackDetails details = binpack_open(fileName, 1);
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

    uint64_t recentlyRead = 0;

    clock_t readStartTime = clock();
    while((temp = binpack_next(&details, 0, &b, &s, &r, 0, 0)) != -1)
    {
        skippedCount += temp;
        usedCount++;

        recentlyRead+= 1 + temp;

        //Printout roughly every 10,000,000 entries.
        if(recentlyRead > 10000000)
        {
            recentlyRead = 0;

            uint64_t totalCount = usedCount + skippedCount;
            printf("\033[4A");
            printf("\033[2K\rBinpack statistics:\n");
            printf("\033[2K\r\tTotal: %llu\n", totalCount);
            printf("\033[2K\r\tUsable: %llu (%.1f%%)\n", usedCount, (100.0 * usedCount) / totalCount);
            printf("\033[2K\r\tSkipped: %llu (%.1f%%)\n", skippedCount, (100.0 * skippedCount) / totalCount);
            fflush(stdout);
        }
    }
    clock_t readEndTime = clock();
    double duration = (readEndTime - readStartTime) /  (double) CLOCKS_PER_SEC;
    uint64_t totalCount = usedCount + skippedCount;
    double positionsPerSecond = totalCount / duration;

    uint64_t byteLength = details.mmap_file.size;
    float positionsPerByte = (float) byteLength / totalCount;

    printf("\033[4A");
    printf("\033[2K\rBinpack statistics:\n");
    printf("\033[2K\r\tTotal: %llu\n", totalCount);
    printf("\033[2K\r\tUsable: %llu (%.1f%%)\n", usedCount, (100.0 * usedCount) / totalCount);
    printf("\033[2K\r\tSkipped: %llu (%.1f%%)\n", skippedCount, (100.0 * skippedCount) / totalCount);
    printf("\033[2K\r\tDuration: %.3f seconds\n", duration);
    printf("\033[2K\r\tPositions per second: %.4f\n", positionsPerSecond);
    printf("\033[2K\r\tByte Size: %llu\n", (uint64_t)byteLength);
    printf("\033[2K\r\tPositions per Byte: %.4f\n", positionsPerByte);

    binpack_close(&details);
}


//Doesn't yet handle the 0-bitboard special entries.
int64_t* binpack_acquireHeaderIndices(const char* fileName, int* headerEntries) 
{
    char dataFileName[256];
    char* extension = strstr(fileName, ".viri");
    size_t base_len = extension - fileName;
    snprintf(dataFileName, sizeof(dataFileName), "%.*s.head", (int)base_len, fileName);

    FILE* headerIndices = fopen(dataFileName, "rb");
    if(!headerIndices)
    {
        printf("Generating missing .head file...\n");

        headerIndices = fopen(dataFileName, "wb");
        FILE* binpack = fopen(fileName, "rb");

        Viri_MoveScorePair pairBuffer;
        Viri_PackedBoard packedBoardBuffer;

        int64_t offsetIndex = ftell_64(binpack);
        fwrite(&offsetIndex, sizeof(int64_t), 1, headerIndices);
        fread(&packedBoardBuffer, sizeof(Viri_PackedBoard), 1, binpack);

        int gameCount = 1;
        *headerEntries = 1;

        while(1)
        {
            if(fread(&pairBuffer, sizeof(Viri_MoveScorePair), 1, binpack) < 1)
                break;
            else if(pairBuffer.raw == 0)
            {
                if(++gameCount % 1000 == 0)
                {
                    offsetIndex = ftell_64(binpack);
                    fwrite(&offsetIndex, sizeof(int64_t), 1, headerIndices);
                    if(gameCount % 1000000 == 0) printf("\rPre-processing Games: %d", gameCount);
                    *headerEntries = *headerEntries + 1;
                }
                fread(&packedBoardBuffer, sizeof(Viri_PackedBoard), 1, binpack);
            }
        }
        printf("\rPre-processing Games: %d\n", gameCount);\

        fclose(binpack);
        fclose(headerIndices);
        headerIndices = fopen(dataFileName, "rb");
    }

    fseek_64(headerIndices, 0, SEEK_END);
    *headerEntries = ftell_64(headerIndices) / sizeof(int64_t);
    rewind(headerIndices);
    int64_t* indices = calloc(*headerEntries, sizeof(int64_t));
    fread(indices, sizeof(int64_t), *headerEntries, headerIndices);
    fclose(headerIndices);
    return indices;

}