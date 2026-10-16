#include "analyze/cuckoo.h"
#include "board/bitboard.h"
#include "board/moves.h"

uint64_t betweenSquares[64][64];
uint64_t cuckooHashes[CUCKOO_SIZE] = {0};
move     cuckooMoves[CUCKOO_SIZE]  = {0};

//Also initializes betweenSquares since only cuckootables use them
void initCuckooMoveTable()
{
    memset(cuckooHashes, 0, sizeof(cuckooHashes));
    memset(cuckooMoves, 0, sizeof(cuckooMoves));

    uint64_t stm_hash = zobrist_keys[780];
    for(int pc = WHITE_KNIGHT; pc <= BLACK_KING; pc++)
    {
        for(int from = 0; from < 64; from++)
        {
            uint64_t attacks;
            switch(PIECE(pc))
            {
                case KNIGHT:
                    attacks = knightMoves(0, from);
                    break;
                case BISHOP:
                    attacks = bishopMoves(0, 0, from);
                    break;
                case ROOK:
                    attacks = rookMoves(0, 0, from);
                    break;
                case QUEEN:
                    attacks = queenMoves(0, 0, from);
                    break;
                case KING:
                    //Castling moves aren't reversible or symmetrical.
                    attacks = kingAttacks[from];
                    break;
                default:
                    attacks = 0;
                    break;
            }
            //Non-pawn moves are symmetrical in from/to.
            for(int to = from + 1; to < 64; to++)
            {
                if(!(attacks & singleBitMask(to)))
                    continue;
                
                move m = {
                    .startSquare = from, 
                    .endSquare = to
                };
                uint64_t moveHash = zobrist_piece_keys[pc][from] ^ zobrist_piece_keys[pc][to] ^ stm_hash;

                int index = CUCKOO_HASH1(moveHash);

                while(1)
                {
                    uint64_t tempHash = cuckooHashes[index];
                    cuckooHashes[index] = moveHash;
                    moveHash = tempHash;

                    move tempMove = cuckooMoves[index];
                    cuckooMoves[index] = m;
                    m = tempMove;

                    if(!m.raw)
                        break;
                    
                    index = (index == CUCKOO_HASH1(moveHash)) ? CUCKOO_HASH2(moveHash) : CUCKOO_HASH1(moveHash);
                }
            }
        }
    }

    for(int a = 0; a < 64; a++)
    {
        for(int b = 0; b < 64; b++)
        {
            if(getRow(a) == getRow(b) || getColumn(a) == getColumn(b))
                betweenSquares[a][b] = rookMoves(singleBitMask(b), 0, a) & rookMoves(singleBitMask(a), 0, b);
            else if(bishopMoves(0, 0, a) & singleBitMask(b))
                betweenSquares[a][b] = bishopMoves(singleBitMask(b), 0, a) & bishopMoves(singleBitMask(a), 0, b);
        }
    }

}

int isUpcomingRepetition(bitboard* board, repetitionVector repetitions, int ply)
{
    int halfmoves = _min(board->halfmoveClock, board->pliesFromNull);

    if(halfmoves < 3)
        return 0;
 
    uint64_t turnHash = zobrist_keys[780];

    uint64_t originalHash = repetitions.hashCodes[board->halfMoveCount]; 
    uint64_t prevHash     = repetitions.hashCodes[board->halfMoveCount - 1];

    uint64_t other = originalHash ^ prevHash ^ turnHash;

    for(int i = 3; i <= halfmoves; i+=2)
    {
        uint64_t currentHash = repetitions.hashCodes[board->halfMoveCount - i];
        other ^= currentHash ^ repetitions.hashCodes[board->halfMoveCount - (i - 1)] ^ turnHash;

        if(!other) continue;

        uint64_t diff = originalHash ^ currentHash;
        int cuckooIndex = CUCKOO_HASH1(diff);
        if(diff != cuckooHashes[cuckooIndex])
            cuckooIndex = CUCKOO_HASH2(diff);
        if(diff != cuckooHashes[cuckooIndex])
            continue;
        
        move m = cuckooMoves[cuckooIndex];
        
        if(!(betweenSquares[m.startSquare][m.endSquare] & board->pieces_all))
        {
            if(ply > i)
                return 1;

            int pc = findPieceOnSquare(board, m.startSquare);
            if(pc == EMPTY_PIECE)
                pc = findPieceOnSquare(board, m.endSquare);
            return COLOR(pc) == board->turn;
        }
    }
    return 0;
}