#include "board/moves.h"
#include "board/bitboard.h"
#include "debug.h"
#include "hashtables/transpositiontable.h"
#include "analyze/book.h"
#include "analyze/neuralnet.h"
#include "pyrrhic/tbprobe.h"
#include "analyze/engine.h"
#include "train/train.h"
#include "binpack/generate.h"
#include <string.h>
#include <omp.h>

void readyUp(int *isPathDirty, int *isReady, char* sygyzyPath, searchThreadContext* context, bitboard** board);

int main(int argc, char** argv)
{
    omp_set_num_threads(threadCount); 
    srand(time(NULL));
    
    bitboard* board = NULL;
    char buffer[4096] = {'\0'};
    const char* delim = " \t\r\n";
    char* str = NULL;
    char* strtok_ptr = NULL;
    
    int quit = 0;
    int isReady = 0;
    
    char sygyzyPath[1024] = PROJECT_CWD "/sygyzy/";
    int isPathDirty = 1; //Has sygyzy been initialized with the current path?

    clock_t endTime; //Doubles as a terminate flag

    searchThreadContext* threadContext = calloc(1, sizeof(searchThreadContext));
    threadContext->board = board;
    threadContext->maxDepth = MAX_PLY;
    threadContext->maxNodes = INT_MAX;
    threadContext->endTime = &endTime;

    THREADTYPE calculateThread = THREAD_INIT;

    while(!quit)
    {
        memset(buffer, 0, 4096 * sizeof(char));
        while(fgets(buffer, 4096, stdin) == NULL);
        str = _strtok(buffer, delim, &strtok_ptr);
        while(str != NULL)
        {
            if(strcmp(str, "uci") == 0) 
            {
                printf("id name ChessBot 0.1\n");
                printf("id author Grant\n");
                printf("option name Hash type spin default 4 min 1 max 4096\n");
                printf("option name Threads type spin default 1 min 1 max 64\n");
                printf("option name Ponder type check default false\n");
                printf("option name OwnBook type check default false\n");
                printf("option name SygyzyPath type string default " PROJECT_CWD "/sygyzy/\n");
                printf("option name SygyzyProbeLimit type spin default 5 min 3 max 7\n"); //n-man sygyzy tablebase.
                printf("option name SyzygyProbeDepth type spin default 6 min 5 max 32\n"); //Probe sygyzy at non-root if at least n depth remaining in search.
                //Bounds & defaults handled by SPSA program weather-factory
                #ifdef SPSA
                printf("option name initial_aspiration_margin type spin default %d min 10 max 100\n", initial_aspiration_margin);
                printf("option name maximum_aspiration_margin type spin default %d min 50 max 200\n", maximum_aspiration_margin);
                printf("option name aspiration_margin_mult_factor type spin default %d min 1200 max 3000\n", (int) (aspiration_margin_mult_factor * 1000));

                printf("option name reverse_futility_margin type spin default %d min 100 max 400\n", reverse_futility_margin);
                printf("option name reverse_futility_margin_improving type spin default %d min 50 max 200\n", reverse_futility_margin_improving);
                printf("option name futility_margin type spin default %d min 150 max 450\n", futility_margin);
                printf("option name delta_pruning_offset type spin default %d min 200 max 600\n", delta_pruning_offset);

                printf("option name probcut_offset type spin default %d min 200 max 600\n", probcut_offset);
                printf("option name probcut_offset_improving type spin default %d min 200 max 500\n", probcut_offset_improving);
                #endif
                printf("uciok\n");
                fflush(stdout);
                break;
            }
            else if(strcmp(str, "ucinewgame") == 0)
            {
                readyUp(&isPathDirty, &isReady, sygyzyPath, threadContext, &board);
                if(isCalculating)
                {
                    endTime = 0;
                    THREAD_WAIT(calculateThread);
                }
                clear_tt(transpositionTable);
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
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                uint64_t byteSize;
                                sscanf(str, "%" PRIu64 "", &byteSize);
                                tt_size_entries = (byteSize * 1024 * 1024) / sizeof(table_entry_tt);
                                destroy_hashTable_tt(transpositionTable);
                                transpositionTable = create_hashTable_tt();
                                threadContext->tt = transpositionTable;
                            }
                            break;
                        }
                        else if(strcmp(str, "Threads") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                if(isCalculating)
                                {
                                    DEBUG_ERROR("Cannot change thread count while calculating.");
                                    break;
                                }
                                sscanf(str, "%d", &threadCount);
                                threadCount = clamp(threadCount, MIN_THREADS, MAX_THREADS);
                                omp_set_num_threads(threadCount); 
                            }
                            
                            break;
                        }
                        else if(strcmp(str, "Ponder") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                if(strcmp(str, "true") == 0) { enablePonder = 1; }
                                else { enablePonder = 0; }
                            }
                            break;
                        }
                        else if(strcmp(str, "OwnBook") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                if(strcmp(str, "true") == 0) { useBook = 1; }
                                else { useBook = 0; }
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
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                strncpy(sygyzyPath, str, 1023);
                                sygyzyPath[1023] = '\0';
                                isPathDirty = 1;
                            }
                            break;
                        }
                        else if(strcmp(str, "SygyzyProbeLimit") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                sscanf(str, "%d", &sygyzyProbeLimit);
                                sygyzyProbeLimit = clamp(sygyzyProbeLimit, MIN_PROBE_LIMIT, MAX_PROBE_LIMIT);
                            }
                            break;
                        }
                        else if(strcmp(str, "SyzygyProbeDepth") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                if(str) sscanf(str, "%d", &sygyzyProbeDepth);
                                sygyzyProbeDepth = clamp(sygyzyProbeDepth, MIN_PROBE_DEPTH, MAX_PROBE_DEPTH);
                            }
                            break;
                        }
                        #ifdef SPSA
                        else if(strcmp(str, "initial_aspiration_margin") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                                sscanf(str, "%d", &initial_aspiration_margin);
                            break;
                        }
                        else if(strcmp(str, "maximum_aspiration_margin") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                                sscanf(str, "%d", &maximum_aspiration_margin);
                        }
                        else if(strcmp(str, "aspiration_margin_mult_factor") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                            {
                                int temp;
                                sscanf(str, "%d", &temp);
                                aspiration_margin_mult_factor = temp / 1000.0;
                            }
                            break;
                        }
                        else if(strcmp(str, "reverse_futility_margin") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                                sscanf(str, "%d", &reverse_futility_margin);
                            break;
                        }
                        else if(strcmp(str, "reverse_futility_margin_improving") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                                sscanf(str, "%d", &reverse_futility_margin_improving);
                            break;
                        }
                        else if(strcmp(str, "futility_margin") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                                sscanf(str, "%d", &futility_margin);
                            break;
                        }
                        else if(strcmp(str, "probcut_offset") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                                sscanf(str, "%d", &probcut_offset);
                            break;
                        }
                        else if(strcmp(str, "probcut_offset_improving") == 0)
                        {
                            if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                                sscanf(str, "%d", &probcut_offset_improving);
                            break;
                        }
                        #endif
                        break;
                    }
                }
            }
            else if(strcmp(str, "isready") == 0)
            {
                readyUp(&isPathDirty, &isReady, sygyzyPath, threadContext, &board);
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
                readyUp(&isPathDirty, &isReady, sygyzyPath, threadContext, &board);
                if(isCalculating)
                {
                    endTime = 0;
                    THREAD_WAIT(calculateThread);
                }

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
                        load_fen_string_to_board(board, STARTPOS_FEN);
                    }

                    if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "moves") == 0)
                    {
                        move_c m;
                        while((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        {
                            board->historyIndex = 0;
                            m = getStructFromString(board, str);
                            moveFromStruct(board, m);
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
                    DEBUG_INFO("Aborting search in progress.");
                    endTime = 0;
                    THREAD_WAIT(calculateThread);
                }
                isCalculating = 1;
                readyUp(&isPathDirty, &isReady, sygyzyPath, threadContext, &board);

                int whiteTime = INT32_MAX;
                int blackTime = INT32_MAX;
                int whiteIncrement = 0;
                int blackIncrement = 0;
                int fixedMoveTime = 0;
                int isInfinite = 0;
                threadContext->isPonder = 0;
                threadContext->maxNodes = INT32_MAX;
                memset(threadContext->searchedMoves, 0, MAX_REQUIRED_MOVES * sizeof(move_c));

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
                            sscanf(str, "%d", &threadContext->maxDepth);
                            threadContext->maxDepth = clamp(threadContext->maxDepth, 1, MAX_PLY);
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
                        while((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && searchedMoveCount < MAX_REQUIRED_MOVES)
                        {
                            threadContext->searchedMoves[searchedMoveCount] = getStructFromString(board, str);
                            searchedMoveCount++;
                        }
                    }
                }

                //Finished parsing command modifers, setup & launch thread.
                if(fixedMoveTime) endTime = clock() + (fixedMoveTime * CLOCKS_PER_SEC) / 1000; 
                else if(isInfinite)
                {
                    endTime = LONG_MAX;
                    threadContext->maxDepth = MAX_PLY;
                }
                else
                {
                    if(ISWHITE(board->turn)) endTime = (whiteTime / 20) + (whiteIncrement / 2);
                    else endTime = (blackTime / 20) + (blackIncrement / 2);
                    endTime = (clock() + (endTime * CLOCKS_PER_SEC) / 1000)  - 50; //Subtracting UCI overhead from move time.
                }

                THREAD_START(calculateThread, calculateBestMove, threadContext);

                break;
            }
            else if(strcmp(str, "ponderhit") == 0)
            {
                if(isCalculating)
                {
                    threadContext->isPonder = 0;
                    THREAD_WAIT(calculateThread);
                }
                else
                {
                    threadContext->isPonder = 0;
                    THREAD_START(calculateThread, calculateBestMove, threadContext);
                }
            }
            else if(strcmp(str, "stop") == 0)
            {
                //Reap the thread, which will print out its results as it terminates.
                if(isCalculating)
                {
                    threadContext->isPonder = 0;
                    endTime = 0;
                    isCalculating = 0;
                    THREAD_WAIT(calculateThread);
                }
                
            }
            else if(strcmp(str, "perft") == 0)
            {
                readyUp(&isPathDirty, &isReady, sygyzyPath, threadContext, &board);

                //Not a part of UCI.
                //Format: "perft <depth>"
                if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                {
                    int pdepth;
                    sscanf(str, "%d", &pdepth);
                    clock_t startTime = clock();
                    int result = perft(board, pdepth, 0);
                    clock_t duration = clock() - startTime;
                    double seconds = ((double) duration / CLOCKS_PER_SEC);
                    double NPS = result / seconds;
                    printf("Searched through %d nodes in %f seconds at %f NPS.\n", result, seconds, NPS);
        
                }
                break;
            }
            else if(strcmp(str, "perftv") == 0)
            {
                readyUp(&isPathDirty, &isReady, sygyzyPath, threadContext, &board);

                //verbose perft
                //Not a part of UCI.
                //Format: "perft <depth>"
                if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                {
                    int pdepth;
                    sscanf(str, "%d", &pdepth);
                    clock_t startTime = clock();
                    int result = perft(board, pdepth, 1);
                    clock_t duration = clock() - startTime;
                    double seconds = ((double) duration / CLOCKS_PER_SEC);
                    double NPS = result / seconds;
                    printf("Searched through %d nodes in %f seconds at %f NPS.\n", result, seconds, NPS);
                }
                break;
            }
            else if(strcmp(str, "print") == 0)
            {
                board_print(board, 1);
                break;
            }
            else if(strcmp(str, "eval") == 0)
            {
                loadInputAccumulator(board, playerAccumulator, WHITE);
                loadInputAccumulator(board, playerAccumulator, BLACK);
                int eval = forwardPropagate(board, playerAccumulator);
                printf("%d\n", eval);
                break;
            }
            else if(strcmp(str, "netinfo") == 0)
            {
                readyUp(&isPathDirty, &isReady, sygyzyPath, threadContext, &board);
                print_network_statistics();
                break;
            }
            #ifdef TRAIN
            else if(strcmp(str, "binpackinfo") == 0)
            {
                readyUp(&isPathDirty, &isReady, sygyzyPath, threadContext, &board);
                binpackPrintInfo(VALIDATION_DATA_PATH);
                binpackPrintInfo(TRAINING_DATA_PATH);
                break;
            }
            else if(strcmp(str, "train") == 0)
            {
                readyUp(&isPathDirty, &isReady, sygyzyPath, threadContext, &board);
                
                //Not a part of UCI.
                //Format: "train <epoch count>"
                if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                {
                    int epochCount;
                    sscanf(str, "%d", &epochCount);
                    train(epochCount, 5e-4f);
                }
                break;
            }
            else if(strcmp(str, "generate") == 0)
            {
                readyUp(&isPathDirty, &isReady, sygyzyPath, threadContext, &board);
                generate();
                break;
            }
            #endif
            else if(strcmp(str, "quit") == 0) 
            {
                quit = 1;
                break;
            }

            str = _strtok(NULL, delim, &strtok_ptr);
        }
    }

    if(raw_weights) free(raw_weights);
    if(int_weights) free(int_weights);
    if(playerAccumulator) free(playerAccumulator);
    if(threadContext) free(threadContext);
    destroy_hashTable_tt(transpositionTable);
    tb_free();
    if(board) free(board);
    unloadBook(BOOK_PATH);
    disableDebugMessages();  //closes file if open.
}

void readyUp(int *isPathDirty, int *isReady, char* sygyzyPath, searchThreadContext* context, bitboard** board)
{
    if(*isReady) return;

    *isReady = 1;
    if(zobrist_piece_keys[0][0] == 0) initZobristPieceKeys();
    
    initSearchTables();

    if(rookTable[0] == 0) initMagics();
    if(pawnAttacks[0][0] == 0) initPawnAttacks();
    if(knightAttacks[0] == 0) initKnightMoveTable();
    if(kingAttacks[0] == 0) initKingMoveTable();

    loadQuantizedWeights();
    #ifdef TRAIN
    loadRawWeights();
    #endif

    if(!transpositionTable) 
    {
        transpositionTable = create_hashTable_tt();
        context->tt = transpositionTable;
    }
    if(!playerAccumulator) playerAccumulator = calloc(1, sizeof(accumulator));
    if(!playingRefreshTable) playingRefreshTable = createRefreshTable();
    
    context->accumulator = playerAccumulator;
    context->refreshTable = playingRefreshTable;

    loadBook(BOOK_PATH);
    
    if(*isPathDirty)
    {
        tb_init(sygyzyPath);
        isPathDirty = 0;
    }

    if(!(*board)) *board = create_board(NULL);
    context->board = *board;
}