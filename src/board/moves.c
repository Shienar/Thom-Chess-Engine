#include "board/bitboard.h"
#include "board/moves.h" 
#include "debug.h"
#include "analyze/history.h"
#include <string.h>

int generateMoveList(move* movesList, bitboard* board, int genType)
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
        if(genType == GET_NOISY_MOVES)
            mask &= kingThreats_Pawn;
        else if(genType == GET_QUIET_MOVES)
            mask &= ~kingThreats_Pawn;
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            createCompactMove(&movesList[size++], endSquare - 16, endSquare, 0);
            mask&=(mask - 1);
        }

        mask = WHITE_PAWN_PUSH_MASK(board);
        if(genType == GET_NOISY_MOVES)
            mask &= (kingThreats_Pawn | board_rank[7]);
        else if(genType == GET_QUIET_MOVES)
            mask &= ~(kingThreats_Pawn | board_rank[7]);
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

        if(genType != GET_QUIET_MOVES)
        {
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
    }
    else
    {
        mask = BLACK_PAWN_DOUBLEPUSH_MASK(board);
        if(genType == GET_NOISY_MOVES)
            mask &= kingThreats_Pawn;
        else if(genType == GET_QUIET_MOVES)
            mask &= ~kingThreats_Pawn;
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            createCompactMove(&movesList[size++], endSquare + 16, endSquare, 0);
            
            mask&=(mask - 1);
        }

        mask = BLACK_PAWN_PUSH_MASK(board);
        if(genType == GET_NOISY_MOVES)
            mask &= (kingThreats_Pawn | board_rank[0]);
        else if(genType == GET_QUIET_MOVES)
            mask &= ~(kingThreats_Pawn | board_rank[0]);
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
        
        if(genType != GET_QUIET_MOVES)
        {
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
    }

    while(knights) 
    {
        int startSquare = __builtin_ctzll(knights);
        uint64_t mask = knightMoves(allies, startSquare);
        if(genType == GET_NOISY_MOVES) 
            mask &= (enemies | kingThreats_Knight);
        else if(genType == GET_QUIET_MOVES)
            mask &= ~(enemies | kingThreats_Knight);
        
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
        if(genType == GET_NOISY_MOVES) 
            moveMask &= (enemies | kingThreats_Bishop);
        else if(genType == GET_QUIET_MOVES)
            moveMask &= ~(enemies | kingThreats_Bishop);

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
        if(genType == GET_NOISY_MOVES) 
            moveMask &= (enemies | kingThreats_Rook);
        else if(genType == GET_QUIET_MOVES)
            moveMask &= ~(enemies | kingThreats_Rook);

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
        if(genType == GET_NOISY_MOVES) 
            moveMask &= (enemies | kingThreats_Queen);
        else if(genType == GET_QUIET_MOVES)
            moveMask &= ~(enemies | kingThreats_Queen);

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
        if(genType == GET_NOISY_MOVES) 
            moveMask &= enemies;
        else if(genType == GET_QUIET_MOVES)
            moveMask &= ~enemies;

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

int pieceValuesSEE[15] = {100, 100, 300, 300, 300, 300, 500, 500, 900, 900, 1e6, 1e6, 0, 0, 0};
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

void scoreMoves(moveIterator* iter, int startIndex, int count)
{
    bitboard* board = iter->board;
    threadContext* context = iter->context;
    for(int i = startIndex; i < startIndex + iter->count; i++)
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

        if(iter->moveList[i].raw == iter->excludedMove.raw)
            iter->moveScores[i] = EXCLUDED_SCORE;
        else if(iter->moveList[i].raw == iter->ttMove.raw)
            iter->moveScores[i] = TT_MOVE_SCORE;
        else if(isCapture || iter->moveList[i].promoteTo)
        {
            int seeValue = staticExchangeEvaluation(board, iter->moveList[i]);
            int historyBonus = getCaptureHistoryOffset(context, currentPiece, iter->moveList[i].endSquare, capturedPiece);
            iter->moveScores[i] += ((seeValue >= 0) ? CAPTURE_SCORE : -CAPTURE_SCORE) + seeValue + historyBonus;
        }
        else if(iter->moveList[i].raw == iter->killer1Move.raw || iter->moveList[i].raw == iter->killer2Move.raw)
            iter->moveScores[i] = INT16_MIN;
        else
        {
            iter->moveScores[i] = getHistoryValue(context, board->turn, currentPiece, iter->moveList[i].endSquare);

            if(iter->counterMove.raw && iter->moveList[i].raw == iter->counterMove.raw)
                iter->moveScores[i] = _min(iter->moveScores[i] + COUNTERMOVE_BONUS, MAX_HISTORY_SCORE + 2);
            else if(iter->followUpMove.raw && iter->moveList[i].raw == iter->followUpMove.raw)
                iter->moveScores[i] = _min(iter->moveScores[i] + FOLLOWUPMOVE_BONUS, MAX_HISTORY_SCORE + 1);
        }
    }
}

int16_t selectNextBestMove(moveIterator* iter, uint8_t startIndex, uint8_t* searchedCount, uint8_t maxCount)
{
    if(*searchedCount >= maxCount) 
        return -1;

    int bestIndex = -1;
    int maxScoreRemaining = INT16_MIN;

    uint8_t shouldSkipMove;
    move* temp;
    do
    {
        shouldSkipMove = 0;
        bestIndex = -1;
        maxScoreRemaining = INT16_MIN;

        //Select highest remaining move from those not chosen yet.
        for(uint8_t j = startIndex + *searchedCount; j < maxCount; j++) 
        {
            if(iter->moveScores[j] > maxScoreRemaining) 
            {
                bestIndex = j;
                maxScoreRemaining = iter->moveScores[j];
            }
        }

        if(bestIndex == -1)
            return -1;

        //Move it to the front of the list where it can be skipped over in subsequent iterations.
        if(bestIndex > startIndex + *searchedCount)
        {
            int16_t tempScore = iter->moveScores[bestIndex];
            iter->moveScores[bestIndex] = iter->moveScores[startIndex + *searchedCount];
            iter->moveScores[startIndex + *searchedCount] = tempScore;

            move temp = iter->moveList[startIndex + *searchedCount];
            iter->moveList[startIndex + *searchedCount] = iter->moveList[bestIndex];
            iter->moveList[bestIndex] = temp;
        }

        temp = &iter->moveList[startIndex + *searchedCount];
        if(temp->raw == iter->excludedMove.raw ||
            temp->raw == iter->ttMove.raw ||
            temp->raw == iter->killer1Move.raw ||
            temp->raw == iter->killer2Move.raw )
        {
            (*searchedCount)++;
            shouldSkipMove = 1;
        }
    }while(shouldSkipMove);
    
    (*searchedCount)++;
    return startIndex + *searchedCount - 1;
}

moveIterator* create_move_iterator(threadContext* context, int genType, int ply, move* ttMove)
{
    moveIterator* iter = calloc(1, sizeof(moveIterator));
    iter->type = genType;
    iter->stage = 0;
    iter->board = &context->boardStack[ply];
    iter->context = context;

    if(ply == 0 && context->searchedMoves[0].raw)
    {
        memcpy(iter->moveList, context->searchedMoves, MAX_REQUIRED_MOVES * sizeof(move));
        while(context->searchedMoves[iter->count].raw)
            iter->count++;
        scoreMoves(iter, 0, iter->count);
    }
    else
    {
        iter->ttMove.raw = ttMove ? ttMove->raw : 0;

        iter->killer1Move = context->killerMoves[ply][0];
        iter->killer2Move = context->killerMoves[ply][1];

        move* counterMove  = getCounterMove(context, ply);
        move* followUpMove = getFollowupMove(context, ply);
        iter->counterMove.raw  = counterMove  ? counterMove->raw  : 0;
        iter->followUpMove.raw = followUpMove ? followUpMove->raw : 0;

        iter->excludedMove.raw = context->excludedMove[ply].raw;
    }
    return iter;
}

int isMoveUsable(bitboard* board, move m)
{
    uint64_t allies = board->pieces_side[board->turn];
    uint64_t enemies = board->pieces_side[FLIP_COLOR(board->turn)];

    uint8_t piece = findPieceOnSquare(board, m.startSquare);
    if(piece == EMPTY_PIECE || COLOR(piece) != board->turn)
        return 0;

    uint64_t destMask = singleBitMask(m.endSquare);

    switch(PIECE(piece))
    {
        case PAWN:
            //Captures (Including en passant)
            if((destMask & pawnAttacks[COLOR(piece)][m.startSquare]))
            {
                if(m.endSquare == board->enPassantSquare)
                    return 1;
                
                int capturedPiece = findPieceOnSquare(board, m.endSquare);
                if(capturedPiece == EMPTY_PIECE || COLOR(capturedPiece) == COLOR(piece))
                    return 0;
                else
                    return 1;
            }

            //Can't use the original global board push mask generations, else moves tt moves like a2->c4 might
            //get interpreted as valid pawn moves.
            if(ISWHITE(piece))
            {
                //Single pushes
                uint64_t singlePushMask = (singleBitMask(m.startSquare) << 8)&(~board->pieces_all);
                if(destMask & singlePushMask)
                    return 1;

                //Double pushes
                if(getRow(m.startSquare) == 1 && (destMask & ((singlePushMask << 8)&(~board->pieces_all)) ))
                    return 1;

                return 0;
            }
            else
            {
                //Single pushes
                uint64_t singlePushMask = (singleBitMask(m.startSquare) >> 8)&(~board->pieces_all);
                if(destMask & singlePushMask)
                    return 1;

                //Double pushes
                if(getRow(m.startSquare) == 6 && (destMask & ((singlePushMask >> 8)&(~board->pieces_all)) ))
                    return 1;

                return 0;
            }
            break;
        case KNIGHT:
            return (knightMoves(allies, m.startSquare) & destMask) != 0;
        case BISHOP:
            return (bishopMoves(allies, enemies, m.startSquare) & destMask) != 0;
        case ROOK:
            return (rookMoves(allies, enemies, m.startSquare) & destMask) != 0;
        case QUEEN:
            return (queenMoves(allies, enemies, m.startSquare) & destMask) != 0;
        case KING:
            return (kingMoves(board, m.startSquare, board->turn) & destMask) != 0;
        default:
            __builtin_unreachable();
            break;
    }

    return 0;
}

move* iterate_next_move(moveIterator* iter)
{
    move* returnedMove = NULL;
    
    switch(iter->stage)
    {
        case ITER_STAGE_TT:
            if(iter->type == GET_QUIET_MOVES)
                iter->stage = ITER_STAGE_KILLER_1;
            else
                iter->stage = ITER_STAGE_GET_NOISY;
                
            if(iter->ttMove.raw && isMoveUsable(iter->board, iter->ttMove))
            {
                int currentPiece = findPieceOnSquare(iter->board, iter->ttMove.startSquare);
                int capturedPiece = findPieceOnSquare(iter->board, iter->ttMove.endSquare);
                int isEP = 0;
                if(capturedPiece == EMPTY_PIECE && ISPAWN(currentPiece) && iter->ttMove.endSquare == iter->board->enPassantSquare)
                {
                    capturedPiece = FLIP_COLOR(currentPiece);
                    isEP = 1;
                }
                int isCapture = capturedPiece != EMPTY_PIECE || isEP;

                if(iter->type == GET_ALL_MOVES || (iter->type == GET_NOISY_MOVES && isCapture) || (iter->type == GET_QUIET_MOVES && !isCapture))
                {
                    returnedMove = &iter->ttMove;
                    break;
                }
            }
            else if(iter->type == GET_QUIET_MOVES)
                goto quiet_start;
        case ITER_STAGE_GET_NOISY:
            iter->count = generateMoveList(iter->moveList, iter->board, GET_NOISY_MOVES);
            scoreMoves(iter, 0, iter->count);

            iter->stage = ITER_STAGE_GOOD_NOISY;
        case ITER_STAGE_GOOD_NOISY:
            iter->selectedIndex = selectNextBestMove(iter, 0, &iter->visitedGood, iter->count);
            if(iter->selectedIndex != -1)
            {
                if(iter->moveScores[iter->selectedIndex] >= 0)
                {
                    returnedMove = &iter->moveList[iter->selectedIndex];
                    break;
                }
                else
                {
                    iter->badCaptureIndex = iter->selectedIndex;
                    iter->badCaptureCount = iter->count;
                }
            }

            if(iter->type == GET_NOISY_MOVES)
            {
                iter->stage = ITER_STAGE_BAD_NOISY;
                goto bad_noisy;
            }
            else
                iter->stage = ITER_STAGE_KILLER_1;

        case ITER_STAGE_KILLER_1:
            quiet_start:
            iter->stage = ITER_STAGE_KILLER_2;
            if(iter->killer1Move.raw && iter->killer1Move.raw != iter->ttMove.raw && isMoveUsable(iter->board, iter->killer1Move))
            {
                returnedMove = &iter->killer1Move;
                break;
            }
        case ITER_STAGE_KILLER_2:
            iter->stage = ITER_STAGE_GET_QUIETS;
            if(iter->killer2Move.raw && iter->killer2Move.raw != iter->ttMove.raw && isMoveUsable(iter->board, iter->killer2Move))
            {
                returnedMove = &iter->killer2Move;
                break;
            }
        case ITER_STAGE_GET_QUIETS:
            iter->quietIndex = iter->count;
            uint8_t quietCount = generateMoveList(&iter->moveList[iter->quietIndex], iter->board, GET_QUIET_MOVES);
            iter->count += quietCount;
            scoreMoves(iter, iter->quietIndex, quietCount);

            iter->stage = ITER_STAGE_QUIETS;
        case ITER_STAGE_QUIETS:
            iter->selectedIndex = selectNextBestMove(iter, iter->quietIndex, &iter->visitedQuiet, iter->count);
            if(iter->selectedIndex != -1)
            {
                returnedMove = &iter->moveList[iter->selectedIndex];
                break;
            }
            if(iter->type == GET_QUIET_MOVES)
            {
                iter->stage = ITER_STAGE_DONE;
                break;
            }
            else
                iter->stage = ITER_STAGE_BAD_NOISY;
        case ITER_STAGE_BAD_NOISY:
            bad_noisy:
            iter->selectedIndex = selectNextBestMove(iter, iter->badCaptureIndex, &iter->visitedBad, iter->badCaptureCount);
            if(iter->selectedIndex != -1)
            {
                returnedMove = &iter->moveList[iter->selectedIndex];
                break;
            }
            else
                iter->stage = ITER_STAGE_DONE;
        case ITER_STAGE_DONE:
            break;
        case ITER_STAGE_REQUIRED:
            iter->selectedIndex = selectNextBestMove(iter, 0, &iter->visitedGood, iter->count);
            if(iter->selectedIndex != -1)
            {
                returnedMove = &iter->moveList[iter->selectedIndex];
                break;
            }
            else
                iter->stage = ITER_STAGE_DONE;
        default:
            __builtin_unreachable(); 
            break;
    }
    return returnedMove;
}

int isThreatened(bitboard* board, int square, int defendingColor)
{
    //Silently fail (instead of segfault) on corrupted binpack games.
    //Negligable impact on perft
    if(square < 0 || square > 63)
        return 0;

    int attackingColor = FLIP_COLOR(defendingColor);

    if(pawnAttacks[defendingColor][square] & board->pieces[PAWN | attackingColor])
        return THREAT_TYPE_PAWN;
    if(knightAttacks[square] & board->pieces[KNIGHT | attackingColor])
        return THREAT_TYPE_KNIGHT;
    if(kingAttacks[square] & board->pieces[KING | attackingColor])
        return THREAT_TYPE_KING;
        
    uint64_t bishopqueen = board->pieces[BISHOP | attackingColor] | board->pieces[QUEEN | attackingColor];
    uint64_t rookqueen = board->pieces[ROOK | attackingColor] | board->pieces[QUEEN | attackingColor];

    uint64_t enemyPieces = board->pieces_side[attackingColor];
    uint64_t allyPieces = board->pieces_side[defendingColor];

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

        if(board->canKingsideCastle_w && !board->in_check && !(board->pieces_all&0x60)) 
            returnedValue|=0x40;
        if(board->canQueensideCastle_w && !board->in_check && !(board->pieces_all&0xE)) 
            returnedValue|=0x4;
        
    }
    else
    {
        returnedValue&=(~board->pieces_side[BLACK]);

        if(board->canKingsideCastle_b && !board->in_check && !(board->pieces_all&0x6000000000000000)) 
            returnedValue|=0x4000000000000000;
        if(board->canQueensideCastle_b && !board->in_check && !(board->pieces_all&0x0E00000000000000)) 
            returnedValue|=0x0400000000000000;
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
            //Attempting to castle through a threatened sqare.
            if((compactMove.endSquare - compactMove.startSquare ==  2 && isThreatened(board, compactMove.startSquare + 1, board->turn)) || //Kingside
               (compactMove.endSquare - compactMove.startSquare == -2 && isThreatened(board, compactMove.startSquare - 1, board->turn)))   //Queenside
                    return -1;

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
    //Special case - null move
    if(str[0] == '0')
        return (move){0};

    //String format: [2 char: startsquare][2 char: endsquare][1 char: promotion (q, n, r, b)]
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

    if(!m.raw || movePiece(newBoard, m, repetitions) != 0) 
    {
        DEBUG_ERROR("Failed to move piece from struct.");
        return -1;
    }
    
    //Psuedo-legal move generator created an illegal move. Fail silently.
    if(isThreatened(newBoard, newBoard->kingSquare[FLIP_COLOR(newBoard->turn)], FLIP_COLOR(newBoard->turn)))
        return -1;

    #ifdef VERIFY
        bitboard temp;
        memcpy(&temp, newBoard, sizeof(bitboard));
        generateHashCode(&temp);
        if(newBoard->hashCode != temp.hashCode)
        {
            uint64_t xor = temp.hashCode ^ newBoard->hashCode;
            printf("Error! board code 0x%016" PRIx64 " != expected 0x%016" PRIx64 " (XOR = 0x%016" PRIx64 ")\n", board->hashCode, temp.hashCode, xor);
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
        else if(newBoard->pawnHash != temp.pawnHash)
        {
            uint64_t xor = temp.pawnHash ^ newBoard->pawnHash;
            printf("Error! board code 0x%016" PRIx64 " != expected 0x%016" PRIx64 " (XOR = 0x%016" PRIx64 ")\n", board->pawnHash, temp.pawnHash, xor);
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
        else if(newBoard->nonPawnHash[WHITE] != temp.nonPawnHash[WHITE])
        {
            uint64_t xor = temp.nonPawnHash[WHITE] ^ newBoard->nonPawnHash[WHITE];
            printf("Error! board code 0x%016" PRIx64 " != expected 0x%016" PRIx64 " (XOR = 0x%016" PRIx64 ")\n", board->nonPawnHash[WHITE], temp.nonPawnHash[WHITE], xor);
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
        else if(newBoard->nonPawnHash[BLACK] != temp.nonPawnHash[BLACK])
        {
            uint64_t xor = temp.nonPawnHash[BLACK] ^ newBoard->nonPawnHash[BLACK];
            printf("Error! board code 0x%016" PRIx64 " != expected 0x%016" PRIx64 " (XOR = 0x%016" PRIx64 ")\n", board->nonPawnHash[BLACK], temp.nonPawnHash[BLACK], xor);
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