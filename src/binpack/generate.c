#include "binpack/generate.h"
#include "analyze/book.h"
#include "analyze/search.h"
#include "board/bitboard.h"

binpackDetails details;
int exitWhile;

void generate(const char* path)
{
    exitWhile = 0;
    int concurrency = threadCount;
    threadCount = 1;
    
    int wasDebugEnabled = printDebugMessages;
    if(wasDebugEnabled) 
        disableDebugMessages();

    suppressUCIMessages = 1;
    searchThreadContext* contextList = calloc(concurrency, sizeof(searchThreadContext));

    THREADTYPE* threadList = calloc(concurrency, sizeof(THREADTYPE));
    binpack_open(&details, path, 0);
    details.startTime = clock();

    for(int i = 0; i < concurrency; i++)
    {
        contextList[i].board = create_board(NULL);
        contextList[i].startTime = 0,
        contextList[i].hardEndTime = LONG_MAX,
        contextList[i].softEndTime = LONG_MAX,
        contextList[i].maxDepth = 9;
        contextList[i].maxNodes = 5000;
        contextList[i].abortFlag = calloc(1, sizeof(uint8_t));

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
    exitWhile = 1;

    printf("Stopping all threads...\n");
    for(int i = 0; i < concurrency; i++) 
    {
        *contextList[i].abortFlag = 1;
        THREAD_WAIT(threadList[i]);
        destroy_hashTable_tt(contextList[i].tt);
        free(contextList[i].board);
        free(contextList[i].abortFlag);
    }
    
    threadCount = concurrency;
    suppressUCIMessages = 0;
    
    binpack_close(&details);
    if(wasDebugEnabled) 
        enableDebugMessages();
    printf("Stopped generating data.\n");
    
    binpackPrintInfo(path);
}

uint32_t rng_xorshift32(uint32_t* seed) 
{
    uint32_t x = *seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *seed = x;
    return x;
}

/**
 * Make eight random moves on the board, creating some random opening position. 
 * Do a search on this position, returning me a score and a move. 
 * The score gets applied to the current position and the move creates the next position.
 * The final move of the game doesn't get included in the binpack.
 * 
 * If the initial position is noisy (abs(cp) > 300), skip that game and try another opening.
 * 
 * If one side is dominant (> 2000 cp) for 5 moves, adjudicate an early win.
 * If both sides are tied for 5 moves after move 40 (cp in [-10, +10]), adjudicate an early draw
 * 
 * If the score is a mate score, we can terminate early & don't need to play out the full continuation.
 */
THREAD_RETURN generateWorkerThread(THREAD_PARAM param)
{
    searchThreadContext* context = (searchThreadContext*) param;
    THREADTYPE searchThread = THREAD_INIT;

    int movesThisGame = 0;
    int isNewGame = 1;

    Viri_PackedBoard* packedBoard = calloc(1, sizeof(Viri_PackedBoard));
    Viri_MoveScorePair* pairList = calloc(MAX_POSITIONS_PER_GAME, sizeof(Viri_MoveScorePair));

    srand(time(NULL));
    uint32_t seed = rand();
    
    //Adjudication
    int consecutiveHighScores = 0;
    int consecutiveLowScores = 0;
    int consecutiveDrawScores = 0;

    while(!exitWhile)
    {
        if(isNewGame)
        {
            load_fen_string_to_board(context->board, STARTPOS_FEN);

            //Play some random moves
            //Early mate is possible, goto newloop is used as a break + continue;
            for(int i = 0; i <= 8; i++)
            {
                move_c moveList[MAX_MOVES] = {0};
                int count = generateMoveList(moveList, context->board, 0);
                if(!count)
                    goto newloop;
                int index = rng_xorshift32(&seed) % count;
                if(moveFromStruct(context->board, moveList[index]))
                    goto newloop;
            }
            context->board->historyIndex = 0;

            isNewGame = 0;
            movesThisGame = 0;
            consecutiveHighScores = 0;
            consecutiveLowScores = 0;
            consecutiveDrawScores = 0;

            boardToPackedBoard(context->board, packedBoard);
            packedBoard->result = UINT8_MAX;
            continue;
        }

        assert(context->board->pieces[WHITE_KING] && context->board->pieces[BLACK_KING]);

        THREAD_START(searchThread, calculateBestMove, param);
        THREAD_WAIT(searchThread);

        move_c bestMove = context->pv.line[0];

        int whiteEval = (ISWHITE(context->board->turn)) ? context->score : - context->score;

        if(whiteEval > MIN_MATE_SCORE)
            packedBoard->result = VIRI_WHITE_WIN;
        else if(whiteEval < -MIN_MATE_SCORE)
            packedBoard->result = VIRI_BLACK_WIN;

        consecutiveHighScores = (whiteEval > 2000) ? consecutiveHighScores + 1 : 0;
        consecutiveLowScores = (whiteEval < -2000) ? consecutiveLowScores + 1 : 0;
        consecutiveDrawScores = (whiteEval > -10 && whiteEval < 10) ? consecutiveDrawScores + 1 : 0;

        if(consecutiveHighScores > 5) packedBoard->result = VIRI_WHITE_WIN;
        else if(consecutiveLowScores > 5) packedBoard->result = VIRI_BLACK_WIN;
        else if(consecutiveDrawScores > 5 && context->board->repetitionIndex > 40) packedBoard->result = VIRI_DRAW;

        if(packedBoard->result < UINT8_MAX)
        {
            binpack_writeGame(&details, packedBoard, pairList, movesThisGame - 1);
            isNewGame = 1;
            continue;
        }

        if(bestMove.raw == 0)
            goto gameover;

        int piece = findPieceOnSquare(context->board, bestMove.startSquare);
        int difference = bestMove.endSquare - bestMove.startSquare;

        pairList[movesThisGame].move.endSquare = bestMove.endSquare;
        pairList[movesThisGame].move.startSquare = bestMove.startSquare;
        pairList[movesThisGame].move.promotePiece = promoteMappingsToViri[bestMove.promoteTo];
        if(bestMove.promoteTo) 
            pairList[movesThisGame].move.moveType = VIRI_MOVE_TYPE_PROMOTIONS;
        else if(ISKING(piece) && abs(difference) == 2)
        {
            pairList[movesThisGame].move.moveType = VIRI_MOVE_TYPE_CASTLING;

            //Reformat move as king-takes rook (Assumes standard variant)
            int rowOffset = getRow(bestMove.startSquare) * 8;
            if(bestMove.endSquare > bestMove.startSquare)
                pairList[movesThisGame].move.endSquare = rowOffset + 7;
            else
                pairList[movesThisGame].move.endSquare = rowOffset;
        }
        else if(ISPAWN(piece) && bestMove.endSquare == context->board->enPassantSquare)
            pairList[movesThisGame].move.moveType = VIRI_MOVE_TYPE_ENPASSANT;
        else
            pairList[movesThisGame].move.moveType = VIRI_MOVE_TYPE_NONE;

        if(movesThisGame == 0)
            packedBoard->whiteScore = whiteEval;
        else
            pairList[movesThisGame - 1].whiteScore = whiteEval;

        movesThisGame++;
        if(movesThisGame >= MAX_POSITIONS_PER_GAME)
        {
            //Determine game result without saving extra moves.
            do
            {
                THREAD_START(searchThread, calculateBestMove, param);
                THREAD_WAIT(searchThread);
            }while(!moveFromStruct(context->board, context->pv.line[0]));
            goto gameover;
        }

        //End of game
        if(moveFromStruct(context->board, bestMove))
        {
            gameover:

            if(isDraw(context->board))
                packedBoard->result = VIRI_DRAW;
            else
            {
                //Should get detected early, this is a fallback.
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

            binpack_writeGame(&details, packedBoard, pairList, movesThisGame - 1);
            isNewGame = 1;
            continue;
        }

        newloop:
    }

    free(packedBoard);
    free(pairList);
    return 0;
}