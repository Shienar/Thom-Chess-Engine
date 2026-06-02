#include "./board/moves.h"
#include "./board/bitboard.h"
#include "debug.h"
#include "./hashtables/transpositiontable.h"
#include "./analyze/book.h"
#include "./analyze/neuralnet.h"
#include "./pyrrhic/tbprobe.h"
#include "./analyze/engine.h"
#include <string.h>
#include <omp.h>

void readyUp(int *isPathDirty, int *useBook, int *isReady, char* sygyzyPath, searchThreadContext* context);

int main(int argc, char** argv)
{
    omp_set_num_threads(threadCount); 
    srand(time(NULL));
    
    bitboard* board = create_board();

    char buffer[4096] = {'\0'};
    const char* delim = " \t\r\n";
    char* str = NULL;
    char* strtok_ptr = NULL;
    
    int quit = 0;
    int useBook = 1;
    int isReady = 0;
    
    char sygyzyPath[1024] = "./sygyzy/";
    int isPathDirty = 1; //Has sygyzy been initialized with the current path?

    clock_t endTime; //Doubles as a terminate flag

    searchThreadContext* threadContext = calloc(1, sizeof(searchThreadContext));
    threadContext->board = board;
    threadContext->depth = MAX_PLY;
    threadContext->maxNodes = INT_MAX;
    threadContext->endTime = &endTime;
    threadContext->board = board;
    threadContext->score = calloc(1, sizeof(searchThreadContext));

    THREADTYPE calculateThread;

    enableDebugMessages();

    while(!quit)
    {
        memset(buffer, 0, 4096 * sizeof(char));
        fgets(buffer, 4096, stdin);
        str = _strtok(buffer, delim, &strtok_ptr);
        while(str != NULL)
        {
            if(strcmp(str, "uci") == 0) 
            {
                printf("id name ChessBot 0.1\n");
                printf("id author name Grant\n");
                printf("option name Hash type spin default 16 min 1 max 4096\n");
                printf("option name Threads type spin default 8 min 1 max 64\n");
                printf("option name Ponder type check default false\n");
                printf("option name OwnBook type check default true\n");
                printf("option name SygyzyPath type string default ./sygyzy/\n");
                printf("option name SygyzyProbeLimit type spin default 5 min 3 max 7\n"); //n-man sygyzy tablebase.
                printf("option name SyzygyProbeDepth type spin default 6 min 5 max 32\n"); //Probe sygyzy at non-root if at least n depth remaining in search.
                printf("uciok\n");
                fflush(stdout);
                break;
            }
            else if(strcmp(str, "ucinewgame") == 0)
            {
                clear_tt(transpositionTable);
                if(useBook) loadBook();

                destroyRefreshTable(playingRefreshTable);
                playingRefreshTable = createRefreshTable();
                break;
            }
            else if(strcmp(str, "setoption") == 0) 
            {
                if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "name") == 0)
                {
                    if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                    {
                        if(strcmp(str, "Hash") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                if(strcmp(str, "value") == 0)
                                {
                                    str = _strtok(NULL, delim, &strtok_ptr);
                                    uint64_t byteSize;
                                    sscanf(str, "%" PRIu64 "", &byteSize);
                                    tt_size_entries = (byteSize * 1024 * 1024) / sizeof(table_entry_tt);
                                    destroy_hashTable_tt(transpositionTable);
                                    transpositionTable = create_hashTable_tt();
                                }
                            }
                            
                            break;
                        }
                        else if(strcmp(str, "Threads") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                if(strcmp(str, "value") == 0)
                                {
                                    str = _strtok(NULL, delim, &strtok_ptr);
                                    sscanf(str, "%d", &threadCount);
                                    threadCount = _min(_max(threadCount, MIN_THREADS), MAX_THREADS);
                                    omp_set_num_threads(threadCount); 
                                }
                            }
                            
                            break;
                        }
                        else if(strcmp(str, "Ponder") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                if(strcmp(str, "value") == 0)
                                {
                                    str = _strtok(NULL, delim, &strtok_ptr);
                                    if(strcmp(str, "true") == 0) { enablePonder = 1; }
                                    else { enablePonder = 0; }
                                }
                            }
                            break;
                        }else if(strcmp(str, "OwnBook") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                if(strcmp(str, "value") == 0)
                                {
                                    str = _strtok(NULL, delim, &strtok_ptr);
                                    if(strcmp(str, "true") == 0) { useBook = 1; loadBook(); }
                                    else { useBook = 0; unloadBook(); }
                                }
                            }
                            break;
                        }
                        else if(strcmp(str, "Clear") == 0)
                        {
                            str = _strtok(NULL, delim, &strtok_ptr);
                            if(str && strcmp(str, "Hash") == 0)
                            {
                                clear_tt(transpositionTable);
                            }
                            break;
                        }
                        else if(strcmp(str, "SygyzyPath") == 0)
                        {
                            str = _strtok(NULL, delim, &strtok_ptr);
                            if(str) 
                            {
                                strncpy(sygyzyPath, str, 1023);
                                sygyzyPath[1023] = '\0';
                                isPathDirty = 1;
                            }
                        }
                        else if(strcmp(str, "SygyzyProbeLimit") == 0)
                        {
                            str = _strtok(NULL, delim, &strtok_ptr);
                            if(str) sscanf(str, "%d", &sygyzyProbeLimit);
                            sygyzyProbeLimit = _max(_min(sygyzyProbeLimit, MAX_PROBE_LIMIT), MIN_PROBE_LIMIT);
                            break;
                        }
                        else if(strcmp(str, "SyzygyProbeDepth") == 0)
                        {
                            str = _strtok(NULL, delim, &strtok_ptr);
                            if(str) sscanf(str, "%d", &sygyzyProbeDepth);
                            sygyzyProbeDepth = _max(_min(sygyzyProbeDepth, MAX_PROBE_DEPTH), MIN_PROBE_DEPTH);
                            break;
                        }
                        break;
                    }
                }
            }
            else if(strcmp(str, "isready") == 0)
            {
                readyUp(&isPathDirty, &useBook, &isReady, sygyzyPath, threadContext);
                printf("readyok\n");
                fflush(stdout);
            }
            else if(strcmp(str, "debug") == 0)
            {
                if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                {
                    if(strcmp(str, "on") == 0) enableDebugMessages();
                    else if(strcmp(str, "off") == 0) disableDebugMessages();
                }
                break;
            }
            else if(strcmp(str, "position") == 0)
            {
                readyUp(&isPathDirty, &useBook, &isReady, sygyzyPath, threadContext);
                if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                {
                    if(strcmp(str, "fen") == 0)
                    {
                        char FEN[128] = {'\0'};
                        short insertIndex = 0;
                        short length = 0;

                        for(int i = 0; i < 6; i++)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) == NULL) break; //Invalid FEN

                            length = strlen(str);
                            strncpy(&FEN[insertIndex], str, 127 - insertIndex);
                            FEN[127] = '\0';
                            insertIndex+=length;
                            if(i < 5) FEN[insertIndex++] = ' ';
                        }
                        if(!str) break; //Invalid FEN
                        load_fen_string_to_board(board, FEN);
                    }
                    else if(strcmp(str, "startpos") == 0)
                    {
                        load_fen_string_to_board(board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                    }

                    if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "moves") == 0)
                    {
                        while((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        {
                            board->historyIndex = 0;
                            moveFromStruct(board, getStructFromString(board, str));
                        }
                    }

                    loadInputAccumulator(board, playerAccumulator, WHITE);
                    loadInputAccumulator(board, playerAccumulator, BLACK);
                }
                break; 
            }
            else if(strcmp(str, "go") == 0)
            {
                if(isCalculating) 
                {
                    DEBUG_ERROR("Engine is already calculating.");
                    break;
                }
                isCalculating = 1;
                readyUp(&isPathDirty, &useBook, &isReady, sygyzyPath, threadContext);

                int whiteTime = INT32_MAX;
                int blackTime = INT32_MAX;
                int whiteIncrement = 0;
                int blackIncrement = 0;
                int fixedMoveTime = 0;
                int isInfinite = 0;
                threadContext->isPonder = 0;
                memset(threadContext->searchedMoves, 0, 16 * sizeof(move));

                short searchedMoveCount = 0;
                while((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                {
                    if(strcmp(str, "infinite") == 0)
                    {
                        isInfinite = 1;
                    }
                    else if(strcmp(str, "ponder") == 0)
                    {
                        //Just fill up the TT table, don't print.
                        threadContext->isPonder = 1;
                    }
                    else if(strcmp(str, "wtime") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) sscanf(str, "%d", &whiteTime);
                    }
                    else if(strcmp(str, "btime") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) sscanf(str, "%d", &blackTime);
                    }
                    else if(strcmp(str, "winc") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) sscanf(str, "%d", &whiteIncrement);
                    }
                    else if(strcmp(str, "binc") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) sscanf(str, "%d", &blackIncrement);
                    }
                    else if(strcmp(str, "depth") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) 
                        {
                            sscanf(str, "%d", &threadContext->depth);
                            threadContext->depth = _min(_max(threadContext->depth, 1), MAX_PLY);
                        }
                    }
                    else if(strcmp(str, "nodes") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) sscanf(str, "%d", &threadContext->maxNodes);
                    }
                    else if(strcmp(str, "movetime") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) sscanf(str, "%d", &fixedMoveTime);
                    }
                    else if(strcmp(str, "searchmoves") == 0)
                    {
                        //Assume this is the final command in list.
                        while((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && searchedMoveCount < 16)
                        {
                            threadContext->searchedMoves[searchedMoveCount] = getStructFromString(board, str);
                            searchedMoveCount++;
                        }
                    }
                }

                //Finished parsing command modifers, setup & launch thread.
                if(fixedMoveTime) endTime = clock() + (fixedMoveTime * CLOCKS_PER_SEC) / 1000;
                else
                {
                    if(isInfinite) endTime = LONG_MAX;
                    else
                    {
                        if(ISWHITE(board->turn)) endTime = (whiteTime / 20) + (whiteIncrement / 2);
                        else endTime = (blackTime / 20) + (blackIncrement / 2);
                    } 
                }

                THREAD_START(calculateThread, calculateBestMove, threadContext);
                break;
            }
            else if(strcmp(str, "ponderhit") == 0)
            {
                endTime = 0;
                THREAD_WAIT(calculateThread);

                threadContext->isPonder = 0;
                THREAD_START(calculateThread, calculateBestMove, threadContext);
            }
            else if(strcmp(str, "stop") == 0)
            {
                //Reap the thread, which will print out its results as it terminates.
                endTime = 0;
                isCalculating = 0;
                THREAD_WAIT(calculateThread);
            }
            else if(strcmp(str, "train") == 0)
            {
                readyUp(&isPathDirty, &useBook, &isReady, sygyzyPath, threadContext);

                //Not a part of UCI.
                //Format: "train <epoch count>"
                if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                {
                    int epochCount;
                    sscanf(str, "%d", &epochCount);
                    train(epochCount, 4e-4f);
                }
                break;
            }
            else if(strcmp(str, "perft") == 0)
            {
                readyUp(&isPathDirty, &useBook, &isReady, sygyzyPath, threadContext);

                //Not a part of UCI.
                //Format: "perft <depth>"
                if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                {
                    int pdepth;
                    sscanf(str, "%d", &pdepth);
                    clock_t startTime = clock();
                    int result = perft(board, pdepth, pdepth, 0);
                    clock_t duration = clock() - startTime;
                    double seconds = ((double) duration / CLOCKS_PER_SEC);
                    double NPS = result / seconds;
                    printf("Searched through %d nodes in %f seconds at %f NPS.\n", result, seconds, NPS);
        
                }
                break;
            }
            else if(strcmp(str, "perftv") == 0)
            {
                readyUp(&isPathDirty, &useBook, &isReady, sygyzyPath, threadContext);

                //verbose perft
                //Not a part of UCI.
                //Format: "perft <depth>"
                if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                {
                    int pdepth;
                    sscanf(str, "%d", &pdepth);
                    clock_t startTime = clock();
                    int result = perft(board, pdepth, pdepth, 1);
                    clock_t duration = clock() - startTime;
                    double seconds = ((double) duration / CLOCKS_PER_SEC);
                    double NPS = result / seconds;
                    printf("Searched through %d nodes in %f seconds at %f NPS.\n", result, seconds, NPS);
                }
                break;
            }
            else if(strcmp(str, "moveerror") == 0)
            {
                char FEN[128] = {'\0'};
                export_fen_from_board(board, FEN);
                DEBUG_ERROR("Move error received on FEN %s", FEN);
                break;
            }
            else if(strcmp(str, "print") == 0)
            {
                board_print(board, 1);
            }
            else if(strcmp(str, "quit") == 0) 
            {
                quit = 1;
                break;
            }

            str = _strtok(NULL, delim, &strtok_ptr);
        }
    }


    if(nnue_weights) free(nnue_weights);
    if(playerAccumulator) free(playerAccumulator);
    if(threadContext->score) free(threadContext->score);
    if(threadContext) free(threadContext);
    destroyRefreshTable(playingRefreshTable);
    destroy_hashTable_tt(transpositionTable);
    tb_free();
    if(board) free(board);
    unloadBook();
}

void readyUp(int *isPathDirty, int *useBook, int *isReady, char* sygyzyPath, searchThreadContext* context)
{
    if(*isReady) return;

    *isReady = 1;
    if(zobrist_piece_keys[0][0] == 0) initZobristPieceKeys();

    if(rookTable[0] == 0) initMagics();
    if(pawnAttacks[0][0] == 0) initPawnAttacks();
    if(knightAttacks[0] == 0) initKnightMoveTable();
    if(kingAttacks[0] == 0) initKingMoveTable();

    loadWeights();

    if(!transpositionTable) transpositionTable = create_hashTable_tt();
    if(!playerAccumulator) playerAccumulator = calloc(1, sizeof(accumulator));
    if(!playingRefreshTable) playingRefreshTable = createRefreshTable();
    
    context->accumulator = playerAccumulator;
    context->accumulatorTable = playingRefreshTable;

    if(*useBook) loadBook();
    else unloadBook();
    
    if(*isPathDirty)
    {
        tb_init(sygyzyPath);
        isPathDirty = 0;
    }
}