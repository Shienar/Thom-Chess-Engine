#include "board/moves.h"
#include "board/bitboard.h"
#include "debug.h"
#include "hashtables/transpositiontable.h"
#include "analyze/book.h"
#include "pyrrhic/tbprobe.h"
#include "analyze/search.h"
#include "binpack/generate.h"
#include "analyze/hce/tuner.h"
#include "analyze/nnue/neuralnet.h"
#include <omp.h>
#include <string.h>

#ifdef SPSA

#define SET_SPA_OPTION_INT(name) \
    else if(strcmp(str, #name) == 0) \
    { \
        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) \
            sscanf(str, "%d", &name); \
        break; \
    }
    
#define SET_SPA_OPTION_FLOAT(name) \
    else if(strcmp(str, #name) == 0) \
    { \
        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) \
        { \
            int temp; \
            sscanf(str, "%d", &temp); \
            name = temp / 1000.0; \
        } \
        break; \
    }

#endif

void readyUp(int *isPathDirty, int *isReady, char* SyzygyPath, searchThreadContext* context);

int main(int argc, char** argv)
{
    omp_set_num_threads(threadCount);
    srand(time(NULL));
    
    char buffer[4096] = {'\0'};
    const char* delim = " \t\r\n";
    char* str = NULL;
    char* strtok_ptr = NULL;

    int quit = 0;
    int isReady = 0;
    
    char SyzygyPath[1024] = {'\0'};
    int isPathDirty = 1; //Has syzygy been initialized with the current path?

    char debugLogPath[1024] = {'\0'};

    searchThreadContext* threadContext = calloc(1, sizeof(searchThreadContext));
    bitboard* board = &threadContext->boardStack[0];
    threadContext->maxDepth = MAX_PLY;
    threadContext->hardMaxNodes = INT32_MAX;
    threadContext->softMaxNodes = INT32_MAX;
    threadContext->abortFlag = &abortFlag;

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
                printf("id author Grant Vizaniaris\n");
                printf("option name Hash type spin default 256 min 1 max 4096\n");
                printf("option name Threads type spin default 1 min 1 max 64\n");
                printf("option name Ponder type check default false\n");
                printf("option name OwnBook type check default false\n");
                printf("option name UseNNUE type check default true\n");
                printf("option name LogFilePath type string default <empty>\n");
                printf("option name SyzygyPath type string default <empty>\n");
                printf("option name SyzygyProbeLimit type spin default 5 min 3 max 7\n"); //n-man syzygy tablebase.
                printf("option name SyzygyProbeDepth type spin default 6 min 5 max 32\n"); //Probe syzygy at non-root if at least n depth remaining in search.
                #ifdef SPSA
                printf("option name initial_aspiration_margin type spin default %d min 10 max 100\n", initial_aspiration_margin);
                printf("option name maximum_aspiration_margin type spin default %d min 50 max 200\n", maximum_aspiration_margin);
                printf("option name aspiration_margin_mult_factor type spin default %d min 1200 max 3000\n", (int) (aspiration_margin_mult_factor * 1000));

                printf("option name razoring_a type spin default %d min 25 max 250\n", razoring_a);
                printf("option name razoring_b type spin default %d min 25 max 250\n", razoring_b);

                printf("option name delta_pruning_offset type spin default %d min 10 max 200\n", delta_pruning_offset);
                printf("option name delta_pruning_nnue_offset type spin default %d min 100 max 1500\n", delta_pruning_nnue_offset);
                

                printf("option name futility_margin type spin default %d min 25 max 250\n", futility_margin);
                printf("option name futility_depth_margin type spin default %d min 25 max 250\n", futility_depth_margin);


                printf("option name reverse_futility_margin type spin default %d min 100 max 400\n", reverse_futility_margin);
                printf("option name reverse_futility_margin_improving type spin default %d min 50 max 200\n", reverse_futility_margin_improving);

                printf("option name probcut_offset type spin default %d min 200 max 600\n", probcut_offset);
                printf("option name probcut_offset_improving type spin default %d min 200 max 500\n", probcut_offset_improving);
                
                printf("option name historyBonusScale type spin default %d min 50 max 600\n", historyBonusScale);
                printf("option name historyBonusOffset type spin default %d min 50 max 600\n", historyBonusOffset);
                printf("option name historyPenaltyScale type spin default %d min 50 max 600\n", historyPenaltyScale);
                printf("option name historyPenaltyOffset type spin default %d min 50 max 600\n", historyPenaltyOffset);
                
                printf("option name lowHistoryVal type spin default %d min -500 max 0\n", lowHistoryVal);

                printf("option name lmr_a type spin default %d min 100 max 2000\n", (int) (lmr_a * 1000));
                printf("option name lmr_b type spin default %d min 1000 max 5000\n", (int) (lmr_b * 1000));
                
                printf("option name lmp_a type spin default %d min 500 max 7000\n", (int) (lmp_a * 1000));
                printf("option name lmp_b type spin default %d min 500 max 7000\n", (int) (lmp_b * 1000));
                printf("option name lmp_improving_a type spin default %d min 500 max 7000\n", (int) (lmp_improving_a * 1000));
                printf("option name lmp_improving_b type spin default %d min 500 max 7000\n", (int) (lmp_improving_b * 1000));
                
                printf("option name stable_eval_margin type spin default %d min 0 max 75\n", stable_eval_margin);
                #endif
                printf("uciok\n");
                fflush(stdout);
                break;
            }
            else if(strcmp(str, "ucinewgame") == 0)
            {
                readyUp(&isPathDirty, &isReady, SyzygyPath, threadContext);
                if(isCalculating)
                {
                    abortFlag = 1;
                    THREAD_WAIT(calculateThread);
                }
                clear_tt(transpositionTable);
                break;
            }
            else if(strcmp(str, "setoption") == 0) 
            {
                if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "name") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                {
                    if(strcmp(str, "Hash") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        {
                            uint64_t byteSize;
                            sscanf(str, "%" PRIu64 "", &byteSize);
                            tt_size_entries = (byteSize * 1024 * 1024) / sizeof(tt_entry);
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
                    else if(strcmp(str, "UseNNUE") == 0 && isNetworkLoaded)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        {
                            if(strcmp(str, "true") == 0) { useNNUE = 1; }
                            else { useNNUE = 0; }
                        }
                        readyUp(&isPathDirty, &isReady, SyzygyPath, threadContext);
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
                    else if(strcmp(str, "LogFilePath") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        {
                            if(strcmp(str, "<empty>") == 0)
                                debugLogPath[0] = '\0';
                            else
                            {
                                strncpy(debugLogPath, str, 1023);
                                debugLogPath[1023] = '\0';
                            }
                            if(printDebugMessages)
                            {
                                disableDebugMessages();
                                enableDebugMessages();
                            }
                        }
                        break;
                    }
                    else if(strcmp(str, "SyzygyPath") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        {
                            if(strcmp(str, "<empty>") == 0)
                                debugLogPath[0] = '\0';
                            else
                            {
                                strncpy(SyzygyPath, str, 1023);
                                SyzygyPath[1023] = '\0';
                            }
                            isPathDirty = 1;
                        }
                        break;
                    }
                    else if(strcmp(str, "SyzygyProbeLimit") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        {
                            sscanf(str, "%d", &syzygyProbeLimit);
                            syzygyProbeLimit = clamp(syzygyProbeLimit, MIN_PROBE_LIMIT, MAX_PROBE_LIMIT);
                        }
                        break;
                    }
                    else if(strcmp(str, "SyzygyProbeDepth") == 0)
                    {
                        if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        {
                            if(str) sscanf(str, "%d", &syzygyProbeDepth);
                            syzygyProbeDepth = clamp(syzygyProbeDepth, MIN_PROBE_DEPTH, MAX_PROBE_DEPTH);
                        }
                        break;
                    }
                    #ifdef SPSA
                    SET_SPA_OPTION_INT(initial_aspiration_margin)
                    SET_SPA_OPTION_INT(maximum_aspiration_margin)
                    SET_SPA_OPTION_FLOAT(aspiration_margin_mult_factor)
                    SET_SPA_OPTION_INT(razoring_a)
                    SET_SPA_OPTION_INT(razoring_b)
                    SET_SPA_OPTION_INT(reverse_futility_margin)
                    SET_SPA_OPTION_INT(reverse_futility_margin_improving)
                    SET_SPA_OPTION_INT(futility_margin)
                    SET_SPA_OPTION_INT(futility_depth_margin)
                    SET_SPA_OPTION_INT(probcut_offset)
                    SET_SPA_OPTION_INT(probcut_offset_improving)
                    SET_SPA_OPTION_INT(historyBonusScale)
                    SET_SPA_OPTION_INT(historyBonusOffset)
                    SET_SPA_OPTION_INT(historyPenaltyScale)
                    SET_SPA_OPTION_INT(historyPenaltyOffset)
                    SET_SPA_OPTION_INT(lowHistoryVal)
                    SET_SPA_OPTION_INT(stable_eval_margin)
                    SET_SPA_OPTION_INT(delta_pruning_offset)
                    SET_SPA_OPTION_INT(delta_pruning_nnue_offset)
                    SET_SPA_OPTION_FLOAT(lmr_a)
                    SET_SPA_OPTION_FLOAT(lmr_b)
                    SET_SPA_OPTION_FLOAT(lmp_a)
                    SET_SPA_OPTION_FLOAT(lmp_b)
                    SET_SPA_OPTION_FLOAT(lmp_improving_a)
                    SET_SPA_OPTION_FLOAT(lmp_improving_b)
                    #endif
                    break;
                }
            }
            else if(strcmp(str, "isready") == 0)
            {
                readyUp(&isPathDirty, &isReady, SyzygyPath, threadContext);
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
                readyUp(&isPathDirty, &isReady, SyzygyPath, threadContext);
                if(isCalculating)
                {
                    abortFlag = 1;
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
                        load_fen_string_to_board(board, FEN, &threadContext->repetitions);
                    }
                    else if(strcmp(str, "startpos") == 0)
                        load_fen_string_to_board(board, STARTPOS_FEN, &threadContext->repetitions);

                    if((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "moves") == 0)
                    {
                        move m;
                        while((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        {
                            m = getStructFromString(board, str);
                            moveFromStruct(board, board, m, &threadContext->repetitions);
                        }
                    }
                }
                break; 
            }
            else if(strcmp(str, "go") == 0)
            {
                if(isCalculating) 
                {
                    DEBUG_INFO("Aborting search in progress.");
                    abortFlag = 1;
                    THREAD_WAIT(calculateThread);
                }
                isCalculating = 1;
                readyUp(&isPathDirty, &isReady, SyzygyPath, threadContext);

                int timeLeft = INT32_MAX;
                int increment = 0;
                int fixedMoveTime = 0;
                int isInfinite = 0;
                isPonder = 0;
                threadContext->hardMaxNodes = INT32_MAX;
                threadContext->softMaxNodes = INT32_MAX;
                memset(threadContext->searchedMoves, 0, MAX_REQUIRED_MOVES * sizeof(move));

                short searchedMoveCount = 0;
                while((str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                {
                    if(strcmp(str, "infinite") == 0)
                        isInfinite = 1;
                    else if(strcmp(str, "ponder") == 0)
                        isPonder = 1;
                    else if(strcmp(str, "wtime") == 0 && ISWHITE(board->turn) && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        sscanf(str, "%d", &timeLeft);
                    else if(strcmp(str, "btime") == 0 && ISBLACK(board->turn) && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        sscanf(str, "%d", &timeLeft);
                    else if(strcmp(str, "winc") == 0 && ISWHITE(board->turn) && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        sscanf(str, "%d", &increment);
                    else if(strcmp(str, "binc") == 0 && ISBLACK(board->turn) && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        sscanf(str, "%d", &increment);
                    else if(strcmp(str, "depth") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        sscanf(str, "%d", &threadContext->maxDepth);
                    else if(strcmp(str, "nodes") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        sscanf(str, "%d", &threadContext->hardMaxNodes);
                    else if(strcmp(str, "movetime") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
                        sscanf(str, "%d", &fixedMoveTime);
                    else if(strcmp(str, "searchmoves") == 0) //Assume this is the final command in list.
                        while((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && searchedMoveCount < MAX_REQUIRED_MOVES) 
                            threadContext->searchedMoves[searchedMoveCount++] = getStructFromString(board, str);
                }

                threadContext->maxDepth = clamp(threadContext->maxDepth, 1, MAX_PLY);

                //Finished parsing command modifers, setup & launch thread.
                threadContext->startTime = clock();
                if(fixedMoveTime)
                    threadContext->softEndTime = threadContext->hardEndTime = threadContext->startTime + (fixedMoveTime * CLOCKS_PER_SEC) / 1000;
                else if(isInfinite)
                {
                    threadContext->softEndTime = threadContext->hardEndTime = INT_MAX;
                    threadContext->maxDepth = MAX_PLY;
                }
                else
                {
                    int uciOverhead = _min(25, timeLeft / 2);
                    clock_t softEndTime = timeLeft / 20 + increment / 2 - uciOverhead;
                    clock_t hardEndTime = timeLeft / 20 + increment / 2 - uciOverhead;
                    threadContext->softEndTime = (threadContext->startTime + (softEndTime * CLOCKS_PER_SEC) / 1000);
                    threadContext->hardEndTime = (threadContext->startTime + (hardEndTime * CLOCKS_PER_SEC) / 1000);
                }

                THREAD_START(calculateThread, calculateBestMove, threadContext);
                break;
            }
            else if(strcmp(str, "ponderhit") == 0)
            {
                if(isCalculating)
                {
                    isPonder = 0;
                    THREAD_WAIT(calculateThread);
                }
                else
                {
                    isPonder = 0;
                    THREAD_START(calculateThread, calculateBestMove, threadContext);
                }
            }
            else if(strcmp(str, "stop") == 0)
            {
                //Reap the thread, which will print out its results as it terminates.
                if(isCalculating)
                {
                    isPonder = 0;
                    abortFlag = 1;
                    isCalculating = 0;
                    THREAD_WAIT(calculateThread);
                }
                
            }
            else if(strcmp(str, "perft") == 0)
            {
                readyUp(&isPathDirty, &isReady, SyzygyPath, threadContext);

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
                readyUp(&isPathDirty, &isReady, SyzygyPath, threadContext);

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
                readyUp(&isPathDirty, &isReady, SyzygyPath, threadContext);
                if(useNNUE)
                    updateAccumulatorFromTable(board, &threadContext->accumulatorStack[0], threadContext->refreshTable);
                printf("%d\n", useNNUE ? forwardPropagate(board, &threadContext->accumulatorStack[0]) : hce_eval(board));
                break;
            }
            else if(strcmp(str, "tune") == 0)
            {
                //Format: 'tune <double forcedK (0 for auto)> <uint64_t epochs> <double max_lr> <double min_lr> "<inputPath>" "<outputPath>"'

                readyUp(&isPathDirty, &isReady, SyzygyPath, threadContext);
                
                char inputPath[256] = {'\0'};
                char outputPath[256] = {'\0'};
                double forcedK;
                uint64_t epochs;
                double max_lr;
                double min_lr;
                
                if((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
                    break;
                
                sscanf(str, "%lf", &forcedK);

                if((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
                    break;

                sscanf(str, "%" PRId64" ", &epochs);

                if((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
                    break;
                    
                sscanf(str, "%lf", &max_lr);

                if((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
                    break;
                    
                sscanf(str, "%lf", &min_lr);

                if((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
                    break;

                sscanf(str, "\"%[^\"]", inputPath);

                if((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
                    break;
                
                sscanf(str, "\"%[^\"]", outputPath);
                Tune(inputPath, outputPath, forcedK, epochs, max_lr, min_lr);
                

                break;
            }
            else if(strcmp(str, "generate") == 0)
            {
                //Format: 'generate "<outputFilePath>"'
                readyUp(&isPathDirty, &isReady, SyzygyPath, threadContext);
                
                if((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
                    break;

                generate(str);
                break;
            }
            else if(strcmp(str, "binpackinfo") == 0)
            {
                //Format: 'binpackinfo "<binpackFilePath>"
                readyUp(&isPathDirty, &isReady, SyzygyPath, threadContext);
                
                if((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
                    break;

                binpackPrintInfo(str);
                break;
            }
            else if(strcmp(str, "quit") == 0) 
            {
                quit = 1;
                break;
            }

            str = _strtok(NULL, delim, &strtok_ptr);
        }
    }

    if(threadContext->accumulatorStack)
        free(threadContext->accumulatorStack);
    if(threadContext->refreshTable)
        free(threadContext->refreshTable);
    if(threadContext) 
        free(threadContext);
    destroy_hashTable_tt(transpositionTable);
    tb_free();
    disableDebugMessages();  //closes file if open.
}

void readyUp(int *isPathDirty, int *isReady, char* SyzygyPath, searchThreadContext* context)
{
    if(*isReady) return;

    *isReady = 1;
    if(zobrist_piece_keys[0][0] == 0) initZobristPieceKeys();
    
    initSearchTables();

    if(rookTable[0] == 0) initMagics();
    if(pawnAttacks[0][0] == 0) initPawnAttacks();
    if(knightAttacks[0] == 0) initKnightMoveTable();
    if(kingAttacks[0] == 0) initKingMoveTable();

    init_HCE_tables();

    if(!transpositionTable) 
    {
        transpositionTable = create_hashTable_tt();
        context->tt = transpositionTable;
    }

    loadBook();

    initNNUE();
    if(useNNUE)
    {
        if(!context->accumulatorStack)
            context->accumulatorStack = calloc(MAX_PLY + 1, sizeof(accumulator));
        if(!context->refreshTable)
            context->refreshTable = calloc(1, sizeof(accumulatorRefreshTable));
    }
    else
    {
        if(context->accumulatorStack)
        {
            free(context->accumulatorStack);
            context->accumulatorStack = NULL;
        }
        if(context->refreshTable)
        {
            free(context->refreshTable);
            context->refreshTable = NULL;
        }
    }
    
    if(*isPathDirty)
    {
        tb_init(SyzygyPath);
        isPathDirty = 0;
    }
    
    load_fen_string_to_board(&context->boardStack[0], STARTPOS_FEN, &context->repetitions);
}