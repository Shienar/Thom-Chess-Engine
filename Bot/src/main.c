#include "../include/moves.h"
#include "../include/debug.h"
#include "../include/evolve.h"
#include "../include/bitboard.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

/**
 * TODO List:
 *  - Draws are getting declared early.
 *  - A black bishop on a6 got suggested a move to capture a black pawn on b5.
 *  - Store more information in the transposition tables.
 *  - Move sorting
 *      - PV move is currently prioritized.
 *      - Ordering for other moves.
 *  - Quiescent search optimizations.
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
    double maxTime = 3;
    int printHistory = 0;
    int shouldTrain = 0;
    int maxTrainingIterations = 10;
    int tweaksPerIteration = 3;
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
            printf("--time\t\tSpecify maximum computation time per turn in seconds.\n");
            printf("--train\t\tTrains the chess engine with hill climbing. The next two variables should be maxIterations ands tweaksPerIteration\n");
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
        else if(strcmp(argv[i], "--time") == 0)
        {
            i++;
            maxTime = atof(argv[i]);
        }
        else if(strcmp(argv[i], "--train") == 0)
        {
            shouldTrain = 1;
            maxTrainingIterations = atoi(argv[++i]);;
            tweaksPerIteration = atoi(argv[++i]);;
        }
    }
    
    srand(time(NULL));

    if(shouldTrain)
    {
        printf("Starting Training.\n");
        engine* trainableEngine = CALLOC(1, sizeof(engine));
        initEnginePieceWeights(trainableEngine, 1);

        hill_climb(trainableEngine, maxTrainingIterations, tweaksPerIteration, depth, maxTime);

        printf("Training Complete.\n");
        dump_allocations();
        exit(0);
    }

    bitboard* board = create_board();

    engine opponentEngine = {0};
    initEnginePieceWeights(&opponentEngine, 1);

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
                move* bestMove = calculateBestMove(board, &opponentEngine, depth, maxTime);
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
    
    /*
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
            printf("\t[%02x", m->piece);
            if(m->promoteTo) printf("->%02x", m->promoteTo);
            printf("]: %s->%s", startSquareName, endSquareName);
            if(m->capturedPiece) printf(", Captured %d on %s\n", m->capturedPiece, capturedSquareName);
            else printf("\n");
        }
        printf("\n");
    }
    */

    destroy_board(board);

    dump_allocations();
}