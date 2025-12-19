#include "bitboard.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

//Returns value in range [1, 8]
int getColumn(int square)
{
    if(square < 0 || square > 63)
    {
        DEBUG("Cannot get column of square %d - out of bounds [0, 63]", square)
        return 0;
    }

    return square%8 + 1;
}

//Returns value in range [1, 8]
int getRow(int square)
{
    if(square < 0 || square > 63)
    {
        DEBUG("Cannot get row of square %d - out of bounds [0, 63]", square)
        return 0;
    } 

    return floor(square/8) + 1;
}

char getColumnChar(int x, int isSquare)
{
    if(isSquare)
    {
        return columnNames[getColumn(x)-1];
    }
    else
    {
        return columnNames[x-1];
    }
    
}

void getSquareName(int square, char* target)
{
    if(square < 0 || square > 63)
    {
        DEBUG("Square %d is out of bounds [0, 63]", square)
        return;
    } 
    //Output example: e4
    char squareName[3] = {'\0'};
    squareName[0] = getColumnChar(square, 1);
    squareName[1] = ('0' + getRow(square));
    strncpy(target, squareName, 3);
}

int getSquareNumber(char* squareName)
{
    return ((squareName[0] - 97) + 8*(squareName[1] - '0' - 1));
}

//Resets the board to an opening position
void board_reset(bitboard* board)
{
    if(board == NULL) {
        DEBUG("Error resetting board. Board is NULL")
        return;
    }

    board->pawn_w = 0x000000000000FF00;
    board->pawn_b = 0x00FF000000000000;
    
    board->bishop_w = 0x0000000000000024;
    board->bishop_b = 0x2400000000000000;
    
    board->knight_w = 0x0000000000000042;
    board->knight_b = 0x4200000000000000;
    
    board->rook_w = 0x0000000000000081;
    board->rook_b = 0x8100000000000000;
    
    board->queen_w = 0x0000000000000008;
    board->queen_b = 0x0800000000000000;
    
    board->king_w = 0x0000000000000010;
    board->king_b = 0x1000000000000000;

    
    board->pieces_w = 0x000000000000FFFF;
    board->pieces_b = 0xFFFF000000000000;
    board->pieces_all = 0xFFFF00000000FFFF;

    board->kingSquare_b = 60;
    board->kingSquare_w  = 4;

    //Castling
    board->canQueensideCastle_b = 1;
    board->canKingsideCastle_b = 1;
    board->canQueensideCastle_w = 1;
    board->canKingsideCastle_w = 1;

    //Check
    board->in_check_w = 0;
    board->in_check_b = 0;

    //Turn
    board->turn = WHITE;

    //Checkmate
    board->victor = 0;

    //History
    board->moveStackTop = NULL;
    board->ht = create_hashTable();
}

//Sets all the position bits to 0 for an empty board
void board_clear(bitboard* board)
{
    if(board == NULL) {
        DEBUG("Error clearing board. Board is NULL")
        return;
    }

    memset(board, 0, sizeof(bitboard));
}

int findPieceOnSquare(bitboard* board, int square)
{
    if(board == NULL)
    {
        DEBUG("Cannot find piece on square. Board is NULL")
        return 0;
    }
    else if(square < 0 || square > 63)
    {
        DEBUG("Cannot check square %d - out of bounds [0, 63]", square)
        return 0;
    }

    int piece = 0;
    uint64_t mask = (1ull<<square);

    if((board->pieces_b&mask) == mask)
    {
        piece|=BLACK;

        if((board->pawn_b&mask) == mask)
        {
            piece|=PAWN;
        }
        else if((board->knight_b&mask) == mask)
        {
            piece|=KNIGHT;
        }
        else if((board->bishop_b&mask) == mask)
        {
            piece|=BISHOP;
        }
        else if((board->rook_b&mask) == mask)
        {
            piece|=ROOK;
        }
        else if((board->queen_b&mask) == mask)
        {
            piece|=QUEEN;
        }
        else if((board->king_b&mask) == mask)
        {
            piece|=KING;
        }
    }
    else if((board->pieces_w&mask) == mask)
    {
        piece|=WHITE;

        if((board->pawn_w&mask) == mask)
        {
            piece|=PAWN;
        }
        else if((board->knight_w&mask) == mask)
        {
            piece|=KNIGHT;
        }
        else if((board->bishop_w&mask) == mask)
        {
            piece|=BISHOP;
        }
        else if((board->rook_w&mask) == mask)
        {
            piece|=ROOK;
        }
        else if((board->queen_w&mask) == mask)
        {
            piece|=QUEEN;
        }
        else if((board->king_w&mask) == mask)
        {
            piece|=KING;
        }
    }

    return piece;
}

//Will clear all piece tpyes if Least Significant Byte's value is out of range 1-6
//Will clear all colors if 2nd LSByte's value is out of range 1-2
void board_clear_square(bitboard* board, int square, int pieceType)
{
    if(board == NULL)
    {
        DEBUG("Cannot clear square. Board is NULL")
        return;
    }
    else if(square < 0 || square > 63)
    {
        DEBUG("Cannot clear square %d - out of bounds [0, 63]", square)
        return;
    } 
    
    uint64_t applyMask = ~(1ull<<square);
    if(ISWHITE(pieceType))
    {
        if(ISPAWN(pieceType))
        {
            board->pawn_w&=applyMask;
        }
        else if(ISBISHOP(pieceType))
        {
            board->bishop_w&=applyMask;
        }
        else if(ISKNIGHT(pieceType))
        {
            board->knight_w&=applyMask;
        }
        else if(ISROOK(pieceType))
        {
            board->rook_w&=applyMask;
        }
        else if(ISQUEEN(pieceType))
        {
            board->queen_w&=applyMask;
        }
        else if(ISKING(pieceType))
        {
            board->king_w&=applyMask;
        }
        else
        {
            board->pawn_w&=applyMask;
            
            board->bishop_w&=applyMask;
            
            board->knight_w&=applyMask;
            
            board->rook_w&=applyMask;
            
            board->queen_w&=applyMask;
            
            board->king_w&=applyMask;
        }

        board->pieces_w&=applyMask;
        board->pieces_all&=applyMask;
    }
    else if(ISBLACK(pieceType))
    {
        if(ISPAWN(pieceType))
        {
            board->pawn_b&=applyMask;
        }
        else if(ISBISHOP(pieceType))
        {
            board->bishop_b&=applyMask;
        }
        else if(ISKNIGHT(pieceType))
        {
            board->knight_b&=applyMask;
        }
        else if(ISROOK(pieceType))
        {
            board->rook_b&=applyMask;
        }
        else if(ISQUEEN(pieceType))
        {
            board->queen_b&=applyMask;
        }
        else if(ISKING(pieceType))
        {
            board->king_b&=applyMask;
        }
        else
        {
            board->pawn_b&=applyMask;
            
            board->bishop_b&=applyMask;
            
            board->knight_b&=applyMask;
            
            board->rook_b&=applyMask;
            
            board->queen_b&=applyMask;
            
            board->king_b&=applyMask;
        }

        board->pieces_b&=applyMask;
        board->pieces_all&=applyMask;
    }
    else
    {
        if(ISPAWN(pieceType))
        {
            board->pawn_w&=applyMask;
            board->pawn_b&=applyMask;
        }
        else if(ISBISHOP(pieceType))
        {
            board->bishop_w&=applyMask;
            board->bishop_b&=applyMask;
        }
        else if(ISKNIGHT(pieceType))
        {
            board->knight_w&=applyMask;
            board->knight_b&=applyMask;
        }
        else if(ISROOK(pieceType))
        {
            board->rook_w&=applyMask;
            board->rook_b&=applyMask;
        }
        else if(ISQUEEN(pieceType))
        {
            board->queen_w&=applyMask;
            board->queen_b&=applyMask;
        }
        else if(ISKING(pieceType))
        {
            board->king_w&=applyMask;
            board->king_b&=applyMask;
        }
        else
        {
            board->pawn_w&=applyMask;
            board->pawn_b&=applyMask;
            
            board->bishop_w&=applyMask;
            board->bishop_b&=applyMask;
            
            board->knight_w&=applyMask;
            board->knight_b&=applyMask;
            
            board->rook_w&=applyMask;
            board->rook_b&=applyMask;
            
            board->queen_w&=applyMask;
            board->queen_b&=applyMask;
            
            board->king_w&=applyMask;
            board->king_b&=applyMask;
        }

        board->pieces_w&=applyMask;
        board->pieces_b&=applyMask;
        board->pieces_all&=applyMask;
    }
}

void board_set(bitboard* board, int square, int piece)
{
    if(board == NULL)
    {
        DEBUG("Cannot set square. Board is NULL")
        return;
    }
    else if(square < 0 || square > 63)
    {
        DEBUG("Cannot set square %d - out of bounds [0, 63]", square)
        return;
    }

    uint64_t applyMask = (1ull<<square);

    //Avoid duplicate pieces on a square.
    board_clear_square(board, square, 0);

    if (ISWHITE(piece)) 
    {
        switch(piece&0xF)
        {
            case PAWN:
                board->pawn_w|=applyMask;
                break;
            case KNIGHT:
                board->knight_w|=applyMask;
                break;
            case BISHOP:
                board->bishop_w|=applyMask;
                break;
            case ROOK:
                board->rook_w|=applyMask;
                break;
            case QUEEN:
                board->queen_w|=applyMask;
                break;
            case KING:
                board->king_w|=applyMask;
                break;
            default:
                return;
        }
        board->pieces_w|=applyMask;
        board->pieces_all|=applyMask;
    }
    else if (ISBLACK(piece))
    {
        switch(piece&0xF)
        {
            case PAWN:
                board->pawn_b|=applyMask;
                break;
            case KNIGHT:
                board->knight_b|=applyMask;
                break;
            case BISHOP:
                board->bishop_b|=applyMask;
                break;
            case ROOK:
                board->rook_b|=applyMask;
                break;
            case QUEEN:
                board->queen_b|=applyMask;
                break;
            case KING:
                board->king_b|=applyMask;
                break;
            default:
                return;
        }   
        board->pieces_b|=applyMask;
        board->pieces_all|=applyMask;
    }
}

void piece_print(char boardArray[8][9], uint64_t piece, char printChar)
{
    int square = 0;
    while(square < 64)
    {
        if((piece&1) == 1) boardArray[getRow(square) - 1][getColumn(square) - 1] = printChar;
        piece = piece>>1;
        square++;
    }
}

void board_print(bitboard* board, int printValues, int printHistory)
{
    if(board == NULL)
    {
        DEBUG("Cannot print board. Board is NULL")
        return;
    }
    
    if(printValues) values_print(board);

    //8 rows x (8 columns + null terminator)
    char boardArray[8][9] = {"--------\0", "--------\0", "--------\0", "--------\0", "--------\0", "--------\0", "--------\0", "--------\0"};

    piece_print(boardArray, board->pawn_w, 'p');
    piece_print(boardArray, board->pawn_b, 'P');

    piece_print(boardArray, board->rook_w, 'r');
    piece_print(boardArray, board->rook_b, 'R');

    piece_print(boardArray, board->knight_w, 'n');
    piece_print(boardArray, board->knight_b, 'N');

    piece_print(boardArray, board->bishop_w, 'b');
    piece_print(boardArray, board->bishop_b, 'B');

    piece_print(boardArray, board->queen_w, 'q');
    piece_print(boardArray, board->queen_b, 'Q');

    piece_print(boardArray, board->king_w, 'k');
    piece_print(boardArray, board->king_b, 'K');

    printf("\n");
    for(int row = 7; row >= 0; row--)
    {
        for(int column = 0; column <= 7; column++)
        {
            if (boardArray[row][column] < 91 && boardArray[row][column] > 64)
            {
                boardArray[row][column] = boardArray[row][column]+32;
                printf("\033[31m%c\033[0m ", boardArray[row][column]);
            }
            else
            {
                printf("%c ", boardArray[row][column]);
            }
        }
        printf("\n");
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
            printf("\t[%02x->%02x]: %s->%s", m->piece, m->promoteTo, startSquareName, endSquareName);
            if(m->capturedPiece) printf(" Captured %02x on %s", m->capturedPiece, capturedSquareName);
            printf("\n");
        }
        printf("\n");
    }
    
}


void values_print(bitboard* board)
{
    if(board == NULL)
    {
        DEBUG("Cannot print board values. Board is NULL")
        return;
    }

    printf("ALL: %016llx\n", board->pieces_all);
    printf("WHITE/BLACK: %016llx %016llx\n", board->pieces_w, board->pieces_b);
    printf("PAWN: %016llx %016llx\n", board->pawn_w, board->pawn_b);
    printf("ROOK: %016llx %016llx\n", board->rook_w, board->rook_b);
    printf("KNIGHT: %016llx %016llx\n", board->knight_w, board->knight_b);
    printf("BISHOP: %016llx %016llx\n", board->bishop_w, board->bishop_b);
    printf("QUEEN: %016llx %016llx\n", board->queen_w, board->queen_b);
    printf("KING: %016llx %016llx\n\t(%d) (%d)\n", board->king_w, board->king_b, board->kingSquare_w, board->kingSquare_b);
    printf("Can white kingside castle: %d\n", board->canKingsideCastle_w);
    printf("Can white queenside castle: %d\n", board->canQueensideCastle_w);
    printf("Can black kingside castle: %d\n", board->canKingsideCastle_b);
    printf("Can black queenside castle: %d\n", board->canQueensideCastle_b);
    printf("White in check: %d\n", board->in_check_w);
    printf("Black in check: %d\n", board->in_check_b);
}


void bitmask_print(uint64_t mask, char fill)
{
    char boardArray[8][9] = {"--------\0", "--------\0", "--------\0", "--------\0", "--------\0", "--------\0", "--------\0", "--------\0"};
    piece_print(boardArray, mask, fill);
    printf("\nMask: %016llx\n", mask);
    for(int row = 7; row >= 0; row--)
    {
        printf("\t");
        for(int column = 0; column <= 7; column++)
        {
            printf("%c ", boardArray[row][column]);
        }
        printf("\n");
    }
}


int moves_push(bitboard* board, move* m)
{
    if(!board || !m)
    {
        DEBUG("Failed to push move")
        return -1;
    }

    if(!board->moveStackTop)
    {
        board->moveStackTop = m;
    }
    else
    {
        m->nextMove = board->moveStackTop;
        board->moveStackTop = m;
    }
    return 0;
}

move* moves_pop(bitboard* board)
{
    if(!board)
    {
        DEBUG("Failed to pop move from null board")
        return NULL;
    }

    move* tempMove = board->moveStackTop;
    if(!tempMove) return NULL;
    board->moveStackTop = board->moveStackTop->nextMove;
    tempMove->nextMove = NULL;
    return tempMove;
}

move* createMove(int startSquare, int endSquare, int promoteTo, int piece, int capturedPiece, int capturedPieceSquare, int castleRights)
{
    move* m = calloc(1, sizeof(move));
    if(!m) return NULL;

    m->startSquare = startSquare;
    m->endSquare = endSquare;
    m->promoteTo = promoteTo;
    m->piece = piece;
    m->capturedPiece = capturedPiece;
    m->capturedPieceSquare = capturedPieceSquare;
    m->previousCastleRights = castleRights;
    m->nextMove = NULL;
    return m;
}