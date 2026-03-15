#include "../include/bitboard.h"
#include "../include/moves.h"
#include <string.h>
#include "../include/debug.h"

move** generateMoveList(bitboard* board, int capturesOnly)
{
    if(board->victor) 
    {
        DEBUG("Cannot generate moves for terminated game; Victor=%02x", board->victor);
        return NULL;
    }

    int size = 0;
    int capacity = 32;
    move** movesList = CALLOC(capacity, sizeof(move*));
    if(!movesList) return NULL;
    for(int currentSquare = 0; currentSquare < 64; currentSquare++)
    {
        if((board->turn == WHITE && board->pieces_w&(1ull<<currentSquare)) ||
            (board->turn == BLACK && board->pieces_b&(1ull<<currentSquare)))
        {
            int piece = findPieceOnSquare(board, currentSquare);
            move** tempMoves = generatePieceMoves(board, piece, currentSquare, piece, capturesOnly);
            int index = 0;
            while(tempMoves[index] != NULL)
            {
                if(size >= capacity)
                {
                    capacity+=8;
                    movesList = REALLOC(movesList, capacity*sizeof(move*));
                    if(!movesList) return NULL;
                }
                movesList[size] = CALLOC(1, sizeof(move));
                memcpy(movesList[size], tempMoves[index], sizeof(move));
                size++;
                index++;
            }
            freeMoveList(tempMoves);
        }
    }


    //Game over, no moves available.
    //stalemate/checkmate gets decided elsewhere.
    if(size == 0) 
    {
        FREE(movesList);
        return NULL;
    }

    movesList = REALLOC(movesList, (size+1)*sizeof(move*));
    if(!movesList) return NULL;
    movesList[size] = NULL;

    return movesList;
}

move** generatePieceMoves(bitboard* board, int piece, int square, int color, int capturesOnly)
{
    if(piece == 0)
    {
        DEBUG("Cannot generate piece moves on invalid piece type.");
        return NULL;
    }
    else if(square < 0 || square > 63)
    {
        DEBUG("Cannot generate piece moves from invalid square.");
        return NULL;
    }
    else if(!ISWHITE(color) && !ISBLACK(color))
    {
        DEBUG("Cannot generate piece moves from invalid color.");
        return NULL;
    }
    move** moveArray = CALLOC(40, sizeof(move*));
    if(!moveArray) return NULL;
    int size = 0;
    uint64_t moveMask;

    uint64_t opposingPieceMask;
    if(ISWHITE(color)) opposingPieceMask = board->pieces_b;
    else opposingPieceMask = board->pieces_w;

    if(ISPAWN(piece))
    {
        moveMask = pawnMoves(board, square, color);
        int currentSquare = 0;
        while(moveMask)
        {
            if(moveMask&1 && (!capturesOnly || (capturesOnly && opposingPieceMask&1) || board->enPassantSquare == currentSquare))
            {
                int targetPiece = findPieceOnSquare(board, currentSquare);
                int targetSquare = currentSquare;

                if((ISWHITE(color) && currentSquare >= 56) || (ISBLACK(color) && currentSquare <= 7))
                {
                    //Promotion
                    moveArray[size] = createMove(square, currentSquare, KNIGHT, PAWN|color, targetPiece, targetSquare, board);
                    size++;
                    
                    moveArray[size] = createMove(square, currentSquare, BISHOP, PAWN|color, targetPiece, targetSquare, board);
                    size++;
                    
                    moveArray[size] = createMove(square, currentSquare, ROOK, PAWN|color, targetPiece, targetSquare, board);
                    size++;
                    
                    moveArray[size] = createMove(square, currentSquare, QUEEN, PAWN|color, targetPiece, targetSquare, board);
                    size++;
                }
                else
                {
                    if(board->enPassantSquare == currentSquare)
                    {
                        targetPiece = PAWN;
                        if(ISWHITE(color)) 
                        {
                            targetPiece|=BLACK;
                            targetSquare-=8;
                        }
                        else
                        {
                            targetPiece|=WHITE;
                            targetSquare+=8;
                        }
                    }

                    moveArray[size] = createMove(square, currentSquare, 0, PAWN|color, targetPiece, targetSquare, board);
                    size++;
                }
            }
            moveMask = moveMask >> 1;
            opposingPieceMask = opposingPieceMask >> 1;
            currentSquare++;
        }
    }
    else 
    {
        if(ISKNIGHT(piece))
        {
            moveMask = knightMoves(board, square, color);
        }
        else if(ISBISHOP(piece))
        {
            moveMask = bishopMoves(board, square, color);
        }
        else if(ISROOK(piece))
        {
            moveMask = rookMoves(board, square, color); 
        }
        else if(ISQUEEN(piece))
        {
            moveMask = queenMoves(board, square, color);
        }
        else if(ISKING(piece))
        {
            moveMask = kingMoves(board, square, color, 0);
        }
        int currentSquare = 0;

        while(moveMask)
        {
            if(moveMask&1  && (!capturesOnly || (capturesOnly && opposingPieceMask&1)))
            {
                int targetPiece = findPieceOnSquare(board, currentSquare);
                moveArray[size] = createMove(square, currentSquare, 0, piece|color, targetPiece, currentSquare, board);
                size++;
            }
            moveMask = moveMask >> 1;
            opposingPieceMask = opposingPieceMask >> 1;
            currentSquare++;
        }
    }

    move** legalMoveArray = CALLOC(size, sizeof(move*));
    bitboard* tempBoard = create_board();
    int legalSize = 0;
    for(int i = 0; i < size; i++)
    {
        copy_board(tempBoard, board, 0);
        //Check if move is legal.
        if(movePiece(tempBoard, moveArray[i]->startSquare, moveArray[i]->endSquare, moveArray[i]->piece, moveArray[i]->promoteTo) == 0)
        {
            if((ISWHITE(color) && isThreatened(tempBoard, tempBoard->kingSquare_w, WHITE)) ||
                (ISBLACK(color) && isThreatened(tempBoard, tempBoard->kingSquare_b, BLACK)))
            {
                continue;
            }

            legalMoveArray[legalSize] = CALLOC(1, sizeof(move));
            memcpy(legalMoveArray[legalSize], moveArray[i], sizeof(move));
            legalSize++;
        }
    }
    destroy_board(tempBoard);
    freeMoveList(moveArray);

    legalMoveArray = REALLOC(legalMoveArray, (legalSize+1)*sizeof(move));
    if(legalMoveArray == NULL) return NULL;
    legalMoveArray[legalSize] = NULL;

    return legalMoveArray;
}

void freeMoveList(move** moveList)
{
    if(!moveList) return;

    int index = 0;
    while(moveList[index])
    {
        FREE(moveList[index]);
        moveList[index] = NULL;
        index++;
    }

    if(moveList[index]) FREE(moveList[index]);
    FREE(moveList);
    moveList = NULL;
}

int isPinned(bitboard* board, int questionedSquare, int kingSquare, int kingColor)
{
    uint64_t rookqueen = 0;
    uint64_t bishopqueen = 0;

    if(ISBLACK(kingColor))
    {
        rookqueen = board->rook_w|board->queen_w;
        bishopqueen = board->bishop_w|board->queen_w;
    }
    else if(ISWHITE(kingColor))
    {
        rookqueen = board->rook_b|board->queen_b;
        bishopqueen = board->bishop_b|board->queen_b;
    }

    if(getRow(questionedSquare) == getRow(kingSquare))
    {
        //Check for discovered rook/queen attack
        if(questionedSquare < kingSquare)
        {
            //King is on a higher column, same row.

            //Check for pieces between questionedSquare and king square.
            uint64_t greaterMask = (1ull << (kingSquare + 1)) - 1;
            uint64_t lesserMask = (1ull << questionedSquare) - 1;
            uint64_t squaresBetweenMask = greaterMask ^ lesserMask;

            if(!(board->pieces_all&squaresBetweenMask))
            {
                //No pieces between questionedSquare and kingSquare
                //Find the closest attacking rook/queen in other direction.
                if(questionedSquare%8 >= 0)
                {
                    int checkedSquare = questionedSquare - 1;
                    while(checkedSquare%8 >= 0)
                    {
                        if(((1ull<<checkedSquare)&rookqueen) != 0)
                        {
                            //Opposing Rook or queen is looking at king across questionedSquare.
                            return PIN_TYPE_LEFT;
                        }
                        else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                        {
                            //Blocking piece
                            break;
                        }
                        checkedSquare--;
                    }
                }
                //Nothing found pinning the questioned square.
                return PIN_TYPE_NONE;
            }
        }
        else
        {
            //King is on a lower column, same row

            //Check for pieces between questionedSquare and king square.
            uint64_t greaterMask = (1ull << (questionedSquare + 1)) - 1;
            uint64_t lesserMask = (1ull << kingSquare) - 1;
            uint64_t squaresBetweenMask = greaterMask ^ lesserMask;

            if(!(board->pieces_all&squaresBetweenMask))
            {
                //Find the closest attacking rook/queen in other direction.
                if(questionedSquare%8 <= 7)
                {
                    int checkedSquare = questionedSquare + 1;
                    while(checkedSquare%8 <= 7)
                    {
                        if(((1ull<<checkedSquare)&rookqueen) != 0)
                        {
                            //White rook or queen is looking at black king.
                            return PIN_TYPE_RIGHT;
                        }
                        else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                        {
                            //Blocking piece
                            break;
                        }
                        checkedSquare++;
                    }
                }
                //Nothing found pinning the questioned square.
                return PIN_TYPE_NONE;
            }
        }
    }
    else if (getColumn(questionedSquare) == getColumn(kingSquare))
    {
        if(questionedSquare < kingSquare)
        {
            //King is above questioned square.

            //Search for blocking pieces between the two squares.
            uint64_t squaresBetweenMask = 0;
            for(int i = questionedSquare + 8; i < kingSquare; i+=8)
            {
                squaresBetweenMask |= (1ull << i);
            }

            if(!(board->pieces_all&squaresBetweenMask))
            {
                //Find the closest attacking rook/queen in other direction.
                if(questionedSquare/8 >= 0)
                {
                    int checkedSquare = questionedSquare - 8;
                    while(checkedSquare/8 >= 0)
                    {
                        if(((1ull<<checkedSquare)&rookqueen) != 0)
                        {
                            //Attacking rook or queen is looking at king across questioned square.
                            return PIN_TYPE_BELOW;
                        }
                        else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                        {
                            //Blocking piece
                            break;
                        }
                        checkedSquare-=8;
                    }
                }
                //Nothing found.
                return PIN_TYPE_NONE;
            }
        }
        else
        {
            //King is below questioned square.

            //Search for blocking pieces between the two squares.
            uint64_t squaresBetweenMask = 0;
            for(int i = kingSquare + 8; i < questionedSquare; i+=8)
            {
                squaresBetweenMask |= (1ull << i);
            }

            if(!(board->pieces_all&squaresBetweenMask))
            {
                //Find the closest attacking rook/queen in other direction.
                if(questionedSquare/8 <= 7)
                {
                    int checkedSquare = questionedSquare + 8;
                    while(checkedSquare/8 <= 7)
                    {
                        if(((1ull<<checkedSquare)&rookqueen) != 0)
                        {
                            //White rook or queen is looking at black king.
                            return PIN_TYPE_ABOVE;
                        }
                        else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                        {
                            //Blocking piece
                            break;
                        }
                        checkedSquare+=8;
                    }
                }
                //Nothing found
                return PIN_TYPE_NONE;
            }
        }
    }
    else
    {
        int startRow = getRow(questionedSquare);
        int kingRow = getRow(kingSquare);
        int startColumn = getColumn(questionedSquare);
        int kingColumn = getColumn(kingSquare);

        //Check if black king is on same diagonal as questionedSquare to check for discovered queen/bishop attack.
        if(abs(startRow - kingRow) == abs(startColumn - kingColumn))
        {
            if(startRow < kingRow)
            {
                if(startColumn < kingColumn)
                {
                    //questionedSquare is to the bottomleft of black king.

                    //Search for blocking pieces between the two squares.
                    uint64_t squaresBetweenMask = 0;
                    for(int i = questionedSquare + 9; i < kingSquare; i+=9)
                    {
                        squaresBetweenMask |= (1ull << i);
                    }

                    if(!(board->pieces_all&squaresBetweenMask))
                    {
                        //Find the closest attacking bishop/queen to the bottomleft.
                        if(questionedSquare >= 9 && getColumn(questionedSquare) > 1)
                        {
                            int checkedSquare = questionedSquare - 9;
                            while(checkedSquare >= 0 && getColumn(checkedSquare) < getColumn(questionedSquare) && getRow(checkedSquare) < getRow(questionedSquare))
                            {
                                if(((1ull<<checkedSquare)&bishopqueen) != 0)
                                {
                                    //White rook or queen is looking at black king.
                                    return PIN_TYPE_DOWNLEFT;
                                }
                                else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                                {
                                    //Blocking piece
                                    break;
                                }
                                checkedSquare-=9;
                            }
                        }
                        //Nothing found
                        return PIN_TYPE_NONE;
                    }
                }
                else
                {
                    //questionedSquare is to the bottomright of black king.

                    //Search for blocking pieces between the two squares.
                    uint64_t squaresBetweenMask = 0;
                    for(int i = questionedSquare + 7; i < kingSquare; i+=7)
                    {
                        squaresBetweenMask |= (1ull << i);
                    }

                    if(!(board->pieces_all&squaresBetweenMask))
                    {
                        //Find the closest attacking bishop/queen to the bottomright.
                        if(questionedSquare >= 8 && getColumn(questionedSquare) < 8) 
                        {
                            int checkedSquare = questionedSquare - 7;
                            while(checkedSquare >= 0 && getColumn(checkedSquare) > getColumn(questionedSquare) && getRow(checkedSquare) < getRow(questionedSquare))
                            {
                                if(((1ull<<checkedSquare)&bishopqueen) != 0)
                                {
                                    //White rook or queen is looking at black king.
                                    return PIN_TYPE_DOWNRIGHT;
                                }
                                else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                                {
                                    //Blocking piece
                                    break;
                                }
                                checkedSquare-=7;
                            }
                        }
                        //Nothing found
                        return PIN_TYPE_NONE;
                    }
                }
            }
            else
            {
                if(startColumn < kingColumn)
                {
                    //questionedSquare is to the topleft of black king.

                    //Search for blocking pieces between the two squares.
                    uint64_t squaresBetweenMask = 0;
                    for(int i = kingSquare + 7; i < questionedSquare; i+=7)
                    {
                        squaresBetweenMask |= (1ull << i);
                    }

                    if(!(board->pieces_all&squaresBetweenMask))
                    {
                        //Find the closest attacking bishop/queen to the topleft.
                        if(questionedSquare < 56 && getColumn(questionedSquare) > 1)
                        {
                            int checkedSquare = questionedSquare + 7;
                            while(checkedSquare <= 63 && getColumn(checkedSquare) < getColumn(questionedSquare) && getRow(checkedSquare) > getRow(questionedSquare))
                            {
                                if(((1ull<<checkedSquare)&bishopqueen) != 0)
                                {
                                    //White rook or queen is looking at black king.
                                    return PIN_TYPE_UPLEFT;
                                }
                                else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                                {
                                    //Blocking piece
                                    break;
                                }
                                checkedSquare+=7;
                            }
                        }
                        //Nothing found
                        return PIN_TYPE_NONE;
                    }
                }
                else
                {
                    //questionedSquare is to the topright of black king.

                    //Search for blocking pieces between the two squares.
                    uint64_t squaresBetweenMask = 0;
                    for(int i = kingSquare + 9; i < questionedSquare; i+=9)
                    {
                        squaresBetweenMask |= (1ull << i);
                    }
                    if(!(board->pieces_all&squaresBetweenMask))
                    {
                        //Find the closest attacking bishop/queen to the topright.
                        if(questionedSquare < 56 && getColumn(questionedSquare) < 8)
                        {
                            int checkedSquare = questionedSquare + 9;
                            while(checkedSquare <= 63 && getColumn(checkedSquare) > getColumn(questionedSquare) && getRow(checkedSquare) > getRow(questionedSquare))
                            {
                                if(((1ull<<checkedSquare)&bishopqueen) != 0)
                                {
                                    //White rook or queen is looking at black king.
                                    return PIN_TYPE_UPRIGHT;
                                }
                                else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                                {
                                    //Blocking piece
                                    break;
                                }
                                checkedSquare+=9;
                            }
                        }
                        //Nothing found
                        return PIN_TYPE_NONE;
                    }
                }
            }
        }
    }
    return PIN_TYPE_NONE;
}

int isThreatened(bitboard* board, int square, int squareColor)
{
    uint64_t bishopqueen;
    uint64_t rookqueen;
    if(ISWHITE(squareColor))
    {
        bishopqueen = board->bishop_b|board->queen_b;
        rookqueen = board->rook_b|board->queen_b;
        if(((1ull<<(square+7)&board->pawn_b) || (1ull<<(square+9)&board->pawn_b))) return THREAT_TYPE_PAWN;
        if(knightMoves(board, square, squareColor)&(board->knight_b)) return THREAT_TYPE_KNIGHT;
        if(kingMoves(board, square, squareColor, 1)&(board->king_b)) return THREAT_TYPE_KING;
    }
    else if(ISBLACK(squareColor))
    {
        bishopqueen = board->bishop_w|board->queen_w;
        rookqueen = board->rook_w|board->queen_w;
        if(((1ull<<(square-7)&board->pawn_w) || (1ull<<(square-9)&board->pawn_w))) return THREAT_TYPE_PAWN;
        if(knightMoves(board, square, squareColor)&(board->knight_w)) return THREAT_TYPE_KNIGHT;
        if(kingMoves(board, square, squareColor, 1)&(board->king_w)) return THREAT_TYPE_KING;
    }
    if(bishopMoves(board, square, squareColor)&(bishopqueen)) return THREAT_TYPE_BISHOPQUEEN;
    if(rookMoves(board, square, squareColor)&(rookqueen)) return THREAT_TYPE_ROOKQUEEN;

    return THREAT_TYPE_NONE;
}

uint64_t pawnMoves(bitboard* board, int square, int color)
{
    uint64_t returnValue = 0;
    if(ISWHITE(color))
    {
        //Frontleft capture check
        if(board->pieces_b&(1ull<<(square+7)) && getColumn(square) > 1) returnValue|=(1ull<<(square+7));
        
        //Frontright capture check
        if(board->pieces_b&(1ull<<(square+9)) && getColumn(square) < 8) returnValue|=(1ull<<(square+9));

        //One move forward check
        if(!(board->pieces_all&(1ull<<(square+8))))
        {
            returnValue|=(1ull<<(square+8));

            //Two moves forward check
            if(getRow(square) == 2 && !(board->pieces_all&(1ull<<(square+16)))) returnValue|=(1ull<<(square+16));
        }

        //En passant
        if(board->enPassantSquare != -1 && 
            (board->enPassantSquare - square == 7 || board->enPassantSquare - square == 9) && 
            abs(getColumn(board->enPassantSquare) - getColumn(square)) == 1)
        {
            returnValue|=(1ull<<(board->enPassantSquare));
        }
    }
    else
    {
        //Frontleft capture check
        if(board->pieces_w&(1ull<<(square-7)) && getColumn(square) < 8) returnValue|=(1ull<<(square-7));
        
        //Frontright capture check
        if(board->pieces_w&(1ull<<(square-9)) && getColumn(square) > 1) returnValue|=(1ull<<(square-9));

        //One move forward check
        if(!(board->pieces_all&(1ull<<(square-8))))
        {
            returnValue|=(1ull<<(square-8));

            //Two moves forward check
            if(getRow(square) == 7 && !(board->pieces_all&(1ull<<(square-16)))) returnValue|=(1ull<<(square-16));
        }
        
         //En passant
        if(board->enPassantSquare != -1 && 
            (board->enPassantSquare - square == -7 || board->enPassantSquare - square == -9) && 
            abs(getColumn(board->enPassantSquare) - getColumn(square)) == 1)
        {
            returnValue|=(1ull<<(board->enPassantSquare));
        }
    }

    return returnValue;
}

uint64_t knightMoves(bitboard* board, int square, int color)
{
    uint64_t returnedValue = 0;
    int row = getRow(square);
    int column = getColumn(square);

    /*
     *      - 2 - 3 -
     *      1 - - - 4
     *      - - N - -
     *      8 - - - 5
     *      - 7 - 6 -
     * 
     *  1: endSquare = square + 6
     *  2: endSquare = square + 15
     *  3: endSquare = square + 17
     *  4: endSquare = square + 10
     *  5: endSquare = square - 6
     *  6: endSquare = square - 15
     *  7: endSquare = square - 17
     *  8: endSquare = square - 10
     */

    if(column - 2 >= 1 && row + 1 <= 8) returnedValue|=(1ull<<(square+6));
    if(column - 1 >= 1 && row + 2 <= 8) returnedValue|=(1ull<<(square+15));
    if(column + 1 <= 8 && row + 2 <= 8) returnedValue|=(1ull<<(square+17));
    if(column + 2 <= 8 && row + 1 <= 8) returnedValue|=(1ull<<(square+10));
    if(column + 2 <= 8 && row - 1 >= 1) returnedValue|=(1ull<<(square-6));
    if(column + 1 <= 8 && row - 2 >= 1) returnedValue|=(1ull<<(square-15));
    if(column - 1 >= 1 && row - 2 >= 1) returnedValue|=(1ull<<(square-17));
    if(column - 2 >= 1 && row - 1 >= 1) returnedValue|=(1ull<<(square-10));

    if(ISWHITE(color))
    {
        returnedValue^=(returnedValue&board->pieces_w);
    }
    else
    {
        returnedValue^=(returnedValue&board->pieces_b);
    }
     
    return returnedValue;
}

uint64_t bishopMoves(bitboard* board, int square, int color)
{
    uint64_t potentialMoves = 0;

    //Topleft
    int questionedSquare = square + 7;
    while(questionedSquare >= 0 && questionedSquare <= 63 && getColumn(questionedSquare) < getColumn(square) && getRow(questionedSquare) > getRow(square))
    {
        uint64_t questionedMask = (1ull<<questionedSquare);
        if((ISWHITE(color) && (board->pieces_b&questionedMask)) || (ISBLACK(color) && (board->pieces_w&questionedMask)))
        {
            //Opposite color target
            potentialMoves|=questionedMask;
            break;
        }
        else if(board->pieces_all&questionedMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=questionedMask;
        }

        questionedSquare+=7;
    }

    //Topright
    questionedSquare = square + 9;
    while(questionedSquare >= 0 && questionedSquare <= 63 && getColumn(questionedSquare) > getColumn(square) && getRow(questionedSquare) > getRow(square))
    {
        uint64_t questionedMask = (1ull<<questionedSquare);
        if((ISWHITE(color) && (board->pieces_b&questionedMask)) || (ISBLACK(color) && (board->pieces_w&questionedMask)))
        {
            //Opposite color target
            potentialMoves|=questionedMask;
            break;
        }
        else if(board->pieces_all&questionedMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=questionedMask;
        }

        questionedSquare+=9;
    }

    //Bottomleft
    questionedSquare = square - 9;
    while(questionedSquare >= 0 && questionedSquare <= 63 && getColumn(questionedSquare) < getColumn(square) && getRow(questionedSquare) < getRow(square))
    {
        uint64_t questionedMask = (1ull<<questionedSquare);
        if((ISWHITE(color) && (board->pieces_b&questionedMask)) || (ISBLACK(color) && (board->pieces_w&questionedMask)))
        {
            //Opposite color target
            potentialMoves|=questionedMask;
            break;
        }
        else if(board->pieces_all&questionedMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=questionedMask;
        }

        questionedSquare-=9;
    }

    //Bottomright
    questionedSquare = square - 7;
    while(questionedSquare >= 0 && questionedSquare <= 63 && getColumn(questionedSquare) > getColumn(square) && getRow(questionedSquare) < getRow(square))
    {
        uint64_t questionedMask = (1ull<<questionedSquare);
        if((ISWHITE(color) && (board->pieces_b&questionedMask)) || (ISBLACK(color) && (board->pieces_w&questionedMask)))
        {
            //Opposite color target
            potentialMoves|=questionedMask;
            break;
        }
        else if(board->pieces_all&questionedMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=questionedMask;
        }

        questionedSquare-=7;
    }

    return potentialMoves;
}

uint64_t rookMoves(bitboard* board, int square, int color)
{
    uint64_t potentialMoves = 0;
    uint64_t tempMask = 0;
    //Above
    for(int column = getColumn(square) + 1; column <= 8; column++)
    {
        tempMask = (1ull<<(square + (column - getColumn(square))));
        if((ISWHITE(color) && board->pieces_b&tempMask) || (ISBLACK(color) && board->pieces_w&tempMask))
        {
            //Opposite color target
            potentialMoves|=tempMask;
            break;
        }
        else if(board->pieces_all&tempMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=tempMask;
        }
    }

    //Below
    for(int column = getColumn(square) - 1; column >= 1; column--)
    {
        tempMask = (1ull<<(square + (column - getColumn(square))));
        if((ISWHITE(color) && board->pieces_b&tempMask) || (ISBLACK(color) && board->pieces_w&tempMask))
        {
            //Opposite color target
            potentialMoves|=tempMask;
            break;
        }
        else if(board->pieces_all&tempMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=tempMask;
        }
    }

    //Right
    for(int row = getRow(square) + 1; row <= 8; row++)
    {
        tempMask = (1ull<<(square + 8*(row - getRow(square))));
        if((ISWHITE(color) && board->pieces_b&tempMask) || (ISBLACK(color) && board->pieces_w&tempMask))
        {
            //Opposite color target
            potentialMoves|=tempMask;
            break;
        }
        else if(board->pieces_all&tempMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=tempMask;
        }
    }

    //Left
    for(int row = getRow(square) - 1; row >= 1; row--)
    {
        tempMask = (1ull<<(square + 8*(row - getRow(square))));
        if((ISWHITE(color) && board->pieces_b&tempMask) || (ISBLACK(color) && board->pieces_w&tempMask))
        {
            //Opposite color target
            potentialMoves|=tempMask;
            break;
        }
        else if(board->pieces_all&tempMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=tempMask;
        }
    }

    return potentialMoves;
}

uint64_t queenMoves(bitboard* board, int square, int color)
{
    return rookMoves(board, square, color)|bishopMoves(board, square, color);
}

uint64_t kingMoves(bitboard* board, int square, int color, int ignoreThreats)
{
    uint64_t returnedValue = 0;
    int row = getRow(square);
    int column = getColumn(square);

    /*
     *     
     *      1 2 3
     *      4 K 5
     *      6 7 8
     * 
     *  1: endSquare = square + 7
     *  2: endSquare = square + 8
     *  3: endSquare = square + 9
     *  4: endSquare = square - 1
     *  5: endSquare = square + 1
     *  6: endSquare = square - 9
     *  7: endSquare = square - 8
     *  8: endSquare = square - 7
     */
    if(column - 1 >= 1 && row + 1 <= 8 && (ignoreThreats || !isThreatened(board, square+7, color))) returnedValue|=(1ull<<(square+7));
    if(row + 1 <= 8 && (ignoreThreats || !isThreatened(board, square+8, color))) returnedValue|=(1ull<<(square+8));
    if(column + 1 <= 8 && row + 1 <= 8 && (ignoreThreats || !isThreatened(board, square+9, color))) returnedValue|=(1ull<<(square+9));
    if(column - 1 >= 1 && (ignoreThreats || !isThreatened(board, square-1, color))) returnedValue|=(1ull<<(square-1));
    if(column + 1 <= 8 && (ignoreThreats || !isThreatened(board, square+1, color))) returnedValue|=(1ull<<(square+1));
    if(column - 1 >= 1 && row - 1 >= 1 && (ignoreThreats || !isThreatened(board, square-9, color))) returnedValue|=(1ull<<(square-9));
    if(row - 1 >= 1 && (ignoreThreats || !isThreatened(board, square-8, color))) returnedValue|=(1ull<<(square-8));
    if(column + 1 <= 8 && row - 1 >= 1 && (ignoreThreats || !isThreatened(board, square-7, color))) returnedValue|=(1ull<<(square-7));



    if(ISWHITE(color))
    {
        returnedValue^=(returnedValue&board->pieces_w);

        //Castling
        if(!ignoreThreats)
        {
            uint64_t betweenMask = 0x60; //Squares 5 and 6 between king on 4 and rook on 7

            //White kingside
            if((board->flags&1) && !(board->flags&16) && !(board->pieces_all&betweenMask) && !isThreatened(board, square+1, color) && !isThreatened(board, square+2, color)) returnedValue|=0x40;

            betweenMask = 0xE; //Square 1 and 2 and 3 between rook on 0 and king on 4
            //White queenside
            if((board->flags&2) && !(board->flags&16) && !(board->pieces_all&betweenMask) && !isThreatened(board, square-1, color) && !isThreatened(board, square-2, color)) returnedValue|=0x4;
        }
    }
    else
    {
        returnedValue^=(returnedValue&board->pieces_b);

        //Castling
        if(!ignoreThreats)
        {
            uint64_t betweenMask = 0x6000000000000000; //Squares 61 and 62 between king on 60 and rook on 63

            //Black kingside
            if((board->flags&4) && !(board->flags&32) && !(board->pieces_all&betweenMask) && !isThreatened(board, square+1, color) && !isThreatened(board, square+2, color)) returnedValue|=0x4000000000000000;

            betweenMask = 0x0E00000000000000; //Square 57 and 58 and 59 between rook on 56 and king on 60
            //Black queenside
            if((board->flags&8) && !(board->flags&32) && !(board->pieces_all&betweenMask) && !isThreatened(board, square-1, color) && !isThreatened(board, square-2, color)) returnedValue|=0x0400000000000000;
        }
    }
    
    return returnedValue;
}

int movePawn(bitboard *board, int startSquare, int endSquare, int color, int promoteTo)
{
    if(board == NULL)
    {
        DEBUG("Cannot move pawn. Board is NULL");
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Pawn start square %d - out of bounds [0, 63]", startSquare);
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Pawn end square %d - out of bounds [0, 63]", endSquare);
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for pawn move. (%02x)", color);
        return -1;
    }

    //Auto-queen
    if(promoteTo == 0) promoteTo = QUEEN;

    int difference = abs(startSquare - endSquare);

    
    if(difference == 8 || difference == 16)
    {
        //1 or 2 moves forward

        //Set new board positions.
        board_clear_square(board, startSquare, (color|PAWN));

        if((ISWHITE(color) && endSquare > 55) || (ISBLACK(color) && endSquare < 8))
        {
            //Promotion
            board_set(board, endSquare, (color|promoteTo));
        }
        else
        {
            board_set(board, endSquare, (color|PAWN));
        }
        
        //Update en passant square if necessary.
        if(difference == 16)
        {
            if(ISWHITE(color)) board->enPassantSquare = endSquare - 8;
            else board->enPassantSquare = endSquare + 8;
        }
    }
    else if(difference == 7 || difference == 9)
    {
        //Diagonal Capture

        //Check for en passant.
        if(endSquare == board->enPassantSquare)
        {
            int pinType = 0;
            //Check if target pawn is pinned to turn's king.
            if(ISWHITE(color) && (pinType = isPinned(board, board->enPassantSquare - 8, board->kingSquare_w, WHITE)) && pinType != PIN_TYPE_ABOVE)
            {
                char kingSquareName[3] = {0};
                getSquareName(board->kingSquare_w, kingSquareName);
                char pawnSquareName[3] = {0};
                getSquareName(board->moveStackTop->endSquare, pawnSquareName);
                DEBUG("Cannot capture en-passant. Other pawn on %s (%d) is pinned to white king on %s (%d)", pawnSquareName, board->enPassantSquare - 8, kingSquareName, board->kingSquare_w);
                return -1;
            }
            else if(ISBLACK(color) && (pinType =  isPinned(board, board->enPassantSquare + 8, board->kingSquare_b, BLACK)) && pinType != PIN_TYPE_BELOW)
            {
                char kingSquareName[3] = {0};
                getSquareName(board->kingSquare_b, kingSquareName);
                char pawnSquareName[3] = {0};
                getSquareName(board->moveStackTop->nextMove->endSquare, pawnSquareName);
                DEBUG("Cannot capture en-passant. Other pawn on %s (%d) is pinned to black king on %s (%d)", pawnSquareName, board->enPassantSquare + 8, kingSquareName, board->kingSquare_b);
                return -1;
            }

            //Check if capturing other pawn reveals a discovered check.
            if(ISWHITE(color) && isPinned(board, board->enPassantSquare - 8, board->kingSquare_b, BLACK))
            {
                CHECK_B(board->flags);
            }
            else if(ISBLACK(color) && isPinned(board, board->enPassantSquare + 8, board->kingSquare_w, WHITE))
            {
                CHECK_W(board->flags);
            }

            if(ISWHITE(color)) board_clear_square(board, board->enPassantSquare - 8, PAWN|BLACK);
            else board_clear_square(board, board->enPassantSquare + 8, PAWN|WHITE);
        }

        //Set new board positions.
        board_clear_square(board, startSquare, (color|PAWN));

        if((ISWHITE(color) && endSquare > 55) || (ISBLACK(color) && endSquare < 8))
        {
            //Promotion
            board_set(board, endSquare, (color|promoteTo));
        }
        else
        {
            board_set(board, endSquare, (color|PAWN));
        }      
    }

    //Calculate direct check attacks.
    if(ISWHITE(color) && board->king_b&(pawnMoves(board, endSquare, WHITE)))
    {
        CHECK_B(board->flags);
    }
    else if(ISBLACK(color) && board->king_w&(pawnMoves(board, endSquare, BLACK)))
    {
        CHECK_W(board->flags);
    }

    return 0;
}

int moveKnight(bitboard *board, int startSquare, int endSquare, int color)
{
    if(board == NULL)
    {
        DEBUG("Cannot move knight. Board is NULL");
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Knight start square %d - out of bounds [0, 63]", startSquare);
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Knight end square %d - out of bounds [0, 63]", endSquare);
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for knight move.");
        return -1;
    }

    //Set new board positions after ensuring position is valid.
    board_clear_square(board, startSquare, (color|KNIGHT));
    board_set(board, endSquare, (color|KNIGHT));

    //Calculate direct check attacks.
    if(ISWHITE(color) && board->king_b&(knightMoves(board, endSquare, WHITE)))
    {
        CHECK_B(board->flags);
    }
    else if(ISBLACK(color) && board->king_w&(knightMoves(board, endSquare, BLACK)))
    {
        CHECK_W(board->flags);
    }

    return 0;
}

int moveBishop(bitboard *board, int startSquare, int endSquare, int color)
{
    if(board == NULL)
    {
        DEBUG("Cannot move bishop. Board is NULL");
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Bishop start square %d - out of bounds [0, 63]", startSquare);
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Bishop end square %d - out of bounds [0, 63]", endSquare);
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for bishop move.");
        return -1;
    }

    board_clear_square(board, startSquare, (color|BISHOP));
    board_set(board, endSquare, (color|BISHOP));

    //Calculate direct check attacks.
    if(ISWHITE(color) && board->king_b&(bishopMoves(board, endSquare, WHITE)))
    {
        CHECK_B(board->flags);
    }
    else if(ISBLACK(color) && board->king_w&(bishopMoves(board, endSquare, BLACK)))
    {
        CHECK_W(board->flags);
    }

    return 0;
}

int moveRook(bitboard *board, int startSquare, int endSquare, int color)
{
    if(board == NULL)
    {
        DEBUG("Cannot move rook. Board is NULL");
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Rook start square %d - out of bounds [0, 63]", startSquare);
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Rook end square %d - out of bounds [0, 63]", endSquare);
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for rook move.");
        return -1;
    }

    board_clear_square(board, startSquare, (color|ROOK));
    board_set(board, endSquare, (color|ROOK));

    if(ISWHITE(color))
    {
        if(startSquare == 0 && QUEENSIDE_CASTLE_WHITE(board->flags)) 
        {
            BAN_QUEENCASTLE_W(board->flags);
        }
        else if(startSquare == 7 && KINGSIDE_CASTLE_WHITE(board->flags)) 
        {
            BAN_KINGCASTLE_W(board->flags);
        }

        if(board->king_b&(rookMoves(board, endSquare, WHITE))) CHECK_B(board->flags);
    }
    else if(ISBLACK(color))
    {
        if(startSquare == 56 && QUEENSIDE_CASTLE_BLACK(board->flags)) 
        {
            BAN_QUEENCASTLE_B(board->flags);
        }
        else if(startSquare == 63 && KINGSIDE_CASTLE_BLACK(board->flags)) 
        {
            BAN_KINGCASTLE_B(board->flags);
        }

        if(board->king_w&(rookMoves(board, endSquare, BLACK))) CHECK_W(board->flags);
    }
    
    return 0;
}

int moveQueen(bitboard *board, int startSquare, int endSquare, int color)
{
    if(board == NULL)
    {
        DEBUG("Cannot move queen. Board is NULL");
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Queen start square %d - out of bounds [0, 63]", startSquare);
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Queen end square %d - out of bounds [0, 63]", endSquare);
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for queen move.");
        return -1;
    }

    board_clear_square(board, startSquare, (color|QUEEN));
    board_set(board, endSquare, (color|QUEEN));

    //Calculate direct check attacks.
    if(ISWHITE(color) && board->king_b&(queenMoves(board, endSquare, WHITE)))
    {
        CHECK_B(board->flags);
    }
    else if(ISBLACK(color) && board->king_w&(queenMoves(board, endSquare, BLACK)))
    {
        CHECK_W(board->flags);
    }

    return 0;
}

int moveKing(bitboard *board, int startSquare, int endSquare, int color)
{
    if(board == NULL)
    {
        DEBUG("Cannot move king. Board is NULL");
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("King start square %d - out of bounds [0, 63]", startSquare);
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("King end square %d - out of bounds [0, 63]", endSquare);
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for king move.");
        return -1;
    }

    if(ISBLACK(color)) 
    {
        board->kingSquare_b = endSquare;

        if(QUEENSIDE_CASTLE_BLACK(board->flags) || KINGSIDE_CASTLE_BLACK(board->flags))
        {
            BAN_QUEENCASTLE_B(board->flags);
            BAN_KINGCASTLE_B(board->flags);
        }
    }
    else
    {
        board->kingSquare_w = endSquare;

        if(QUEENSIDE_CASTLE_WHITE(board->flags) || KINGSIDE_CASTLE_WHITE(board->flags))
        {
            BAN_QUEENCASTLE_W(board->flags);
            BAN_KINGCASTLE_W(board->flags);
        }
    }

    board_clear_square(board, startSquare, (color|KING));
    board_set(board, endSquare, (color|KING));

    //Castling
    if(startSquare - endSquare == 2)
    {
        board_clear_square(board, startSquare - 4, (color|ROOK));
        board_set(board, endSquare + 1, (color|ROOK));
    }
    else if(startSquare - endSquare == -2)
    {
        board_clear_square(board, startSquare + 3, (color|ROOK));
        board_set(board, endSquare - 1, (color|ROOK));
    }

    return 0;
}

int movePiece(bitboard *board, int startSquare, int endSquare, int piece, int promoteTo)
{
    int error = 0;
    if(ISPAWN(piece))
    {
        error = movePawn(board, startSquare, endSquare, piece&0xF0, promoteTo);
    }
    else if(ISKNIGHT(piece))
    {
        error = moveKnight(board, startSquare, endSquare, piece&0xF0);
    }
    else if(ISBISHOP(piece))
    {
        error = moveBishop(board, startSquare, endSquare, piece&0xF0);
    }
    else if(ISROOK(piece))
    {
        error = moveRook(board, startSquare, endSquare, piece&0xF0);
    }
    else if(ISQUEEN(piece))
    {
        error = moveQueen(board, startSquare, endSquare, piece&0xF0);
    }
    else if(ISKING(piece))
    {
        error = moveKing(board, startSquare, endSquare, piece&0xF0);
    }
    else
    {
        DEBUG("Attempted to move invalid piece type.");
        return -1;
    }

    return error;
}

int moveFromString(bitboard* board, char* str)
{
    //String format: [2 char - startsquare][2 char - endsquare][1 char - promotion (q, n, r, b)]
    char start[3] = {'\0'};
    strncpy(start, str, 2);
    char end[3] = {'\0'};
    strncpy(end, str + 2, 2);
    char promotion = str[4];

    int startSquare = getSquareNumber(start);
    int endSquare = getSquareNumber(end);
    int piece = findPieceOnSquare(board, startSquare);
    if(!piece)
    {
        DEBUG("Could not find piece on start square.");
        return -1;
    }

    int promoteTo = 0;
    switch(promotion)
    {
        case 'q':
            promoteTo = QUEEN;
            break;
        case 'r':
            promoteTo = ROOK;
            break;
        case 'n':
            promoteTo = KNIGHT;
            break;
        case 'b':
            promoteTo = BISHOP;
            break;        
        default:
            break;
    }

    int capturedPiece;
    int capturedSquare;
    if(endSquare == board->enPassantSquare)
    {
        //en passant
        if(ISWHITE(board->turn))
        {
            capturedPiece = PAWN|BLACK;
            capturedSquare = board->enPassantSquare - 8;
        }
        else
        {
            capturedPiece = PAWN|WHITE;
            capturedSquare = board->enPassantSquare + 8;
        }
    }
    else
    {
        capturedPiece = findPieceOnSquare(board, endSquare);
        capturedSquare = endSquare;
    }

    return moveFromStruct(board, createMove(startSquare, endSquare, promoteTo, piece, capturedPiece, capturedSquare, board));
}

int moveFromStruct(bitboard* board, move* m)
{   
    if(!m) return -1;
    
    if(board->victor)
    {
        DEBUG("Cannot move from terminal gamestate; Victor=%02x", board->victor);
        return -1;
    }
    else if(ISBLACK(m->piece) && board->turn == WHITE)
    {
        DEBUG("Attempted to move black piece on white's turn. (%d->%d)", m->startSquare, m->endSquare);
        return -1;
    }
    else if(ISWHITE(m->piece) && board->turn == BLACK)
    {
        DEBUG("Attempted to move white piece on black's turn. (%d->%d)", m->startSquare, m->endSquare);
        return -1;
    }
    else if(m->startSquare < 0 || m->startSquare > 63 || m->endSquare < 0 || m->endSquare > 63)
    {
        DEBUG("Piece cannot move out of bounds (%d -> %d).", m->startSquare, m->endSquare);
        return -1;
    }
    else if(m->startSquare == m->endSquare)
    {
        DEBUG("Piece cannot move in place on square %d.", m->startSquare);
        return -1;
    }

    move** potentialMoveList = generatePieceMoves(board, m->piece, m->startSquare, m->piece&0xF0, 0);
    if(potentialMoveList == NULL)
    {
        DEBUG("Failed to generate potential moves.");
        return -1;
    }
    int moveIndex = 0;
    int isLegal = 0;
    while(potentialMoveList[moveIndex])
    { 
        if(!isLegal && potentialMoveList[moveIndex]->startSquare == m->startSquare && potentialMoveList[moveIndex]->endSquare == m->endSquare)
        {
            isLegal = 1;
            break;
        }
        moveIndex++;
    }
    freeMoveList(potentialMoveList);
    if(!isLegal)
    {
        char startSquareName[3] = {'\0'};
        char endSquareName[3] = {'\0'};
        getSquareName(m->startSquare, startSquareName);
        getSquareName(m->endSquare, endSquareName);
        DEBUG("Piece move is not legal. Legal moves=%d, [%02x], %s->%s", moveIndex, m->piece, startSquareName, endSquareName);
        return -1;
    }


    if(movePiece(board, m->startSquare, m->endSquare, m->piece, m->promoteTo) != 0) 
    {
        DEBUG("Failed to move piece from struct.");
        return -1;
    }
    moves_push(board, m);
    
    if(!(ISPAWN(m->piece) && abs(m->startSquare - m->endSquare) == 16)) board->enPassantSquare = -1;

    //50 move rule counting
    if(ISPAWN(m->piece) || m->capturedPiece) board->movesSinceLastChange = 0;
    else board->movesSinceLastChange++;
    board->halfMoveCount++;


    //Calculate discovered checks and change turn.
    if(board->turn == WHITE)
    {
        if(INCHECK_W(board->flags)) UNCHECK_W(board->flags);
        if(isPinned(board, m->startSquare, board->kingSquare_b, BLACK)) CHECK_B(board->flags);
        board->turn=BLACK;
    }
    else
    {
        if(INCHECK_B(board->flags)) UNCHECK_B(board->flags);
        if(isPinned(board, m->startSquare, board->kingSquare_w, WHITE)) CHECK_W(board->flags);
        board->turn=WHITE;
    }

    //3-fold repetition check
    if(increment_table_value(board->ht, board) >= 3) board->victor = DRAW|THREEFOLD;
    else if(board->movesSinceLastChange >= 100) board->victor = DRAW|FIFTYMOVERULE; //Variable stores half-moves
    else if((board->king_b|board->king_w) == board->pieces_all) board->victor = DRAW|INSUFFICIENT_MATERIAL; //King v King drawn INSUFFICIENT_MATERIAL
    else if(!(potentialMoveList = generateMoveList(board, 0)))
    {
        //No potential moves - Calculate checkmate/stalemate
        if(board->turn == WHITE)
        {
            if(INCHECK_W(board->flags)) board->victor = BLACK;
            else board->victor = DRAW|STALEMATED_WHITE;
        }
        else
        {
            if(INCHECK_B(board->flags)) board->victor = WHITE;
            else board->victor = DRAW|STALEMATED_BLACK;
        }
    }
    else
    {
        //Freeing the movelist allocation from the previous conditional.
        freeMoveList(potentialMoveList);

        /* Other Drawn INSUFFICIENT_MATERIALs */

        //King + Minor Piece vs King
        if(board->pieces_b == board->king_b && board->pawn_w == 0 && board->rook_w == 0 && board->queen_w == 0)
        {
            uint64_t mask = 1;
            int count = 0;
            for(int i = 0; i < 63; i++)
            {
                if((board->bishop_w|board->knight_w)&mask) count++;

                mask = mask<<1;
            }
            if(count == 1) board->victor = DRAW|INSUFFICIENT_MATERIAL;
        }
        else if(board->pieces_w == board->king_w && board->pawn_b == 0 && board->rook_b == 0 && board->queen_b == 0)
        {
            uint64_t mask = 1;
            int count = 0;
            for(int i = 0; i < 63; i++)
            {
                if((board->bishop_b|board->knight_b)&mask) count++;

                mask = mask<<1;
            }
            if(count == 1) board->victor = DRAW|INSUFFICIENT_MATERIAL;
        }
        //King + Bishops vs King + Bishops (Same color bishops)
        else if(board->pieces_all == (board->king_w|board->bishop_w|board->king_b|board->bishop_b) && 
                ((board->bishop_b|board->bishop_w) == ((board->bishop_b|board->bishop_w)&LIGHT_SQUARES) ||
                 (board->bishop_b|board->bishop_w) == ((board->bishop_b|board->bishop_w)&DARK_SQUARES))) 
        {
            board->victor = DRAW|INSUFFICIENT_MATERIAL;
        }
    }
    
    return 0;
}

move* unmove(bitboard *board)
{
    if(!board)
    {
        DEBUG("Cannot undo a move from a NULL board.");
        return NULL;
    }

    decrement_table_value(board->ht, board);
    move* m = moves_pop(board);
    if(!m)
    {
        DEBUG("No move history to undo.");
        return NULL;
    }

    if(ISKING(m->piece) && abs(m->endSquare-m->startSquare) == 2)
    {
        //Undo castle.
        if(m->endSquare == 2)
        {
            //White queenside castle
            board_clear_square(board, 2, KING|WHITE);
            board_clear_square(board, 3, ROOK|WHITE);
            board_set(board, 0, ROOK|WHITE);
            board_set(board, 4, KING|WHITE);
            board->kingSquare_w = 4;
        }
        else if(m->endSquare == 6)
        {
            //White kingside castle
            board_clear_square(board, 6, KING|WHITE);
            board_clear_square(board, 5, ROOK|WHITE);
            board_set(board, 7, ROOK|WHITE);
            board_set(board, 4, KING|WHITE);
            board->kingSquare_w = 4;
        }
        else if(m->endSquare == 58)
        {
            //Black queenside castle
            board_clear_square(board, 58, KING|BLACK);
            board_clear_square(board, 59, ROOK|BLACK);
            board_set(board, 56, ROOK|BLACK);
            board_set(board, 60, KING|BLACK);
            board->kingSquare_b = 60;
        }
        else if(m->endSquare == 62)
        {
            //Black kingside castle
            board_clear_square(board, 62, KING|BLACK);
            board_clear_square(board, 61, ROOK|BLACK);
            board_set(board, 63, ROOK|BLACK);
            board_set(board, 60, KING|BLACK);
            board->kingSquare_b = 60;
        }
    }
    else
    {
        if(m->promoteTo) board_clear_square(board, m->endSquare, m->promoteTo);
        board_clear_square(board, m->endSquare, m->piece);
        board_set(board, m->startSquare, m->piece);

        if(m->capturedPiece) board_set(board, m->capturedPieceSquare, m->capturedPiece);

        if(ISKING(m->piece))
        {
            if(ISWHITE(m->piece)) board->kingSquare_w = m->startSquare;
            else board->kingSquare_b = m->startSquare;
        }
    }
    
    board->flags = m->flags;
    board->movesSinceLastChange = m->previousMovesSinceLastChange;
    board->enPassantSquare = m->prevEnPassantSquare;

    if(board->turn == WHITE) board->turn = BLACK;
    else if (board->turn==BLACK) board->turn = WHITE;
    
    board->victor = 0;
    board->halfMoveCount--;


    return m;
}