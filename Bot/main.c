#include "moves.h"
#include "bitboard.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include <windows.h>

/**
 * TODO: Check for memory leaks
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
    double delay = 0;
    int printHistory = 0;
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
            printf("--human\t\tHuman v human game\n");
            printf("--history\tPrint's recent moves\n");
            printf("--engine\t\tEngine v Engine game\n");
            printf("--threads\tChange the number of threads for min/max search, takes one integer parameter, max 8.\n");
            printf("--delay\t\tSpecify the number of milliseconds the computer will wait after each move.\n");
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
        else if(strcmp(argv[i], "--delay") == 0)
        {
            i++;
            delay = atof(argv[i]);
        }
    }

    bitboard* board = create_board();

    weights boardWeights = {0};
    initPieceWeights(&boardWeights, 0);

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
                if(tempMove) free(tempMove);
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
                move* bestMove = calculateBestMove(board, &boardWeights, depth, threadCount);
                error = moveFromStruct(board, bestMove);
            }while(error != 0);
            board_print(board, verbose, printHistory);
            if(delay) Sleep(delay);
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
}