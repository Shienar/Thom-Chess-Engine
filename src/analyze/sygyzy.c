#include "analyze/sygyzy.h"

void getFromPyrrhic(bitboard* board, move_c* dest, unsigned result)
{
    dest->endSquare = TB_RESULT_TO(result);
    dest->startSquare = TB_RESULT_FROM(result);
    int piece = findPieceOnSquare(board, dest->startSquare);

    if(!ISPAWN(piece)) dest->promoteTo = 0;
    else if(TB_RESULT_IS_QPROMO(result)) dest->promoteTo = QUEEN;
    else if(TB_RESULT_IS_RPROMO(result)) dest->promoteTo = ROOK;
    else if(TB_RESULT_IS_BPROMO(result)) dest->promoteTo = BISHOP;
    else if(TB_RESULT_IS_NPROMO(result)) dest->promoteTo = KNIGHT;
    else dest->promoteTo = 0;
}

void filterSygyzyMoves(bitboard* board, move_c* requiredMoves)
{
    //3-n man sygyzy endgame with no castling rights.
    if(__builtin_popcountll(board->pieces_all) > sygyzyProbeLimit || (IS_IN_CHECK_ANY(board->flags))) return;

    //Don't probe sygyzy if the GUI assigned required search moves.
    if(IS_VALID_MOVE(requiredMoves[0])) return;

    unsigned moveResults[TB_MAX_MOVES];
    unsigned result = tb_probe_root(board->pieces_side[WHITE],                                  board->pieces_side[BLACK], 
                                    board->pieces[BLACK_KING]   | board->pieces[WHITE_KING],    board->pieces[BLACK_QUEEN]  | board->pieces[WHITE_QUEEN], 
                                    board->pieces[BLACK_ROOK]   | board->pieces[WHITE_ROOK],    board->pieces[BLACK_BISHOP] | board->pieces[WHITE_BISHOP],
                                    board->pieces[BLACK_KNIGHT] | board->pieces[WHITE_KNIGHT],  board->pieces[BLACK_PAWN]   | board->pieces[WHITE_PAWN],
                                    (unsigned) board->movesSinceLastChange,
                                    (board->enPassantSquare == -1) ? 0 : board->enPassantSquare, 
                                    !board->turn,
                                    moveResults);
    if(result == TB_RESULT_FAILED)
    {
        DEBUG_ERROR("Failed to probe sygyzy move.");
        return;
    }
    else if(result == TB_RESULT_CHECKMATE || result == TB_RESULT_STALEMATE) return;

    getFromPyrrhic(board, &requiredMoves[0], result);
    int insertIndex = 1;
    for(int i = 0; i < TB_MAX_MOVES && insertIndex < MAX_REQUIRED_MOVES; i++)
    {
        if(moveResults[i] == TB_RESULT_FAILED) break;
        
        if(TB_GET_WDL(moveResults[i]) == TB_GET_WDL(result) && TB_GET_DTZ(moveResults[i]) <= TB_GET_DTZ(result))
        {
            getFromPyrrhic(board, &requiredMoves[insertIndex++], moveResults[i]);
        }
    }
}

//Return -1 on error. Valid results are SCORE_WIN, -SCORE_WIN, 0
int getSygyzyResult(bitboard* board)
{
    //3-n man sygyzy endgame with no castling rights.
    if(__builtin_popcountll(board->pieces_all) > sygyzyProbeLimit || (IS_IN_CHECK_ANY(board->flags))) return -1;

    uint32_t ep = board->enPassantSquare;
    if(ep == -1) ep = 0;

    bool turn = PYRRHIC_WHITE;
    if(ISBLACK(board->turn)) turn = PYRRHIC_BLACK;

    int result = tb_probe_wdl(board->pieces_side[WHITE], board->pieces_side[BLACK], 
                                    board->pieces[BLACK_KING]|board->pieces[WHITE_KING], board->pieces[BLACK_QUEEN]|board->pieces[WHITE_QUEEN], 
                                    board->pieces[BLACK_ROOK]|board->pieces[WHITE_ROOK], board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP],
                                    board->pieces[BLACK_KNIGHT]|board->pieces[WHITE_KNIGHT], board->pieces[BLACK_PAWN]|board->pieces[WHITE_PAWN],
                                    ep, turn);
    switch(result)
    {
        case TB_LOSS:
            return -SCORE_WIN;
        case TB_WIN:
            return SCORE_WIN;
        case TB_BLESSED_LOSS:
        case TB_CURSED_WIN:
        case TB_DRAW:
            return 0;
        case TB_RESULT_FAILED:
            DEBUG_ERROR("Failed to probe sygyzy result.");
        default:
            return -1;
    }
}
