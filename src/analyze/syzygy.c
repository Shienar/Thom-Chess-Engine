#include "analyze/syzygy.h"

void getFromPyrrhic(bitboard* board, move* dest, unsigned result)
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

    //lazy hotfix for a pyrrhic issue. g7h8 capture promotions recommended when there's no piece on h8.
    //Example: position startpos moves e2e4 c7c6 d2d4 d7d5 e4e5 c8f5 g1f3 e7e6 f1e2 b8d7 e1g1 h7h6 c2c3 g8e7 b1d2 d8c7 f1e1 e8c8 a2a4 g7g5 d2f1 f7f6 e5f6 e7g6 a4a5 d7f6 f1g3 a7a6 f3e5 g6e5 d4e5 f6e4 c1e3 f8c5 g3f5 e6f5 e3c5 e4c5 e2h5 c7e7 d1c2 h8f8 b2b4 c5e6 h5e2 g5g4 a1c1 h6h5 c2a4 e6c7 c3c4 d5c4 e2c4 f5f4 e5e6 h5h4 c1d1 g4g3 d1d8 f8d8 a4a3 d8d2 h2g3 h4g3 f2g3 e7g7 a3c1 g7d4 g1h2 d4f2 c4f1 d2d5 g3g4 f4f3 c1e3 f2h4 h2g1 d5e5 e3f2 h4f2 g1f2 e5e1 f2e1 f3g2 f1g2 c7e6 e1d1 c6c5 g2h1 c5b4 d1c1 b7b6 a5b6 a6a5 c1d1 e6g5 d1e1 b4b3 h1d5 b3b2 d5a2 a5a4 a2b1 a4a3 b6b7 c8b7 e1f1 b7c6 f1e1 g5f3 e1d1 c6c5 d1c2 c5b4 c2d3 f3g5 d3e3 b4b3 e3f4 a3a2 b1a2 b3a2 f4g5 b2b1q g5h6 a2b3 g4g5 b1f5 g5g6 f5f6 h6h7 f6f8 g6g7 f8f5 h7h8 f5h3 h8g8 b3c4
    if(ISPAWN(piece))
    {
        int difference = abs(dest->endSquare - dest->startSquare);
        if((difference == 7 || difference == 9) && findPieceOnSquare(board, dest->endSquare) == EMPTY_PIECE)
            dest->raw = 0;
    }
}

void filterSyzygyMoves(bitboard* board, move* requiredMoves)
{
    //3-n man syzygy endgame with no castling rights.
    if(__builtin_popcountll(board->pieces_all) > syzygyProbeLimit || board->in_check) return;

    //Don't probe syzygy if the GUI assigned required search moves.
    if(IS_VALID_MOVE(requiredMoves[0])) return;

    unsigned moveResults[TB_MAX_MOVES];
    unsigned result = tb_probe_root(board->pieces_side[WHITE],                                  board->pieces_side[BLACK], 
                                    board->pieces[BLACK_KING]   | board->pieces[WHITE_KING],    board->pieces[BLACK_QUEEN]  | board->pieces[WHITE_QUEEN], 
                                    board->pieces[BLACK_ROOK]   | board->pieces[WHITE_ROOK],    board->pieces[BLACK_BISHOP] | board->pieces[WHITE_BISHOP],
                                    board->pieces[BLACK_KNIGHT] | board->pieces[WHITE_KNIGHT],  board->pieces[BLACK_PAWN]   | board->pieces[WHITE_PAWN],
                                    (unsigned) board->halfmoveClock,
                                    (board->enPassantSquare == NO_EP_SQUARE) ? 0 : board->enPassantSquare, 
                                    !board->turn,
                                    moveResults);
    if(result == TB_RESULT_FAILED)
    {
        DEBUG_ERROR("Failed to probe syzygy move.");
        return;
    }
    else if(result == TB_RESULT_CHECKMATE || result == TB_RESULT_STALEMATE) return;

    getFromPyrrhic(board, &requiredMoves[0], result);
    int insertIndex = 1;
    if(!IS_VALID_MOVE(requiredMoves[insertIndex - 1])) insertIndex--;

    for(int i = 0; i < TB_MAX_MOVES && insertIndex < MAX_REQUIRED_MOVES; i++)
    {
        if(moveResults[i] == TB_RESULT_FAILED) break;
        
        if(TB_GET_WDL(moveResults[i]) == TB_GET_WDL(result) && TB_GET_DTZ(moveResults[i]) <= TB_GET_DTZ(result))
        {
            getFromPyrrhic(board, &requiredMoves[insertIndex++], moveResults[i]);
            if(!IS_VALID_MOVE(requiredMoves[insertIndex - 1])) insertIndex--;
        }
    }
}

//Return -1 on error. Valid results are SCORE_WIN, -SCORE_WIN, 0
int getSyzygyResult(bitboard* board)
{
    //3-n man syzygy endgame with no castling rights.
    if(__builtin_popcountll(board->pieces_all) > syzygyProbeLimit || board->in_check) return -1;

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
            DEBUG_ERROR("Failed to probe syzygy result.");
        default:
            return -1;
    }
}
