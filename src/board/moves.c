#include "board/bitboard.h"
#include "board/moves.h" 
#include "debug.h"
#include <string.h>

int generateMoveList(move_c* movesList, bitboard* board, int capturesOnly)
{
    int size = 0;
    uint64_t allies, enemies, knights, bishop, rook, queen, king;
    if(ISWHITE(board->turn))
    {
        allies = board->pieces_side[WHITE];
        enemies = board->pieces_side[BLACK];
        knights = board->pieces[WHITE_KNIGHT];
        bishop = board->pieces[WHITE_BISHOP];
        rook = board->pieces[WHITE_ROOK];
        queen = board->pieces[WHITE_QUEEN];
        king = board->pieces[WHITE_KING];
    }
    else
    {
        allies = board->pieces_side[BLACK];
        enemies = board->pieces_side[WHITE];
        knights = board->pieces[BLACK_KNIGHT];
        bishop = board->pieces[BLACK_BISHOP];
        rook = board->pieces[BLACK_ROOK];
        queen = board->pieces[BLACK_QUEEN];
        king = board->pieces[BLACK_KING];
    }

    uint64_t mask = 0;
    int piece = PAWN|(board->turn);
    if(ISWHITE(piece))
    {
        if(!capturesOnly)
        {
            mask = WHITE_PAWN_DOUBLEPUSH_MASK(board);
            while(mask)
            {
                int endSquare = __builtin_ctzll(mask);
                int startSquare = endSquare - 16;

                createCompactMove(&movesList[size++], startSquare, endSquare, 0);
                
                mask&=(mask - 1);
            }
        }

        //Treat promotions as captures for quiescent search
        mask = WHITE_PAWN_PUSH_MASK(board);
        if(capturesOnly == GET_CAPTURES_AND_PROMOTIONS) mask &= 0xFF00000000000000;
        else if(capturesOnly >= GET_CAPTURES) mask = 0;
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare - 8;

            if(endSquare >= 56)
            {
                //Promotion
                createCompactMove(&movesList[size++], startSquare, endSquare, KNIGHT);
                createCompactMove(&movesList[size++], startSquare, endSquare, BISHOP); 
                createCompactMove(&movesList[size++], startSquare, endSquare, ROOK);     
                createCompactMove(&movesList[size++], startSquare, endSquare, QUEEN);
            }
            else createCompactMove(&movesList[size++], startSquare, endSquare, 0);

            mask&=(mask - 1);
        }

        

        mask = WHITE_PAWN_LEFTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare - 7;

            if(endSquare >= 56)
            {
                //Promotion
                createCompactMove(&movesList[size++], startSquare, endSquare, KNIGHT);
                createCompactMove(&movesList[size++], startSquare, endSquare, BISHOP);
                createCompactMove(&movesList[size++], startSquare, endSquare, ROOK);
                createCompactMove(&movesList[size++], startSquare, endSquare, QUEEN);
            }
            else createCompactMove(&movesList[size++], startSquare, endSquare, 0);
            
            mask&=(mask - 1);
        }

        mask = WHITE_PAWN_RIGHTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare - 9;

            if(endSquare >= 56)
            {
                //Promotion
                createCompactMove(&movesList[size++], startSquare, endSquare, KNIGHT);
                createCompactMove(&movesList[size++], startSquare, endSquare, BISHOP);
                createCompactMove(&movesList[size++], startSquare, endSquare, ROOK);
                createCompactMove(&movesList[size++], startSquare, endSquare, QUEEN);
            }
            else createCompactMove(&movesList[size++], startSquare, endSquare, 0);
            
            mask&=(mask - 1);
        }

        
        if(board->enPassantSquare != NO_EP_SQUARE)
        {
            mask = EN_PASSANT_ATTACKERS_WHITE(singleBitMask(board->enPassantSquare), board);
            while(mask)
            {
                int startSquare = __builtin_ctzll(mask);
                createCompactMove(&movesList[size++], startSquare, board->enPassantSquare, 0);
                mask&=(mask - 1);
            }
        }

    }
    else
    {
        if(!capturesOnly)
        {
            mask = BLACK_PAWN_DOUBLEPUSH_MASK(board);
            while(mask)
            {
                int endSquare = __builtin_ctzll(mask);
                int startSquare = endSquare + 16;

                createCompactMove(&movesList[size++], startSquare, endSquare, 0);
                
                mask&=(mask - 1);
            }
        }

        mask = BLACK_PAWN_PUSH_MASK(board);
        if(capturesOnly == GET_CAPTURES_AND_PROMOTIONS) mask &= 0xFF;
        else if(capturesOnly >= GET_CAPTURES) mask = 0;
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare + 8;

            if(endSquare <= 7)
            {
                //Promotion
                createCompactMove(&movesList[size++], startSquare, endSquare, KNIGHT);
                createCompactMove(&movesList[size++], startSquare, endSquare, BISHOP); 
                createCompactMove(&movesList[size++], startSquare, endSquare, ROOK);     
                createCompactMove(&movesList[size++], startSquare, endSquare, QUEEN);
            }
            else createCompactMove(&movesList[size++], startSquare, endSquare, 0);

            mask&=(mask - 1);
        }
        

        mask = BLACK_PAWN_LEFTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare + 7;

            if(endSquare <= 7)
            {
                //Promotion
                createCompactMove(&movesList[size++], startSquare, endSquare, KNIGHT);
                createCompactMove(&movesList[size++], startSquare, endSquare, BISHOP);
                createCompactMove(&movesList[size++], startSquare, endSquare, ROOK);
                createCompactMove(&movesList[size++], startSquare, endSquare, QUEEN);
            }
            else
            {
                createCompactMove(&movesList[size++], startSquare, endSquare, 0);
            }
            
            mask&=(mask - 1);
        }

        mask = BLACK_PAWN_RIGHTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare + 9;

            if(endSquare <= 7)
            {
                //Promotion
                createCompactMove(&movesList[size++], startSquare, endSquare, KNIGHT);
                createCompactMove(&movesList[size++], startSquare, endSquare, BISHOP);
                createCompactMove(&movesList[size++], startSquare, endSquare, ROOK);
                createCompactMove(&movesList[size++], startSquare, endSquare, QUEEN);
            }
            else
            {
                createCompactMove(&movesList[size++], startSquare, endSquare, 0);
            }
            
            mask&=(mask - 1);
        }

        
        if(board->enPassantSquare != NO_EP_SQUARE)
        {
            uint64_t epMask = singleBitMask(board->enPassantSquare);
            mask = EN_PASSANT_ATTACKERS_BLACK(epMask, board);
            while(mask)
            {
                int startSquare = __builtin_ctzll(mask);
                createCompactMove(&movesList[size++], startSquare, board->enPassantSquare, 0);
                mask&=(mask - 1);
            }
        }
    }

    while(knights) 
    {
        int startSquare = __builtin_ctzll(knights);
        uint64_t mask = knightMoves(allies, startSquare);
        if(capturesOnly) mask &= enemies;
        
        while(mask) 
        {
            int endSquare = __builtin_ctzll(mask);
            createCompactMove(&movesList[size++], startSquare, endSquare, 0);
            mask&=(mask-1);
        }
        knights&=(knights-1);
    }

    while(bishop) 
    {
        int startSquare = __builtin_ctzll(bishop);
        uint64_t moveMask = bishopMoves(allies, enemies, startSquare);
        if(capturesOnly) moveMask &= enemies;

        while(moveMask) 
        {
            int endSquare = __builtin_ctzll(moveMask);
            createCompactMove(&movesList[size++], startSquare, endSquare, 0);
            moveMask&=(moveMask-1);
        }
        bishop&=(bishop-1);
    }

    while(rook) 
    {
        int startSquare = __builtin_ctzll(rook);
        uint64_t moveMask = rookMoves(allies, enemies, startSquare); 
        if(capturesOnly) moveMask &= enemies;

        while(moveMask) 
        {
            int endSquare = __builtin_ctzll(moveMask);
            createCompactMove(&movesList[size++], startSquare, endSquare, 0);
            moveMask&=(moveMask-1);
        }
        rook&=(rook-1);
    }
    
    while(queen) 
    {
        int startSquare = __builtin_ctzll(queen);
        uint64_t moveMask = queenMoves(allies, enemies, startSquare); 
        if(capturesOnly) moveMask &= enemies;

        while(moveMask) 
        {
            int endSquare = __builtin_ctzll(moveMask);
            createCompactMove(&movesList[size++], startSquare, endSquare, 0);
            moveMask&=(moveMask-1);
        }
        queen&=(queen-1);
    }

    if(king)
    {
        int startSquare = __builtin_ctzll(king);
        uint64_t moveMask = kingMoves(board, startSquare, board->turn);
        if(capturesOnly) moveMask &= enemies;
        while(moveMask) 
        {
            int endSquare = __builtin_ctzll(moveMask);
            createCompactMove(&movesList[size++], startSquare, endSquare, 0);
            moveMask&=(moveMask-1);
        }
    }
    
    return size;
}
uint64_t getSlidingAttackers(bitboard* board, int square, int occupied)
{
    //Sliding pieces
    uint64_t bishopqueen = board->pieces[WHITE_BISHOP] | board->pieces[BLACK_BISHOP] | board->pieces[WHITE_QUEEN] | board->pieces[BLACK_QUEEN];
    uint64_t rookqueen = board->pieces[WHITE_ROOK] | board->pieces[BLACK_ROOK] | board->pieces[WHITE_QUEEN] | board->pieces[BLACK_QUEEN];

    return (bishopMoves(0, occupied, square) & bishopqueen) |
           (rookMoves(0, occupied, square) & rookqueen);
}
uint64_t getAttackers(bitboard* board, int square, int occupied)
{
    uint64_t attackers = 0;

    //Pawns.
    attackers |= (pawnAttacks[WHITE][square] & board->pieces[BLACK_PAWN]);
    attackers |= (pawnAttacks[BLACK][square] & board->pieces[WHITE_PAWN]);

    //Knights
    attackers |= (knightMoves(0, square) & (board->pieces[WHITE_KNIGHT] | board->pieces[BLACK_KNIGHT]));

    //Kings
    attackers |= pyrrhicKingAttacks(square) & (board->pieces[WHITE_KING] | board->pieces[BLACK_KING]);

    return attackers | getSlidingAttackers(board, square, occupied);
}

int findLVA(bitboard* board, uint64_t attackers, int side, int* pieceType)
{
    assert(pieceType);
    for(int pc = side; pc <= BLACK_KING; pc+=2)
    {
        uint64_t attackingPiecesOfType = attackers & board->pieces[pc];
        if(attackingPiecesOfType)
        {
            *pieceType = pc;
            return __builtin_ctzll(attackingPiecesOfType);
        }
    }
    return -1;
}

static const int pieceValuesSEE[15] = {100, 100, 300, 300, 325, 325, 500, 500, 900, 900, 1e6, 1e6, 0, 0, 0};
int staticExchangeEvaluation(bitboard* board, move_d m)
{
    int gain[MAX_PLY];
    int ply = 0;

    int side = COLOR(m.piece);

    //Handle the initial capture
    gain[0] = pieceValuesSEE[m.capturedPiece];

    uint64_t removedMask = singleBitMask(m.startSquare);

    uint64_t occupied = board->pieces_all & ~removedMask;
    uint64_t attackers = getAttackers(board, m.endSquare, occupied) & ~removedMask;

    side = FLIP_COLOR(side);

    int attackerSquare;
    int attackerPiece;

    if(ISPAWN(m.piece) || ISBISHOP(m.piece) || ISROOK(m.piece) || ISQUEEN(m.piece))
    {
        attackers |= getSlidingAttackers(board, m.endSquare, occupied);
    }

    while(ply < MAX_PLY - 1)
    {
        attackerSquare = findLVA(board, attackers, side, &attackerPiece);
        if(attackerSquare == -1) break;

        ply++;
        gain[ply] = pieceValuesSEE[attackerPiece] - gain[ply - 1];

        if(gain[ply] < 0 && -gain[ply] > pieceValuesSEE[attackerPiece])
        {
            ply--;
            break;
        }

        removedMask |= singleBitMask(attackerSquare);
        occupied &= ~removedMask;
        if(ISPAWN(attackerPiece) || ISBISHOP(attackerPiece) || ISROOK(attackerPiece) || ISQUEEN(attackerPiece)) 
        {
            attackers |= getSlidingAttackers(board, m.endSquare, occupied);
        }
        
        attackers &= ~removedMask;

        side = FLIP_COLOR(side);
    }

    while(ply > 0)
    {
        if(gain[ply] > 0) gain[ply - 1] -= gain[ply];
        ply--;
    }
    return gain[0];
}

//Only used when move ordering matters.
moveIterator* create_move_iterator(bitboard* board, int capturesOnly, move_c* pvMove, move_c* ttMove, move_c* requiredMoves, move_c* killerMoves, int16_t history[2][6][64])
{
    moveIterator* iter = malloc(sizeof(moveIterator));
    iter->moveList = malloc(MAX_MOVES * sizeof(move_c));
    iter->moveScores = malloc(MAX_MOVES * sizeof(int16_t));
    iter->count = 0;
    iter->visitedCount = 0;

    if(requiredMoves && IS_VALID_MOVE(requiredMoves[0]))
    {
        for(int i = 0; i < MAX_REQUIRED_MOVES; i++)
        {
            if(IS_VALID_MOVE(requiredMoves[i])) 
            {
                iter->count++;
                iter->moveList[i] = requiredMoves[i];
            }
            else break;
        }
    }
    else iter->count = generateMoveList(iter->moveList, board, capturesOnly);

    if(!iter->count) { destroy_move_iterator(iter); return NULL; }
    
    for(int i = 0; i < iter->count; i++)
    {
        move_d m;
        createDetailedMove(&m, iter->moveList[i], board);

        if(pvMove && m.arr[0] == pvMove->raw)
            iter->moveScores[i] = PV_MOVE_SCORE;
        else if(ttMove && m.arr[0] == ttMove->raw)
            iter->moveScores[i] = TT_MOVE_SCORE;
        else if(killerMoves && m.arr[0] == killerMoves[0].raw)
            iter->moveScores[i] = KILLER_1_SCORE;
        else if(killerMoves && m.arr[0] == killerMoves[1].raw)
            iter->moveScores[i] = KILLER_2_SCORE;
        else if(m.capturedPiece != EMPTY_PIECE)
        {
            int seeValue = staticExchangeEvaluation(board, m);

            if(seeValue >= 0) iter->moveScores[i] = CAPTURE_SCORE + seeValue;
            else if(capturesOnly == GET_WINNING_CAPTURES) iter->moveScores[i] = INT16_MIN;
            else iter->moveScores[i] = -CAPTURE_SCORE + seeValue;
        }
        else if(m.promoteTo)
            iter->moveScores[i] = CAPTURE_SCORE;
        else if(history)
            iter->moveScores[i] = history[board->turn][PIECE(m.piece) / 2][m.endSquare];
        else iter->moveScores[i] = (ISKING(m.piece)) ? -1 : m.piece;

    }

    return iter;
}

move_c* iterate_next_move(moveIterator* iter)
{
    if(iter->visitedCount >= iter->count) return NULL;

    int bestIndex = -1;
    int maxScoreRemaining = INT16_MIN;

    for(int j = 0; j < iter->count; j++) 
    {
        if(iter->moveScores[j] > maxScoreRemaining) 
        {
            bestIndex = j;
            maxScoreRemaining = iter->moveScores[j];
        }
    }

    if(bestIndex == -1) return NULL;

    iter->moveScores[bestIndex] = INT16_MIN;
    iter->visitedCount++;
    
    return &iter->moveList[bestIndex];
}

void destroy_move_iterator(moveIterator* iter)
{
    if(iter)
    {
        if(iter->moveList) free(iter->moveList);
        if(iter->moveScores) free(iter->moveScores);
        free(iter);
    }
}


int isThreatened(bitboard* board, int square, int squareColor)
{
    uint64_t bishopqueen = 0;
    uint64_t rookqueen = 0;
    uint64_t enemyPieces = 0;
    uint64_t allyPieces = 0;
    if(ISWHITE(squareColor))
    {
        enemyPieces = board->pieces_side[BLACK];
        allyPieces = board->pieces_side[WHITE];
        bishopqueen = board->pieces[BLACK_BISHOP]|board->pieces[BLACK_QUEEN];
        rookqueen = board->pieces[BLACK_ROOK]|board->pieces[BLACK_QUEEN];
        if(square < 48 && pawnAttacks[0][square]&board->pieces[BLACK_PAWN]) return THREAT_TYPE_PAWN;
        if(knightAttacks[square]&(board->pieces[BLACK_KNIGHT])) return THREAT_TYPE_KNIGHT;
        if(kingAttacks[square]&(board->pieces[BLACK_KING])) return THREAT_TYPE_KING;
    }
    else if(ISBLACK(squareColor))
    {
        enemyPieces = board->pieces_side[WHITE];
        allyPieces = board->pieces_side[BLACK];
        bishopqueen = board->pieces[WHITE_BISHOP]|board->pieces[WHITE_QUEEN];
        rookqueen = board->pieces[WHITE_ROOK]|board->pieces[WHITE_QUEEN];
        if(square > 15 && pawnAttacks[1][square]&board->pieces[WHITE_PAWN]) return THREAT_TYPE_PAWN;
        if(knightAttacks[square]&(board->pieces[WHITE_KNIGHT])) return THREAT_TYPE_KNIGHT;
        if(kingAttacks[square]&(board->pieces[WHITE_KING])) return THREAT_TYPE_KING;
    }
    if(bishopMoves(allyPieces, enemyPieces, square)&(bishopqueen)) return THREAT_TYPE_BISHOPQUEEN;
    if(rookMoves(allyPieces, enemyPieces, square)&(rookqueen)) return THREAT_TYPE_ROOKQUEEN;

    return THREAT_TYPE_NONE;
}

uint64_t pyrrhicPawnAttacks(int square, int color)
{
    color ^= 1;
    return pawnAttacks[color][square];
}

uint64_t knightMoves(uint64_t allyPieces, int square)
{
    return knightAttacks[square]&(~allyPieces);
}

uint64_t bishopMoves(uint64_t allyPieces, uint64_t enemyPieces, int square)
{
    uint64_t occupancy = (allyPieces|enemyPieces) & bishopMagics[square].mask;
    return bishopMagics[square].attacks[(occupancy * bishopMagics[square].magic) >> bishopMagics[square].shiftOffset] & (~allyPieces);
}

uint64_t rookMoves(uint64_t allyPieces, uint64_t enemyPieces, int square)
{
    uint64_t occupancy = (allyPieces|enemyPieces) & rookMagics[square].mask;
    return rookMagics[square].attacks[(occupancy * rookMagics[square].magic) >> rookMagics[square].shiftOffset] & (~allyPieces);
}

uint64_t queenMoves(uint64_t allyPieces, uint64_t enemyPieces, int square)
{
    return rookMoves(allyPieces, enemyPieces, square)|bishopMoves(allyPieces, enemyPieces, square);
}

uint64_t pyrrhicKingAttacks(int square)
{
    return kingAttacks[square];
}

uint64_t kingMoves(bitboard* board, int square, int color)
{
    uint64_t returnedValue = kingAttacks[square];

    if(ISWHITE(color))
    {
        returnedValue&=(~board->pieces_side[WHITE]);

        //uint64_t betweenMask = 0x60; //Squares 5 and 6 between king on 4 and rook on 7

        if(KINGSIDE_CASTLE_WHITE(board->flags) && !(board->flags&16) && !(board->pieces_all&0x60) && 
                                                        !isThreatened(board, square, color) && 
                                                        !isThreatened(board, square+1, color) && 
                                                        !isThreatened(board, square+2, color)) returnedValue|=0x40;

        //betweenMask = 0xE; //Square 1 and 2 and 3 between rook on 0 and king on 4

        if(QUEENSIDE_CASTLE_WHITE(board->flags) && !(board->flags&16) && !(board->pieces_all&0xE) && 
                                                        !isThreatened(board, square, color) && 
                                                        !isThreatened(board, square-1, color) && 
                                                        !isThreatened(board, square-2, color)) returnedValue|=0x4;
        
    }
    else
    {
        returnedValue&=(~board->pieces_side[BLACK]);

        //uint64_t betweenMask = 0x6000000000000000; //Squares 61 and 62 between king on 60 and rook on 63

        //Black kingside
        if(KINGSIDE_CASTLE_BLACK(board->flags) && !(board->flags&32) && !(board->pieces_all&0x6000000000000000) && 
                                                            !isThreatened(board, square, color) &&
                                                            !isThreatened(board, square+1, color) && 
                                                            !isThreatened(board, square+2, color)) returnedValue|=0x4000000000000000;

        //betweenMask = 0x0E00000000000000; //Square 57 and 58 and 59 between rook on 56 and king on 60
        if(QUEENSIDE_CASTLE_BLACK(board->flags&8) && !(board->flags&32) && !(board->pieces_all&0x0E00000000000000) &&
                                                            !isThreatened(board, square, color) &&
                                                            !isThreatened(board, square-1, color) && 
                                                            !isThreatened(board, square-2, color)) returnedValue|=0x0400000000000000;
    }
    
    return returnedValue;
}

int movePiece(bitboard *board, move_c compactMove)
{
    assert(board);
    move_d m;
    createDetailedMove(&m, compactMove, board);
    assert(m.startSquare >= 0 && m.startSquare <= 63 && m.endSquare >= 0 && m.endSquare <= 63);

    //Clear the en passant hash early. clear the en passant square later.
    //En passant square is required for legal moves, en passant hash can change by the end of the function.
    board->hashCode ^= getEnPassantHash(board);

    switch(PIECE(m.piece))
    {
        case PAWN:
            int difference = abs(m.startSquare - m.endSquare);
            
            if(difference == 8 || difference == 16)
            {
                if((ISWHITE(m.piece) && m.endSquare > 55) || (ISBLACK(m.piece) && m.endSquare < 8))
                {
                    //Promotion
                    if(m.promoteTo == 0) m.promoteTo = QUEEN;
                    board_clear_square(board, m.startSquare);
                    board_set(board, m.endSquare, (COLOR(m.piece)|m.promoteTo));
                }
                else
                {
                    board_move_piece_quietly(board, m.startSquare, m.endSquare);
                }
                
                //Update en passant square if necessary.
                if(difference == 16)
                {
                    if(ISWHITE(m.piece)) 
                    {
                        board->enPassantSquare = m.endSquare - 8;
                        board->hashCode ^= getEnPassantHash(board);
                    }
                    else 
                    {
                        board->enPassantSquare = m.endSquare + 8;
                        board->hashCode ^= getEnPassantHash(board);
                    }
                }
            }
            else if(difference == 7 || difference == 9)
            {
                //Diagonal Capture

                //Check for en passant.
                if(m.endSquare == board->enPassantSquare)
                {
                    if(ISWHITE(m.piece)) 
                    {
                        m.capturedPiece = findPieceOnSquare(board, board->enPassantSquare - 8);
                        board_clear_square(board, board->enPassantSquare - 8);
                    }
                    else 
                    {
                        m.capturedPiece = findPieceOnSquare(board, board->enPassantSquare + 8);
                        board_clear_square(board, board->enPassantSquare + 8);
                    }
                }
                else m.capturedPiece = findPieceOnSquare(board, m.endSquare);

                if((ISWHITE(m.piece) && m.endSquare > 55) || (ISBLACK(m.piece) && m.endSquare < 8))
                {
                    //Promotion
                    if(m.promoteTo == 0) m.promoteTo = QUEEN;
                    board_clear_square(board, m.startSquare);
                    board_clear_square(board, m.endSquare);
                    board_set(board, m.endSquare, (COLOR(m.piece)|m.promoteTo));
                }
                else
                {
                    board_move_piece(board, m.startSquare, m.endSquare);
                }      
            }
            break;
        case ROOK:
            if(m.startSquare == 7 && KINGSIDE_CASTLE_WHITE(board->flags)) { BAN_KINGCASTLE_W(board->flags); board->hashCode ^= zobrist_keys[768]; }
            else if(m.startSquare == 0 && QUEENSIDE_CASTLE_WHITE(board->flags)) { BAN_QUEENCASTLE_W(board->flags); board->hashCode ^= zobrist_keys[769]; }
            else if(m.startSquare == 63 && KINGSIDE_CASTLE_BLACK(board->flags)) { BAN_KINGCASTLE_B(board->flags); board->hashCode ^= zobrist_keys[770]; }
            else if(m.startSquare == 56 && QUEENSIDE_CASTLE_BLACK(board->flags)) { BAN_QUEENCASTLE_B(board->flags); board->hashCode ^= zobrist_keys[771]; }
        case KNIGHT:
        case BISHOP:
        case QUEEN:
            m.capturedPiece = findPieceOnSquare(board, m.endSquare);
            board_move_piece(board, m.startSquare, m.endSquare);
            break;
        case KING:
            if(ISBLACK(m.piece)) 
            {
                board->kingSquare_b = m.endSquare;
                if(KINGSIDE_CASTLE_BLACK(board->flags)) { BAN_KINGCASTLE_B(board->flags); board->hashCode ^= zobrist_keys[770]; }
                if(QUEENSIDE_CASTLE_BLACK(board->flags)) { BAN_QUEENCASTLE_B(board->flags); board->hashCode ^= zobrist_keys[771]; }
            }
            else 
            {
                board->kingSquare_w = m.endSquare;
                if(KINGSIDE_CASTLE_WHITE(board->flags)) { BAN_KINGCASTLE_W(board->flags); board->hashCode ^= zobrist_keys[768]; }
                if(QUEENSIDE_CASTLE_WHITE(board->flags)) { BAN_QUEENCASTLE_W(board->flags); board->hashCode ^= zobrist_keys[769]; }
            }

            m.capturedPiece = findPieceOnSquare(board, m.endSquare);
            board_move_piece(board, m.startSquare, m.endSquare);

            //Castling
            if(m.startSquare - m.endSquare == 2) board_move_piece_quietly(board, m.startSquare - 4, m.endSquare + 1);
            else if(m.startSquare - m.endSquare == -2) board_move_piece_quietly(board, m.startSquare + 3, m.endSquare - 1);
            break;
        default:
            DEBUG_ERROR("Attempted to move invalid piece type.");
            return -1;
            break;
    }
    
    m.lastChangeIndex = board->lastChangeIndex;
    moves_push(board, m);

    if(ISROOK(m.capturedPiece))
    {
            if(m.endSquare == 7 && KINGSIDE_CASTLE_WHITE(board->flags)) { BAN_KINGCASTLE_W(board->flags); board->hashCode ^= zobrist_keys[768]; }
            else if(m.endSquare == 0 && QUEENSIDE_CASTLE_WHITE(board->flags)) { BAN_QUEENCASTLE_W(board->flags); board->hashCode ^= zobrist_keys[769]; }
            else if(m.endSquare == 63 && KINGSIDE_CASTLE_BLACK(board->flags)) { BAN_KINGCASTLE_B(board->flags); board->hashCode ^= zobrist_keys[770]; }
            else if(m.endSquare == 56 && QUEENSIDE_CASTLE_BLACK(board->flags)) { BAN_QUEENCASTLE_B(board->flags); board->hashCode ^= zobrist_keys[771]; }
    }

    if(!(ISPAWN(m.piece) && abs(m.startSquare - m.endSquare) == 16) && board->enPassantSquare != -1) 
        board->enPassantSquare = NO_EP_SQUARE;

    //50 move rule counting
    if(ISPAWN(m.piece) || m.capturedPiece != EMPTY_PIECE) board->movesSinceLastChange = 0;
    else board->movesSinceLastChange++;
    board->halfMoveCount++;

    //Calculate checks and change turn.
    if(board->turn == WHITE)
    {
        if(IS_IN_CHECK_W(board->flags)) UNCHECK_W(board->flags);
        if(isThreatened(board, board->kingSquare_b, BLACK)) CHECK_B(board->flags);
    }
    else
    {
        if(IS_IN_CHECK_B(board->flags)) UNCHECK_B(board->flags);
        if(isThreatened(board, board->kingSquare_w, WHITE)) CHECK_W(board->flags);
    }
    board->turn ^= 1;
    board->hashCode ^= zobrist_keys[780];
    
    if(m.capturedPiece != EMPTY_PIECE || ISPAWN(m.piece)) board->lastChangeIndex = board->repetitionIndex;
    board->repetitionHashCodes[board->repetitionIndex++] = board->hashCode;

    return 0;
}

void applyNullMove(bitboard* board)
{
    assert(board);
    board->turn ^= 1;
    board->hashCode ^= zobrist_keys[780];
}

move_c getStructFromString(bitboard* board, char* str)
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
    if(piece == EMPTY_PIECE)
    {
        DEBUG_ERROR("Could not find piece on start square.");
        return (move_c){0};
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

    move_c m = {0};
    createCompactMove(&m, startSquare, endSquare, promoteTo);

    move_c moveList[MAX_MOVES];
    int count = generateMoveList(moveList, board, GET_ALL_MOVES);
    int isPotentialMove = 0;
    for(int index = 0; index < count; index++)
    {
        if(m.startSquare == moveList[index].startSquare && m.endSquare == moveList[index].endSquare)
        {
            isPotentialMove = 1;
            break;
        }
    }

    if(!isPotentialMove)
    {
        printf("Piece move is not legal.\n");
        return (move_c){0};
    }
    
    return m;
}

int moveFromStruct(bitboard* board, move_c m)
{   
    assert(board);
    assert(IS_VALID_MOVE(m));

    if(movePiece(board, m) != 0) 
    {
        DEBUG_ERROR("Failed to move piece from struct.");
        return -1;
    }
    
    if((ISWHITE(board->turn) && isThreatened(board, board->kingSquare_b, BLACK)) || (ISBLACK(board->turn) && isThreatened(board, board->kingSquare_w, WHITE)))
    {
        //Psuedo-legal move generator created an illegal move. Fail silently.
        unmove(board);
        return -1;
    }

    //Terminal gamestate validations have been moved to engine.c search functions.
    /*
    #ifndef NDEBUG
    uint64_t hc = getHashCode(board);
    if(board->hashCode != hc)
    {
        uint64_t xor = hc ^ board->hashCode;
        printf("Error! board code 0x%016llx != expected 0x%016llx (XOR = 0x%016llx)\n", board->hashCode, hc, xor);
        for(int i = 780; i >= 0; i--)
        {
            if(zobrist_keys[i] == xor) printf("\tXOR = zobrist_keys[%d]\n", i);
            break;
        }
        board_print(board, 1);
        exit(0);
    }
    #endif
    */
   
    return 0;
}

move_d unmove(bitboard *board)
{
    assert(board);

    move_d m = moves_pop(board);
    if(!IS_VALID_MOVE(m))
    {
        DEBUG_ERROR("No move history to undo.");
        return (move_d){0};
    }
    board->repetitionIndex--;
    board->lastChangeIndex = m.lastChangeIndex;

    if(m.promoteTo)
    {
        board_clear_square(board, m.endSquare);
        board_set(board, m.startSquare, m.piece);
    }
    else board_move_piece_quietly(board, m.endSquare, m.startSquare);

    if(m.capturedPiece != EMPTY_PIECE) 
    {
        if(m.prevEnPassantSquare == m.endSquare && ISPAWN(m.capturedPiece))
        {
            if(ISWHITE(m.piece)) board_set(board, m.endSquare - 8, m.capturedPiece);
            else board_set(board, m.endSquare + 8, m.capturedPiece);
        }
        else board_set(board, m.endSquare, m.capturedPiece);
    }

    if(ISKING(m.piece))
    {
        if(ISWHITE(m.piece)) board->kingSquare_w = m.startSquare;
        else board->kingSquare_b = m.startSquare;
        
        //undo castle
        if(abs(m.endSquare - m.startSquare) == 2)
        {
            if(m.endSquare == 2) board_move_piece_quietly(board, 3, 0);
            else if(m.endSquare == 6) board_move_piece_quietly(board, 5, 7);
            else if(m.endSquare == 58) board_move_piece_quietly(board, 59, 56);
            else if(m.endSquare == 62) board_move_piece_quietly(board, 61, 63);
        }
    }
    
    
    if(KINGSIDE_CASTLE_WHITE(m.prevFlags) != KINGSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[768];
    if(QUEENSIDE_CASTLE_WHITE(m.prevFlags) != QUEENSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[769];
    if(KINGSIDE_CASTLE_BLACK(m.prevFlags) != KINGSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[770];
    if(QUEENSIDE_CASTLE_BLACK(m.prevFlags) != QUEENSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[771];

    board->flags = m.prevFlags;

    board->movesSinceLastChange = m.previousMovesSinceLastChange;

    board->hashCode ^= getEnPassantHash(board);
    board->enPassantSquare = m.prevEnPassantSquare;
    board->hashCode ^= getEnPassantHash(board);

    board->turn ^= 1;
    board->hashCode ^= zobrist_keys[780];
    
    board->halfMoveCount--;

    return m;
}