#include "../include/moves.h"
#include "../include/debug.h"
#include "../include/evolve.h"
#include "../include/bitboard.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

/**
 * TODO:
 *  - Store principal variation within the engine.
 *      - Triangular PV-Table
 *  - Transposition table values need to get updated when searched at a higher depth.
 *  - Rewrite engine.c to use iterative deepening
 *      - Check the best move from last first.
 *      - Store the principle variation and check it first. Then, check the move list.
 */
int main(int argc, char** argv)
{
    /**
     * Arguments
     */
    int verbose = 0;
    int player_color = WHITE;
    int depth = 6;
    int onlyEngines = 0;
    int onlyHumans = 0;
    int threadCount = 4;
    double maxTime = 0;
    int printHistory = 0;
    int shouldTrain = 0;
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--help") == 0)
        {
            printf("\n\n");
            printf("--help\t\tPrints out this message\n");
            printf("-v\t\tVerbose board information\n");
            printf("--black\t\tPlay as black\n");
            printf("--depth\t\tChange the depth, takes one integer parameter\n");
            printf("--debug\t\tEnable debug messages\n");
            printf("--leaks\t\tTrack memory leaks. Heavily reduces performance\n");
            printf("--human\t\tHuman v human game\n");
            printf("--history\tPrint's recent moves\n");
            printf("--engine\t\tEngine v Engine game\n");
            printf("--threads\tChange the number of threads for min/max search, takes one integer parameter, max 8.\n");
            printf("--maxTime\t\tSpecify maximum computation time per turn in seconds.\n");
            printf("--train\t\tTrains the chess engine with hill climbing\n");
            printf("\n\n");
            exit(0);
        }
        else if(strcmp(argv[i], "-v") == 0)
        {
            verbose = 1;
        }
        else if(strcmp(argv[i], "--black") == 0)
        {
            player_color = BLACK;
        }
        else if(strcmp(argv[i], "--depth") == 0)
        {
            i++;
            depth = atoi(argv[i]);
        }
        else if(strcmp(argv[i], "--debug") == 0)
        {
            enableDebugMessages();
        }
        else if(strcmp(argv[i], "--leaks") == 0)
        {
            enableLeakTracking();
        }
        else if(strcmp(argv[i], "--history") == 0)
        {
            printHistory = 1;
        }
        else if(strcmp(argv[i], "--human") == 0)
        {
            onlyHumans = 1;
            onlyEngines = 0;
        }
        else if(strcmp(argv[i], "--engine") == 0)
        {
            onlyHumans = 0;
            onlyEngines = 1;
        }
        else if(strcmp(argv[i], "--threads") == 0)
        {
            i++;
            threadCount = atoi(argv[i]);
            threadCount = min(threadCount, 8);
        }
        else if(strcmp(argv[i], "--maxTime") == 0)
        {
            i++;
            maxTime = atof(argv[i]);
        }
        else if(strcmp(argv[i], "--train") == 0)
        {
            shouldTrain = 1;
        }
    }

    if(shouldTrain)
    {
        exit(0);
    }

    bitboard* board = create_board();

    engine engine = {0};
    initEnginePieceWeights(&engine, 0);

    char buffer[6] = {'\0'};
    int error = 0;

    board_print(board, 0, printHistory);

    while(1)
    {
        if(onlyHumans || (board->turn == player_color && !onlyEngines))
        {
            fgets(buffer, 6, stdin);
            if(buffer[0] == 'q')
            {
                printf("Exiting program...");
                exit(1);
            }
            else if(buffer[0] == 'u')
            {
                move* tempMove = unmove(board);
                if(tempMove) FREE(tempMove);
                board_print(board, verbose, printHistory);
            }
            else
            {
                error = moveFromString(board, buffer);
                if(!error) board_print(board, verbose, printHistory);
            }
        }
        else
        {   
            do
            {
                move* bestMove = calculateBestMove(board, &engine, depth, maxTime);
                error = moveFromStruct(board, bestMove);
            }while(error != 0);
            board_print(board, verbose, printHistory);
        }
        
        if(board->victor == WHITE)
        {
            printf("White wins!\n\n");
            break;
        }
        else if(board->victor == BLACK)
        {
            printf("Black wins!\n\n");
            break;
        }
        else if (board->victor == (WHITE|BLACK))
        {
            printf("Draw!\n\n");
            break;
        }
    }
    
    if(printHistory)
    {
        char startSquareName[3] = {'\0'};
        char endSquareName[3] = {'\0'};
        char capturedSquareName[3] = {'\0'};
        printf("Recent Moves:\n");
        for(move* m = board->moveStackTop; m != NULL; m= m->nextMove)
        {
            getSquareName(m->startSquare, startSquareName);
            getSquareName(m->endSquare, endSquareName);
            getSquareName(m->capturedPieceSquare, capturedSquareName);
            printf("\t[%02x->%02x]: %s->%s, Captured %d on %s\n", m->piece, m->promoteTo, startSquareName, endSquareName, m->capturedPiece, capturedSquareName);
        }
        printf("\n");
    }

    destroy_board(board);

    dump_allocations();
}