#include "binpack/generate.h"
#include "analyze/book.h"
#include "analyze/engine.h"
#include "board/bitboard.h"

binpackDetails details;

void generate()
{
    binpackPrintInfo(TRAINING_DATA_PATH);
    unloadBook();
    int concurrency = threadCount;
    threadCount = 1;
    int wasDebugEnabled = printDebugMessages;
    if(wasDebugEnabled) disableDebugMessages();

    suppressUCIMessages = 1;
    searchThreadContext* contextList = calloc(concurrency, sizeof(searchThreadContext));

    THREADTYPE* threadList = calloc(concurrency, sizeof(THREADTYPE));
    details = binpack_open(TRAINING_DATA_PATH, 0);

    clock_t endTime = LONG_MAX;
    for(int i = 0; i < concurrency; i++)
    {
        contextList[i].board = create_board(NULL);
        contextList[i].endTime = &endTime;
        contextList[i].maxDepth = 9;
        contextList[i].maxNodes = 5000;

        #ifdef NNUE
        contextList[i].accumulator = calloc(1, sizeof(accumulator));
        contextList[i].refreshTable = createRefreshTable();
        #endif

        contextList[i].tt = create_hashTable_tt();

        THREAD_START(threadList[i], generateWorkerThread, &contextList[i]);
    }

    printf("Started generating data. Type 'stop' to stop.\n");
    char buffer[128];
    char* strtok_ptr;
    const char* delim = " \t\r\n";

    while(fgets(buffer, sizeof(buffer), stdin) != NULL)
    {
        char* token = _strtok(buffer, delim, &strtok_ptr);
        while(token != NULL) 
        {
            if(strcmp(token, "stop") == 0) 
                goto cleanup;
            
            token = _strtok(NULL, delim, &strtok_ptr);
        }
    }

    cleanup:

    printf("Stopping all threads...\n");
    endTime = 0;
    for(int i = 0; i < concurrency; i++) 
    {
        THREAD_WAIT(threadList[i]);
        destroy_hashTable_tt(contextList[i].tt);
        free(contextList[i].board);
        #ifdef NNUE
        destroyRefreshTable(contextList[i].refreshTable);
        free(contextList[i].accumulator);
        #endif
    }
    threadCount = concurrency;
    suppressUCIMessages = 0;
    binpack_close(&details);
    if(wasDebugEnabled) enableDebugMessages();
    loadBook(BOOK_PATH);
    printf("Stopped generating data.\n");
    
    binpackPrintInfo(TRAINING_DATA_PATH);
}

THREAD_RETURN generateWorkerThread(THREAD_PARAM param)
{
    searchThreadContext* context = (searchThreadContext*) param;
    THREADTYPE searchThread = THREAD_INIT;

    int movesThisGame = 0;
    int isNewGame = 1;

    Viri_PackedBoard* packedBoard = calloc(1, sizeof(Viri_PackedBoard));
    Viri_MoveScorePair* pairList = calloc(MAX_POSITIONS_PER_GAME, sizeof(pairList));

    uint8_t hit;
    
    while(*context->endTime)
    {
        context->searchedMoves[0].raw = 0;
        context->pv.line[0].raw = 0;

        THREAD_START(searchThread, calculateBestMove, param);
        THREAD_WAIT(searchThread);

        if(IS_IN_BOOK_OPENING(context->board->flags))
            continue;
        if(isNewGame)
        {
            table_entry_tt rootEntry = transposition_table_get(context->board, context->tt, &hit, 0);
            boardToPackedBoard(context->board, packedBoard);
            packedBoard->score = rootEntry.evaluation;
            isNewGame = 0;
            continue;
        }
        move_c bestMove = context->pv.line[0];
        if(!IS_VALID_MOVE(bestMove))
        {
            if(context->board->turn == WHITE)
                packedBoard->result = VIRI_BLACK_WIN;
            else
                packedBoard->result = VIRI_WHITE_WIN;

            binpack_writeGame(&details, packedBoard, pairList, movesThisGame);
            movesThisGame = 0;
            isNewGame = 1;
            
            memset(context->searchedMoves, 0, MAX_REQUIRED_MOVES * sizeof(move_c));
            load_fen_string_to_board(context->board, STARTPOS_FEN);

            continue;
         }

        if(movesThisGame < MAX_POSITIONS_PER_GAME)
        {
            int piece = findPieceOnSquare(context->board, bestMove.startSquare);
            int difference = bestMove.endSquare - bestMove.startSquare;

            //Root entry in private TT should always hit.
            table_entry_tt rootEntry = transposition_table_get(context->board, context->tt, &hit, 0);

            pairList[movesThisGame].move.endSquare = bestMove.endSquare;
            pairList[movesThisGame].move.startSquare = bestMove.startSquare;
            pairList[movesThisGame].move.promotePiece = promoteMappingsToViri[bestMove.promoteTo];
            if(bestMove.promoteTo) 
                pairList[movesThisGame].move.moveType = VIRI_MOVE_TYPE_PROMOTIONS;
            else if(ISKING(piece) && abs(difference) == 2)
            {
                pairList[movesThisGame].move.moveType = VIRI_MOVE_TYPE_CASTLING;

                //Reformat move as king-takes rook
                uint64_t rookMask = context->board->pieces[ROOK | COLOR(piece)];
                if(difference > 0)
                    rookMask &= (~0 << (bestMove.startSquare + 1));
                else
                    rookMask &= singleBitMask(bestMove.startSquare) - 1;

                pairList[movesThisGame].move.endSquare = __builtin_ctzll(rookMask);
            }
            else if(ISPAWN(piece) && bestMove.endSquare == context->board->enPassantSquare)
                pairList[movesThisGame].move.moveType = VIRI_MOVE_TYPE_ENPASSANT;
            else
                pairList[movesThisGame].move.moveType = VIRI_MOVE_TYPE_NONE;

            //White-relative
            pairList[movesThisGame].score = (ISWHITE(context->board->turn)) ? rootEntry.evaluation : -rootEntry.evaluation;

            movesThisGame++;
        }

        //End of game or invalid move.
        if(moveFromStruct(context->board, bestMove))
        {
            movesThisGame--;
            if(!isDraw(context->board))
            {
                packedBoard->result = getMateResult(context->board);
                switch(packedBoard->result)
                {
                    case VICTOR_BLACK:
                        packedBoard->result = VIRI_BLACK_WIN;
                        break;
                    case VICTOR_WHITE:
                        packedBoard->result = VIRI_WHITE_WIN;
                        break;
                    default:
                        packedBoard->result = VIRI_DRAW;
                        break;
                }
            }
            else
                packedBoard->result = VIRI_DRAW;;
            binpack_writeGame(&details, packedBoard, pairList, movesThisGame);
            movesThisGame = 0;
            isNewGame = 1;
            
            load_fen_string_to_board(context->board, STARTPOS_FEN);

            continue;
        }
    }

    free(packedBoard);
    free(pairList);
    return 0;
}