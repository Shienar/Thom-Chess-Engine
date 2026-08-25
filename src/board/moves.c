#include "board/bitboard.h"
#include "board/moves.h" 
#include "debug.h"
#include <string.h>

int generateMoveList(move* movesList, bitboard* board, int capturesOnly)
{
    int size = 0;
    
    int allyColor = board->turn;
    int enemyColor = FLIP_COLOR(board->turn);

    uint64_t allies = board->pieces_side[allyColor];
    uint64_t enemies = board->pieces_side[enemyColor];
    uint64_t knights = board->pieces[KNIGHT | allyColor];
    uint64_t bishop = board->pieces[BISHOP | allyColor];
    uint64_t rook = board->pieces[ROOK | allyColor];
    uint64_t queen = board->pieces[QUEEN | allyColor];
    uint64_t king = board->pieces[KING | allyColor];

    int enemyKingSq = board->kingSquare[enemyColor];
    uint64_t kingThreats_Pawn = pawnAttacks[enemyColor][board->kingSquare[enemyColor]];
    uint64_t kingThreats_Knight = knightAttacks[board->kingSquare[enemyColor]];
    uint64_t kingThreats_Bishop = bishopMoves(enemies, allies, enemyKingSq);
    uint64_t kingThreats_Rook = rookMoves(enemies, allies, enemyKingSq);;
    
    uint64_t kingThreats_Queen = kingThreats_Bishop | kingThreats_Rook;

    uint64_t mask = 0;
    int piece = PAWN|(board->turn);
    if(ISWHITE(piece))
    {
        mask = WHITE_PAWN_DOUBLEPUSH_MASK(board);
        if(capturesOnly == GET_CAPTURES_AND_CHECKS)
            mask &= kingThreats_Pawn;
        else if(capturesOnly > GET_CAPTURES_AND_CHECKS)
            mask = 0;
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            createCompactMove(&movesList[size++], endSquare - 16, endSquare, 0);
            mask&=(mask - 1);
        }

        mask = WHITE_PAWN_PUSH_MASK(board);
        if(capturesOnly == GET_CAPTURES_AND_CHECKS)
            mask &= kingThreats_Pawn;
        else if(capturesOnly >= GET_CAPTURES) 
            mask = 0;
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
        mask = BLACK_PAWN_DOUBLEPUSH_MASK(board);
        if(capturesOnly == GET_CAPTURES_AND_CHECKS)
            mask &= kingThreats_Pawn;
        else if(capturesOnly >= GET_CAPTURES) 
            mask = 0;
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            createCompactMove(&movesList[size++], endSquare + 16, endSquare, 0);
            
            mask&=(mask - 1);
        }

        mask = BLACK_PAWN_PUSH_MASK(board);
        if(capturesOnly == GET_CAPTURES_AND_CHECKS)
            mask &= kingThreats_Pawn;
        else if(capturesOnly >= GET_CAPTURES) 
            mask = 0;
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
        if(capturesOnly >= GET_CAPTURES) 
            mask &= enemies;
        else if(capturesOnly == GET_CAPTURES_AND_CHECKS)
            mask &= (enemies | kingThreats_Knight);
        
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
        if(capturesOnly == GET_CAPTURES_AND_CHECKS) 
            moveMask &= (enemies | kingThreats_Bishop);
        else if(capturesOnly >= GET_CAPTURES)
            moveMask &= enemies;

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
        if(capturesOnly == GET_CAPTURES_AND_CHECKS) 
            moveMask &= (enemies | kingThreats_Rook);
        else if(capturesOnly >= GET_CAPTURES)
            moveMask &= enemies;

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
        if(capturesOnly == GET_CAPTURES_AND_CHECKS) 
            moveMask &= (enemies | kingThreats_Queen);
        else if(capturesOnly >= GET_CAPTURES)
            moveMask &= enemies;

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
    attackers |= kingAttacks[square] & (board->pieces[WHITE_KING] | board->pieces[BLACK_KING]);

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

static const int pieceValuesSEE[15] = {100, 100, 300, 300, 300, 300, 500, 500, 900, 900, 1e6, 1e6, 0, 0, 0};
int staticExchangeEvaluation(bitboard* board, move m)
{
    int gain[MAX_PLY];
    int ply = 0;

    int piece = findPieceOnSquare(board, m.startSquare);
    int capturedPiece = findPieceOnSquare(board, m.endSquare);
    if(capturedPiece == EMPTY_PIECE && ISPAWN(piece) && m.endSquare == board->enPassantSquare)
        capturedPiece = FLIP_COLOR(piece);

    int side = COLOR(piece);
    int promoteTo = m.promoteTo;

    //Handle the initial capture
    gain[0] = pieceValuesSEE[capturedPiece];
    if(promoteTo)
        gain[0] += pieceValuesSEE[promoteTo] - pieceValuesSEE[PAWN];

    uint64_t removedMask = singleBitMask(m.startSquare);

    uint64_t occupied = board->pieces_all & ~removedMask;
    uint64_t attackers = getAttackers(board, m.endSquare, occupied) & (~removedMask);

    side = FLIP_COLOR(side);

    int attackerSquare;
    int attackerPiece;
    capturedPiece = piece;

    while(ply < MAX_PLY - 1)
    {
        attackerSquare = findLVA(board, attackers, side, &attackerPiece);
        if(attackerSquare == -1) break;

        ply++;
        gain[ply] = pieceValuesSEE[capturedPiece] - gain[ply - 1];
        capturedPiece = attackerPiece;

        if(gain[ply] < 0)
            break;

        removedMask |= singleBitMask(attackerSquare);
        occupied &= ~removedMask;
        attackers &= ~removedMask;
        if(ISPAWN(attackerPiece) || ISBISHOP(attackerPiece) || ISROOK(attackerPiece) || ISQUEEN(attackerPiece)) 
            attackers |= (getSlidingAttackers(board, m.endSquare, occupied) & (~removedMask));
        
        side = FLIP_COLOR(side);
    }

    while(ply > 0)
    {
        if(-gain[ply] < gain[ply - 1])
            gain[ply - 1] = -gain[ply];
        ply--;
    }
    return gain[0];
}

//Only used when move ordering matters (not perft)
moveIterator* create_move_iterator(bitboard* board, int capturesOnly, 
                                        move* requiredMoves, move* excludedMove,
                                        move* pvMove, move* ttMove, 
                                        int16_t history[2][6][64], int16_t captureHistory[6][64][6],
                                        move* killerMoves, 
                                        move* counterMove, move* followUpMove)
{
    moveIterator* iter = malloc(sizeof(moveIterator));
    iter->moveList = malloc(MAX_MOVES * sizeof(move));
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
        int currentPiece = findPieceOnSquare(board, iter->moveList[i].startSquare);
        int capturedPiece = findPieceOnSquare(board, iter->moveList[i].endSquare);
        int isEP = 0;
        if(capturedPiece == EMPTY_PIECE && ISPAWN(currentPiece) && iter->moveList[i].endSquare == board->enPassantSquare)
        {
            capturedPiece = FLIP_COLOR(currentPiece);
            isEP = 1;
        }
        int isCapture = capturedPiece != EMPTY_PIECE || isEP;

        if(excludedMove && iter->moveList[i].raw == excludedMove->raw)
        {
            //We're moving this move to the front of the list and setting its score to INT16_MIN, effectively skipping it.
            if(i > iter->visitedCount)
            {
                iter->moveScores[i] = iter->moveScores[iter->visitedCount];
                iter->moveScores[iter->visitedCount] = INT16_MIN;

                move temp = iter->moveList[iter->visitedCount];
                iter->moveList[iter->visitedCount] = iter->moveList[i];
                iter->moveList[i] = temp;
            }
            iter->visitedCount++;
        }
        else if(ttMove && iter->moveList[i].raw == ttMove->raw)
            iter->moveScores[i] = TT_MOVE_SCORE;
        else if(pvMove && iter->moveList[i].raw == pvMove->raw)
            iter->moveScores[i] = PV_MOVE_SCORE;
        else if(isCapture)
        {
            int seeValue = staticExchangeEvaluation(board, iter->moveList[i]);
            int historyBonus = captureHistory[currentPiece / 2][iter->moveList[i].endSquare][capturedPiece / 2] / 64;
            if(seeValue >= 0) iter->moveScores[i] = CAPTURE_SCORE + seeValue + historyBonus;
            else if(capturesOnly == GET_WINNING_CAPTURES)
            {
                if(i > iter->visitedCount)
                {
                    iter->moveScores[i] = iter->moveScores[iter->visitedCount];
                    iter->moveScores[iter->visitedCount] = INT16_MIN;

                    move temp = iter->moveList[iter->visitedCount];
                    iter->moveList[iter->visitedCount] = iter->moveList[i];
                    iter->moveList[i] = temp;
                }
                iter->visitedCount++;
            } 
            else iter->moveScores[i] = -CAPTURE_SCORE + seeValue + historyBonus;
        }
        else if(killerMoves && iter->moveList[i].raw == killerMoves[0].raw)
            iter->moveScores[i] = KILLER_1_SCORE;
        else if(killerMoves && iter->moveList[i].raw == killerMoves[1].raw)
            iter->moveScores[i] = KILLER_2_SCORE;
        else if(iter->moveList[i].promoteTo)
            iter->moveScores[i] = PROMOTION_SCORE + iter->moveList[i].promoteTo;
        else if(history)
        {
            iter->moveScores[i] = history[board->turn][findPieceOnSquare(board, iter->moveList[i].startSquare) / 2][iter->moveList[i].endSquare];

            if(counterMove && iter->moveList[i].raw == counterMove->raw)
                iter->moveScores[i] = _min(iter->moveScores[i] + COUNTERMOVE_BONUS, MAX_HISTORY_SCORE);
            else if(followUpMove && iter->moveList[i].raw == followUpMove->raw)
                iter->moveScores[i] = _min(iter->moveScores[i] + FOLLOWUPMOVE_BONUS, MAX_HISTORY_SCORE);
        }
        else 
        {
            int pc = findPieceOnSquare(board, iter->moveList[i].startSquare);
            iter->moveScores[i] = (ISKING(pc)) ? -1 : pc;
        }
    }
    return iter;
}

//A step of selection sort that returns the next move.
move* iterate_next_move(moveIterator* iter)
{
    if(iter->visitedCount >= iter->count) return NULL;

    int bestIndex = -1;
    int maxScoreRemaining = INT16_MIN;

    //Select highest remaining move from those not chosen yet.
    for(int j = iter->visitedCount; j < iter->count; j++) 
    {
        if(iter->moveScores[j] > maxScoreRemaining) 
        {
            bestIndex = j;
            maxScoreRemaining = iter->moveScores[j];
        }
    }

    if(bestIndex == -1) return NULL;

    //Move it to the front of the list where it can be skipped over in subsequent iterations.
    if(bestIndex > iter->visitedCount)
    {
        int tempScore = iter->moveScores[bestIndex];
        iter->moveScores[bestIndex] = iter->moveScores[iter->visitedCount];
        iter->moveScores[iter->visitedCount] = tempScore;

        move temp = iter->moveList[iter->visitedCount];
        iter->moveList[iter->visitedCount] = iter->moveList[bestIndex];
        iter->moveList[bestIndex] = temp;
    }
    
    return &iter->moveList[iter->visitedCount++];
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

int isThreatened(bitboard* board, int square, int defendingColor)
{
    int attackingColor = FLIP_COLOR(defendingColor);

    uint64_t enemyPieces = board->pieces_side[attackingColor];
    uint64_t allyPieces = board->pieces_side[defendingColor];
    if(pawnAttacks[defendingColor][square] & board->pieces[PAWN | attackingColor])
        return THREAT_TYPE_PAWN;
    if(knightAttacks[square] & board->pieces[KNIGHT | attackingColor])
        return THREAT_TYPE_KNIGHT;
    if(kingAttacks[square] & board->pieces[KING | attackingColor])
        return THREAT_TYPE_KING;
        
    uint64_t bishopqueen = board->pieces[BISHOP | attackingColor] | board->pieces[QUEEN | attackingColor];
    uint64_t rookqueen = board->pieces[ROOK | attackingColor] | board->pieces[QUEEN | attackingColor];

    if(bishopMoves(allyPieces, enemyPieces, square)&(bishopqueen)) 
        return THREAT_TYPE_BISHOPQUEEN;
    if(rookMoves(allyPieces, enemyPieces, square)&(rookqueen)) 
        return THREAT_TYPE_ROOKQUEEN;

    return THREAT_TYPE_NONE;
}

uint64_t pyrrhicPawnAttacks(int square, int color)
{
    return pawnAttacks[FLIP_COLOR(color)][square];
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

        if(board->canKingsideCastle_w && !board->in_check && !(board->pieces_all&0x60) && 
                                                        !isThreatened(board, square, color) && 
                                                        !isThreatened(board, square+1, color) && 
                                                        !isThreatened(board, square+2, color)) returnedValue|=0x40;

        //betweenMask = 0xE; //Square 1 and 2 and 3 between rook on 0 and king on 4

        if(board->canQueensideCastle_w && !board->in_check && !(board->pieces_all&0xE) && 
                                                        !isThreatened(board, square, color) && 
                                                        !isThreatened(board, square-1, color) && 
                                                        !isThreatened(board, square-2, color)) returnedValue|=0x4;
        
    }
    else
    {
        returnedValue&=(~board->pieces_side[BLACK]);

        //uint64_t betweenMask = 0x6000000000000000; //Squares 61 and 62 between king on 60 and rook on 63

        //Black kingside
        if(board->canKingsideCastle_b && !board->in_check && !(board->pieces_all&0x6000000000000000) && 
                                                            !isThreatened(board, square, color) &&
                                                            !isThreatened(board, square+1, color) && 
                                                            !isThreatened(board, square+2, color)) returnedValue|=0x4000000000000000;

        //betweenMask = 0x0E00000000000000; //Square 57 and 58 and 59 between rook on 56 and king on 60
        if(board->canQueensideCastle_b && !board->in_check && !(board->pieces_all&0x0E00000000000000) &&
                                                            !isThreatened(board, square, color) &&
                                                            !isThreatened(board, square-1, color) && 
                                                            !isThreatened(board, square-2, color)) returnedValue|=0x0400000000000000;
    }
    
    return returnedValue;
}

int movePiece(bitboard *board, move compactMove, repetitionVector* repetitions)
{
    assert(board);
    assert(compactMove.startSquare >= 0 && compactMove.startSquare <= 63 && compactMove.endSquare >= 0 && compactMove.endSquare <= 63);

    //Clear the en passant hash early. clear the en passant square later.
    //En passant square is required for legal moves, en passant hash can change by the end of the function.
    board->hashCode ^= getEnPassantHash(board);

    int piece = findPieceOnSquare(board, compactMove.startSquare);
    int capturedPiece = findPieceOnSquare(board, compactMove.endSquare);
    switch(PIECE(piece))
    {
        case PAWN:
            int difference = abs(compactMove.startSquare - compactMove.endSquare);
            
            if(difference == 8 || difference == 16)
            {
                if((ISWHITE(piece) && compactMove.endSquare > 55) || (ISBLACK(piece) && compactMove.endSquare < 8))
                {
                    //Promotion
                    if(compactMove.promoteTo == 0) compactMove.promoteTo = QUEEN;
                    board_clear_square(board, compactMove.startSquare);
                    board_set(board, compactMove.endSquare, (COLOR(piece)|compactMove.promoteTo));
                }
                else
                {
                    board_move_piece_quietly(board, compactMove.startSquare, compactMove.endSquare);
                }
                
                //Update en passant square if necessary.
                if(difference == 16)
                {
                    if(ISWHITE(piece)) 
                    {
                        board->enPassantSquare = compactMove.endSquare - 8;
                        board->hashCode ^= getEnPassantHash(board);
                    }
                    else 
                    {
                        board->enPassantSquare = compactMove.endSquare + 8;
                        board->hashCode ^= getEnPassantHash(board);
                    }
                }
            }
            else if(difference == 7 || difference == 9)
            {
                //Diagonal Capture

                //Check for en passant.
                if(compactMove.endSquare == board->enPassantSquare)
                {
                    capturedPiece = PAWN | (FLIP_COLOR(board->turn));
                    if(ISWHITE(piece)) 
                        board_clear_square(board, board->enPassantSquare - 8);
                    else
                        board_clear_square(board, board->enPassantSquare + 8);
                }

                if((ISWHITE(piece) && compactMove.endSquare > 55) || (ISBLACK(piece) && compactMove.endSquare < 8))
                {
                    //Promotion
                    if(compactMove.promoteTo == 0) compactMove.promoteTo = QUEEN;
                    board_clear_square(board, compactMove.startSquare);
                    board_clear_square(board, compactMove.endSquare);
                    board_set(board, compactMove.endSquare, (COLOR(piece)|compactMove.promoteTo));
                }
                else
                {
                    board_move_piece(board, compactMove.startSquare, compactMove.endSquare);
                }      
            }
            break;
        case ROOK:
            if(compactMove.startSquare == 7 && board->canKingsideCastle_w) { board->canKingsideCastle_w = 0; board->hashCode ^= zobrist_keys[768]; }
            else if(compactMove.startSquare == 0 && board->canQueensideCastle_w) { board->canQueensideCastle_w = 0; board->hashCode ^= zobrist_keys[769]; }
            else if(compactMove.startSquare == 63 && board->canKingsideCastle_b) { board->canKingsideCastle_b = 0; board->hashCode ^= zobrist_keys[770]; }
            else if(compactMove.startSquare == 56 && board->canQueensideCastle_b) { board->canQueensideCastle_b = 0; board->hashCode ^= zobrist_keys[771]; }
        case KNIGHT:
        case BISHOP:
        case QUEEN:
            board_move_piece(board, compactMove.startSquare, compactMove.endSquare);
            break;
        case KING:
            if(ISBLACK(piece)) 
            {
                board->kingSquare[BLACK] = compactMove.endSquare;
                if(board->canKingsideCastle_b) { board->canKingsideCastle_b = 0; board->hashCode ^= zobrist_keys[770]; }
                if(board->canQueensideCastle_b) { board->canQueensideCastle_b= 0; board->hashCode ^= zobrist_keys[771]; }
            }
            else 
            {
                board->kingSquare[WHITE] = compactMove.endSquare;
                if(board->canKingsideCastle_w) { board->canKingsideCastle_w = 0; board->hashCode ^= zobrist_keys[768]; }
                if(board->canQueensideCastle_w) { board->canQueensideCastle_w= 0; board->hashCode ^= zobrist_keys[769]; }
            }
            board_move_piece(board, compactMove.startSquare, compactMove.endSquare);

            //Castling
            if(compactMove.startSquare - compactMove.endSquare == 2) board_move_piece_quietly(board, compactMove.startSquare - 4, compactMove.endSquare + 1);
            else if(compactMove.startSquare - compactMove.endSquare == -2) board_move_piece_quietly(board, compactMove.startSquare + 3, compactMove.endSquare - 1);
            break;
        default:
            DEBUG_ERROR("Attempted to move invalid piece type.");
            return -1;
            break;
    }

    if(ISROOK(capturedPiece))
    {
        if(compactMove.endSquare == 7 && board->canKingsideCastle_w) { board->canKingsideCastle_w = 0; board->hashCode ^= zobrist_keys[768]; }
        else if(compactMove.endSquare == 0 && board->canQueensideCastle_w) { board->canQueensideCastle_w= 0; board->hashCode ^= zobrist_keys[769]; }
        else if(compactMove.endSquare == 63 && board->canKingsideCastle_b) { board->canKingsideCastle_b = 0; board->hashCode ^= zobrist_keys[770]; }
        else if(compactMove.endSquare == 56 && board->canQueensideCastle_b) { board->canQueensideCastle_b= 0; board->hashCode ^= zobrist_keys[771]; }
    }

    if(!(ISPAWN(piece) && abs(compactMove.startSquare - compactMove.endSquare) == 16) && board->enPassantSquare != NO_EP_SQUARE) 
        board->enPassantSquare = NO_EP_SQUARE;

    //50 move rule counting
    if(ISPAWN(piece) || capturedPiece != EMPTY_PIECE)
    {
        board->halfmoveClock = 0;
        board->lastChangeIndex = board->halfMoveCount;
    }
    else board->halfmoveClock++;
    board->halfMoveCount++;

    board->turn ^= 1;
    board->hashCode ^= zobrist_keys[780];
    board->in_check = isThreatened(board, board->kingSquare[board->turn], board->turn) ? 1 : 0;
    
    //3-fold
    if(repetitions)
    {
        if(!repetitions->hashCodes)
        {
            repetitions->capacity = 16;
            repetitions->hashCodes = calloc(repetitions->capacity, sizeof(uint64_t));
        }

        //This supports going up/down the array using halfMoveCount as a guide.
        //FEN may create an offset that wastes the first few indices.
        while(repetitions->capacity <= board->halfMoveCount)
        {
            repetitions->capacity *= 2;
            repetitions->hashCodes = realloc(repetitions->hashCodes, repetitions->capacity * sizeof(uint64_t));
        }
        repetitions->hashCodes[board->halfMoveCount] = board->hashCode;
    }

    return 0;
}

void applyNullMove(bitboard* board, bitboard* newBoard, repetitionVector* repetitions)
{
    assert(board);
    assert(newBoard);
    memcpy(newBoard, board, sizeof(bitboard));

    newBoard->turn = board->turn ^ 1;
    newBoard->hashCode = board->hashCode ^ zobrist_keys[780];
    newBoard->halfMoveCount++;

    newBoard->hashCode ^= getEnPassantHash(board);
    newBoard->enPassantSquare = NO_EP_SQUARE;
    
    while(repetitions->capacity <= board->halfMoveCount)
    {
        repetitions->capacity *= 2;
        repetitions->hashCodes = realloc(repetitions->hashCodes, repetitions->capacity * sizeof(uint64_t));
    }
    repetitions->hashCodes[board->halfMoveCount] = board->hashCode;

}

move getStructFromString(bitboard* board, char* str)
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
        return (move){0};
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

    move m = {0};
    createCompactMove(&m, startSquare, endSquare, promoteTo);

    move moveList[MAX_MOVES];
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
        return (move){0};
    }
    
    return m;
}

int moveFromStruct(bitboard* board, bitboard* newBoard, move m, repetitionVector* repetitions)
{   
    assert(board);
    assert(newBoard);

    if(board != newBoard)
        memcpy(newBoard, board, sizeof(bitboard));

    if(!IS_VALID_MOVE(m) || movePiece(newBoard, m, repetitions) != 0) 
    {
        DEBUG_ERROR("Failed to move piece from struct.");
        return -1;
    }
    
    //Psuedo-legal move generator created an illegal move. Fail silently.
    if(isThreatened(newBoard, newBoard->kingSquare[FLIP_COLOR(newBoard->turn)], FLIP_COLOR(newBoard->turn)))
        return -1;

    #ifdef VERIFY
        uint64_t hc = getHashCode(newBoard);
        if(newBoard->hashCode != hc)
        {
            uint64_t xor = hc ^ newBoard->hashCode;
            printf("Error! board code 0x%016" PRIx64 " != expected 0x%016" PRIx64 " (XOR = 0x%016" PRIx64 ")\n", board->hashCode, hc, xor);
            for(int i = 780; i >= 0; i--)
            {
                if(zobrist_keys[i] == xor) 
                {
                    printf("\tXOR = zobrist_keys[%d]\n", i);
                    break;
                }
            }
            assert(0);
        }
    #endif
   
    return 0;
}