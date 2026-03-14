#include "../include/structs.h"
#include "../include/debug.h"
#include "../include/bitboard.h"
#include "../include/moves.h"
#include <string.h>
#include <math.h>

//Returns value in range [1, 8]
int getColumn(int square)
{
    if(square < 0 || square > 63)
    {
        DEBUG("Cannot get column of square %d - out of bounds [0, 63]", square);
        return 0;
    }

    return square%8 + 1;
}

//Returns value in range [1, 8]
int getRow(int square)
{
    if(square < 0 || square > 63)
    {
        DEBUG("Cannot get row of square %d - out of bounds [0, 63]", square);
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
        DEBUG("Square %d is out of bounds [0, 63]", square);
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

bitboard* create_board_from_fen(const char* fileName, int lineNumber)
{
    bitboard* board = CALLOC(1, sizeof(bitboard));
    load_fen_to_board(board, fileName, lineNumber);
    return board;
}

void load_fen_to_board(bitboard* board, const char* fileName, int lineNumber)
{
    if(!board)
    {
        DEBUG("Passed NULL board");
        return;
    }

    FILE* inputFile = fopen(fileName, "r");
    if(!inputFile)
    {
        DEBUG("Failed to open file %s", fileName);
        board->victor = -1;
        return;
    }
    
    int currentLine = 1;
    char FEN[100] = {'\0'};
    while(fgets(FEN, 100, inputFile))
    {
        if(currentLine == lineNumber) break;
        currentLine++;
    }
    fclose(inputFile);
    
    if(FEN[0] == '\0')
    {
        DEBUG("Failed to read from file \"import/FEN.txt\"");
        board->victor = -1;
        return;
    }
    else if(FEN[0] == ';')
    {
        DEBUG("Target FEN line is a comment.");
        board->victor = -1;
        return;
    }
    else if(FEN[0] == ' ' || FEN[0] == '\n')
    {
        DEBUG("Target FEN line is whitespace.");
        board->victor = -1;
        return;
    }

    char fullBoardString[80] = {'\0'};
    char* boardStrings[8] = {NULL};
    char activeColor = '-';
    char castlingAvailability[5] = {'\0'};
    char enPassantTargetSquare[3] = {'\0'};
    int halfMoveClock, fullMoveCount = 0;
    sscanf(FEN, "%s %c %s %s %d %d\n", 
        fullBoardString, 
        &activeColor,
        castlingAvailability,
        enPassantTargetSquare,
        &halfMoveClock,
        &fullMoveCount);

    char* curBoardString = strtok(fullBoardString, "/");
    int curIndex = 7;
    while(curBoardString && curIndex >= 0)
    {
        boardStrings[curIndex--] = curBoardString;
        curBoardString = strtok(NULL, "/"); 
    }

    board->pawn_w = 0;
    board->knight_w = 0;
    board->bishop_w = 0;
    board->rook_w = 0;
    board->queen_w = 0;
    board->king_w = 0;
    board->pawn_b = 0;
    board->knight_b = 0;
    board->bishop_b = 0;
    board->rook_b = 0;
    board->queen_b = 0;
    board->king_b = 0;

    for(int row = 0; row < 8; row++)
    {
        if(boardStrings[row][0] != '8')
        {
            int columnOffset = 0;
            int index = 0;
            char currentChar;
            while((currentChar = boardStrings[row][index++]) != '\0' && columnOffset < 8)
            {
                //ASCII values for digits 0-9 (48 to 57 in decimal)
                if(currentChar >= '0'  && currentChar <= '9')
                {
                    columnOffset+= currentChar - '0';
                }
                else
                {
                    uint64_t currentSquareMask = (1ull<<(columnOffset + 8*row));
                    if(currentChar == 'K')
                    {
                        board->king_w|=currentSquareMask;
                        board->kingSquare_w = columnOffset + 8*row;
                    }
                    else if(currentChar == 'k')
                    {
                        board->king_b|=currentSquareMask;
                        board->kingSquare_b = columnOffset + 8*row;
                    }
                    else if(currentChar == 'Q') board->queen_w|=currentSquareMask;
                    else if(currentChar == 'q') board->queen_b|=currentSquareMask;
                    else if(currentChar == 'R') board->rook_w|=currentSquareMask;
                    else if(currentChar == 'r') board->rook_b|=currentSquareMask;
                    else if(currentChar == 'N') board->knight_w|=currentSquareMask;
                    else if(currentChar == 'n') board->knight_b|=currentSquareMask;
                    else if(currentChar == 'B') board->bishop_w|=currentSquareMask;
                    else if(currentChar == 'b') board->bishop_b|=currentSquareMask;
                    else if(currentChar == 'P') board->pawn_w|=currentSquareMask;
                    else if(currentChar == 'p') board->pawn_b|=currentSquareMask;
                    columnOffset++;
                }
            }
        }
    }
    board->pieces_w = board->king_w|board->queen_w|board->rook_w|board->knight_w|board->bishop_w|board->pawn_w;
    board->pieces_b = board->king_b|board->queen_b|board->rook_b|board->knight_b|board->bishop_b|board->pawn_b;
    board->pieces_all = board->pieces_b|board->pieces_w;

    board->flags = 0;
    //Castling Rights
    if(castlingAvailability[0] != '-')
    {
        for(int i = 0; i < 4; i++)
        {
            if(castlingAvailability[i] == 'K') board->flags|=1;
            else if(castlingAvailability[i] == 'Q') board->flags|=2;
            else if(castlingAvailability[i] == 'k') board->flags|=4;
            else if(castlingAvailability[i] == 'q') board->flags|=8;
            else break;
        }
    }

    //Check
    if(isThreatened(board, board->kingSquare_w, WHITE)) board->flags|=16;
    else if(isThreatened(board, board->kingSquare_b, BLACK)) board->flags|=32;

    //Turn
    if(activeColor == 'w') board->turn = WHITE;
    else board->turn = BLACK;

    //50 move rule
    board->movesSinceLastChange = halfMoveClock;
    board->halfMoveCount = fullMoveCount * 2;

    //History can't get imported this way. Start from scratch.
    board->moveStackTop = NULL;
    
    if(enPassantTargetSquare[0] != '-') board->enPassantSquare = getSquareNumber(enPassantTargetSquare);

    if(board->ht) destroy_hashTable_pos(board->ht);
    board->ht = create_hashTable_pos();
    increment_table_value(board->ht, board);
}

//Resets the board to an opening position
bitboard* create_board()
{
    bitboard* board = CALLOC(1, sizeof(bitboard));

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

    //Castling Rights & Check
    board->flags = 0xF;

    //Turn
    board->turn = WHITE;

    //Checkmate
    board->victor = 0;

    //50 move rule
    board->movesSinceLastChange = 0;

    //History
    board->moveStackTop = NULL;
    board->ht = create_hashTable_pos();

    board->enPassantSquare = -1;

    //Other
    board->halfMoveCount = 0;
    increment_table_value(board->ht, board);

    return board;
}

void destroy_board(bitboard* board)
{
    destroy_hashTable_pos(board->ht);
    while(board->moveStackTop)
    {
        move* tempMove = board->moveStackTop;
        board->moveStackTop = board->moveStackTop->nextMove;
        FREE(tempMove);
    }
    FREE(board);
}

void copy_board(bitboard* dest, bitboard* source)
{
    if(!dest || !source) return;

    dest->pawn_w = source->pawn_w;
    dest->pawn_b = source->pawn_b;
    
    dest->bishop_w = source->bishop_w;
    dest->bishop_b = source->bishop_b;
    
    dest->knight_w = source->knight_w;
    dest->knight_b = source->knight_b;
    
    dest->rook_w = source->rook_w;
    dest->rook_b = source->rook_b;
    
    dest->queen_w = source->queen_w;
    dest->queen_b = source->queen_b;
    
    dest->king_w = source->king_w;
    dest->king_b = source->king_b;

    dest->pieces_w = source->pieces_w;
    dest->pieces_b = source->pieces_b;
    dest->pieces_all = source->pieces_all;

    dest->kingSquare_b = source->kingSquare_b;
    dest->kingSquare_w  = source->kingSquare_w;

    //Castling Rights & Check
    dest->flags = source->flags;

    //Turn
    dest->turn = source->turn;

    //Checkmate
    dest->victor = source->victor;

    //History: 
    
    //Make sure to free a preexisting move stack to avoid memory leaks.
    while(dest->moveStackTop)
    {
        move* tempMove = dest->moveStackTop;
        dest->moveStackTop = dest->moveStackTop->nextMove;
        FREE(tempMove);
    }
    if(source->moveStackTop)
    {
        dest->moveStackTop = CALLOC(1, sizeof(move));
        move* currentMove = source->moveStackTop;
        memcpy(dest->moveStackTop, currentMove, sizeof(move));
        dest->moveStackTop->nextMove = NULL;
        move* currentCopy = dest->moveStackTop;
        currentMove = currentMove->nextMove;
        while(currentMove)
        {
            move* nextCopy= CALLOC(1, sizeof(move));
            memcpy(nextCopy, currentMove, sizeof(move));
            nextCopy->nextMove = NULL;
            currentCopy->nextMove = nextCopy;
            currentCopy = nextCopy;
            currentMove = currentMove->nextMove;
        }
    }
    else
    {
        dest->moveStackTop = NULL;
    }

    dest->enPassantSquare = source->enPassantSquare;
    
    destroy_hashTable_pos(dest->ht);
    dest->ht = copy_hashTable_pos(source->ht);
}

int findPieceOnSquare(bitboard* board, int square)
{
    if(board == NULL)
    {
        DEBUG("Cannot find piece on square. Board is NULL");
        return 0;
    }
    else if(square < 0 || square > 63)
    {
        DEBUG("Cannot check square %d - out of bounds [0, 63]", square);
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
        DEBUG("Cannot clear square. Board is NULL");
        return;
    }
    else if(square < 0 || square > 63)
    {
        DEBUG("Cannot clear square %d - out of bounds [0, 63]", square);
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
        DEBUG("Cannot set square. Board is NULL");
        return;
    }
    else if(square < 0 || square > 63)
    {
        DEBUG("Cannot set square %d - out of bounds [0, 63]", square);
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
        DEBUG("Cannot print board. Board is NULL");
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
    if(printHistory) dumpMoves(board);
}


void values_print(bitboard* board)
{
    if(board == NULL)
    {
        DEBUG("Cannot print board values. Board is NULL");
        return;
    }

    printf("ALL: %016llx\n", board->pieces_all);
    printf("WHITE/BLACK: %016llx | %016llx\n", board->pieces_w, board->pieces_b);
    if(board->pawn_w || board->pawn_b) printf("PAWN: %016llx | %016llx\n", board->pawn_w, board->pawn_b);
    if(board->knight_w || board->knight_b) printf("KNIGHT: %016llx | %016llx\n", board->knight_w, board->knight_b);
    if(board->bishop_w || board->bishop_b) printf("BISHOP: %016llx | %016llx\n", board->bishop_w, board->bishop_b);
    if(board->rook_w || board->rook_b) printf("ROOK: %016llx | %016llx\n", board->rook_w, board->rook_b);
    if(board->queen_w || board->queen_b) printf("QUEEN: %016llx | %016llx\n", board->queen_w, board->queen_b);
    printf("KING: %016llx | %016llx\n\t(%d) (%d)\n", board->king_w, board->king_b, board->kingSquare_w, board->kingSquare_b);

    if(board->flags&0xF)
    {
        printf("Castling Rights: ");
        if(KINGSIDE_CASTLE_WHITE(board->flags)) printf("K");
        if(QUEENSIDE_CASTLE_WHITE(board->flags)) printf("Q");
        if(KINGSIDE_CASTLE_BLACK(board->flags)) printf("k");
        if(QUEENSIDE_CASTLE_BLACK(board->flags)) printf("q");
        printf("\n");
    }

    if(board->flags&0x30)
    {
        printf("Check: ");
        if(INCHECK_W(board->flags)) printf("W\n");
        else printf("B\n");
        printf("\n");
    }

    if(board->enPassantSquare != -1) printf("En passant square: %d\n", board->enPassantSquare);
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
        DEBUG("Failed to push move");
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
        DEBUG("Failed to pop move from null board");
        return NULL;
    }

    move* tempMove = board->moveStackTop;
    if(!tempMove) return NULL;
    board->moveStackTop = board->moveStackTop->nextMove;
    tempMove->nextMove = NULL;
    return tempMove;
}

move* createMove(int startSquare, int endSquare, int promoteTo, int piece, int capturedPiece, int capturedPieceSquare, bitboard* prevBoard)
{
    move* m = CALLOC(1, sizeof(move));
    if(!m) return NULL;

    m->startSquare = startSquare;
    m->endSquare = endSquare;
    m->promoteTo = promoteTo;
    m->piece = piece;
    m->capturedPiece = capturedPiece;
    m->capturedPieceSquare = capturedPieceSquare;
    m->flags = prevBoard->flags;
    m->previousMovesSinceLastChange = prevBoard->movesSinceLastChange;
    m->prevEnPassantSquare = prevBoard->enPassantSquare;
    m->nextMove = NULL;
    return m;
}

void recursivePrint(move* m)
{
    if(m)
    {
        recursivePrint(m->nextMove);

        char startSquareName[3] = {'\0'};
        char endSquareName[3] = {'\0'};
        getSquareName(m->startSquare, startSquareName);
        getSquareName(m->endSquare, endSquareName);

        printf("%s%s", startSquareName, endSquareName);
        if(m->promoteTo)
        {
            if(ISKNIGHT(m->promoteTo)) printf("n");
            else if(ISBISHOP(m->promoteTo)) printf("b");
            else if(ISROOK(m->promoteTo)) printf("r");
            else if(ISQUEEN(m->promoteTo)) printf("q");
        }
        printf(" ");
    }
}

void dumpMoves(bitboard* board)
{
    recursivePrint(board->moveStackTop);
    printf("\n");
}
