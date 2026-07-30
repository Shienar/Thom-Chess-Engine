#include "board/magic.h"
#include "debug.h"

magic bishopMagics[64] = {0};
magic rookMagics[64]= {0};
uint64_t rookTable[102400]= {0};
uint64_t bishopTable[20480]= {0};

uint64_t rookMagicNumbers[64] = {
        0x80004000976080ULL,    0x1040400010002000ULL,  0x4880200210000980ULL,  0x5280080010000482ULL,
        0x200040200081020ULL,   0x2100080100020400ULL,  0x4280008001000200ULL,  0x1000a4425820300ULL,
        0x29002100800040ULL,    0x4503400040201004ULL,  0x209002001004018ULL,   0x1131000a10002100ULL,
        0x9000800120500ULL,     0x10e001804820010ULL,   0x29000402000100ULL,    0x2002000d01c40292ULL,
        0x80084000200c40ULL,    0x10004040002002ULL,    0x201030020004014ULL,   0x80012000a420020ULL,
        0x129010008001204ULL,   0x6109010008040002ULL,  0x950010100020004ULL,   0x803a0000c50284ULL,
        0x80004100210080ULL,    0x200240100140ULL,      0x20004040100800ULL,    0x4018090300201000ULL,
        0x4802010a00102004ULL,  0x2001000900040002ULL,  0x4a02104001002a8ULL,   0x2188108200204401ULL,
        0x40400020800080ULL,    0x880402000401004ULL,   0x10040800202000ULL,    0x604410a02001020ULL,
        0x200200206a001410ULL,  0x86000400810080ULL,    0x428200040600080bULL,  0x2001000041000082ULL,
        0x80002000484000ULL,    0x210002002c24000ULL,   0x401a200100410014ULL,  0x5021000a30009ULL,
        0x218000509010010ULL,   0x4000400410080120ULL,  0x20801040010ULL,       0x29040040820011ULL,
        0x4080400024800280ULL,  0x500200040100440ULL,   0x2880142001004100ULL,  0x412020400a001200ULL,
        0x18c028004080080ULL,   0x884001020080401ULL,   0x210810420400ULL,      0x801048745040200ULL,
        0x4401002040120082ULL,  0x408200210012ULL,      0x110008200441ULL,      0x2010002004100901ULL,
        0x801000800040211ULL,   0x480d000400820801ULL,  0x820104201280084ULL,   0x1001040311802142ULL};

uint64_t bishopMagicNumbers[64] = {
        0x1024b002420160ULL,    0x1008080140420021ULL,  0x2012080041080024ULL,  0xc282601408c0802ULL,
        0x2004042000000002ULL,  0x12021004022080ULL,    0x880414820100000ULL,   0x4501002211044000ULL,
        0x20402222121600ULL,    0x1081088a28022020ULL,  0x1004c2810851064ULL,   0x2040080841004918ULL,
        0x1448020210201017ULL,  0x4808110108400025ULL,  0x10504404054004ULL,    0x800010422092400ULL,
        0x40000870450250ULL,    0x402040408080518ULL,   0x1000980a404108ULL,    0x1020804110080ULL,
        0x8200c02082005ULL,     0x40802009a0800ULL,     0x1000201012100ULL,     0x111080200820180ULL,
        0x904122104101024ULL,   0x4008200405244084ULL,  0x44040002182400ULL,    0x4804080004021002ULL,
        0x6401004024004040ULL,  0x404010001300a20ULL,   0x428020200a20100ULL,   0x300460100420200ULL,
        0x404200c062000ULL,     0x22101400510141ULL,    0x104044400180031ULL,   0x2040040400280211ULL,
        0x8020400401010ULL,     0x20100110401a0040ULL,  0x100101005a2080ULL,    0x1a008300042411ULL,
        0x120a025004504000ULL,  0x4001084242101000ULL,  0xa020202010a4200ULL,   0x4000002018000100ULL,
        0x80104000044ULL,       0x1004009806004043ULL,  0x100401080a000112ULL,  0x1041012101000608ULL,
        0x40400c250100140ULL,   0x80a10460a100002ULL,   0x2210030401240002ULL,  0x6040aa108481b20ULL,
        0x4009004050410002ULL,  0x8106003420200e0ULL,   0x1410500a08206000ULL,  0x92548802004000ULL,
        0x1040041241028ULL,     0x120042025011ULL,      0x8060104054400ULL,     0x20004404020a0a01ULL,
        0x40008010020214ULL,    0x4000050209802c1ULL,   0x208244210400ULL,      0x10140848044010ULL};

uint64_t naiveBishop(int square, uint64_t occupancy, int useOccupancy)
{
    uint64_t potentialMoves = 0;

    int row = getRow(square); 
    int column = getColumn(square);
    
    if(useOccupancy)
    {
        //Topleft
        for(int r = row + 1, c = column - 1; r <= 7 && c >= 0; r++, c--)
        {
            potentialMoves |= singleBitMask(8 * r + c);
            if(singleBitMask(8 * r + c) & occupancy) break;
        }

        //Topright
        for(int r = row + 1, c = column + 1; r <= 7 && c <= 7; r++, c++)
        {
            potentialMoves |= singleBitMask(8 * r + c);
            if(singleBitMask(8 * r + c) & occupancy) break;
        }

        //Bottomleft
        for(int r = row - 1, c = column - 1; r >= 0 && c >= 0; r--, c--)
        {
            potentialMoves |= singleBitMask(8 * r + c);
            if(singleBitMask(8 * r + c) & occupancy) break;
        }

        //Bottomright
        for(int r = row - 1, c = column + 1; r >= 0 && c <= 7; r--, c++)
        {
            potentialMoves |= singleBitMask(8 * r + c);
            if(singleBitMask(8 * r + c) & occupancy) break;
        }
    }
    else
    {
        //Topleft
        for(int r = row + 1, c = column - 1; r <= 6 && c >= 1; r++, c--) 
            potentialMoves |= singleBitMask(8 * r + c);

        //Topright
        for(int r = row + 1, c = column + 1; r <= 6 && c <= 6; r++, c++) 
            potentialMoves |= singleBitMask(8 * r + c);

        //Bottomleft
        for(int r = row - 1, c = column - 1; r >= 1 && c >= 1; r--, c--) 
            potentialMoves |= singleBitMask(8 * r + c);

        //Bottomright
        for(int r = row - 1, c = column + 1; r >= 1 && c <= 6; r--, c++) 
            potentialMoves |= singleBitMask(8 * r + c);
    }

    return potentialMoves;
}

uint64_t naiveRook(int square, uint64_t occupancy, int useOccupancy)
{
    uint64_t potentialMoves = 0;

    int row = getRow(square); 
    int column = getColumn(square);
        
    if(useOccupancy)
    {
        //Right
        for(int c = column + 1; c <= 7; c++)
        {
            potentialMoves |= singleBitMask(8 * row + c);
            if(singleBitMask(8 * row + c) & occupancy) break;
        }

        //Left
        for(int c = column - 1; c >= 0; c--)
        {
            potentialMoves |= singleBitMask(8 * row + c);
            if(singleBitMask(8 * row + c) & occupancy) break;
        }

        //Above
        for(int r = row + 1; r <= 7; r++)
        {
            potentialMoves |= singleBitMask(8 * r + column);
            if(singleBitMask(8 * r + column) & occupancy) break;
        }

        //Below
        for(int r = row - 1; r >= 0; r--)
        {
            potentialMoves |= singleBitMask(8 * r + column);
            if(singleBitMask(8 * r + column) & occupancy) break;
        }
    }
    else
    {
        //Right
        for(int c = column + 1; c <= 6; c++) 
            potentialMoves |= singleBitMask(8 * row + c);

        //Left
        for(int c = column - 1; c >= 1; c--) 
            potentialMoves |= singleBitMask(8 * row + c);

        //Above
        for(int r = row + 1; r <= 6; r++) 
            potentialMoves |= singleBitMask(8 * r + column);

        //Below
        for(int r = row - 1; r >= 1; r--) 
            potentialMoves |= singleBitMask(8 * r + column);
    }

    return potentialMoves;
}

uint64_t createOccupancy(int index, uint64_t potentialMoveMask)
{
    /**
     * potentialMoveMask has N set bits
     * there are 2^N indexes, one for each combination of occupancies.
     * 
     * Need to create a unique occupancy for each index.
     * 
     * If we only look at the set bits in potentialMoveMask, you
     * can create 2^N numbers using those N set bits.
     */

    uint64_t occupancy = 0;
    int count = __builtin_popcountll(potentialMoveMask);

    for(int i = 0; i < count; i++)
    {
        int sq = __builtin_ctzll(potentialMoveMask);

        potentialMoveMask &= (potentialMoveMask - 1);

        if(index & (1 << i))
            occupancy|= singleBitMask(sq);
    }

    return occupancy;
}

void initMagics()
{
    uint64_t* startIndex_rook = &rookTable[0];
    uint64_t* startIndex_bishop = &bishopTable[0];

    for(int square = 0; square < 64; square++)
    {
        uint64_t rookMask = naiveRook(square, 0, 0);
        uint64_t bishopMask = naiveBishop(square, 0, 0);
        
        int setBits_rook = __builtin_popcountll(rookMask);
        int setBits_bishop = __builtin_popcountll(bishopMask);

        int occ_rook = 1 << setBits_rook;
        int occ_bishop = 1 << setBits_bishop;

        magic* rookMagic = &rookMagics[square];
        magic* bishopMagic = &bishopMagics[square];

        rookMagic->attacks = startIndex_rook;
        bishopMagic->attacks = startIndex_bishop;

        rookMagic->mask = rookMask;
        bishopMagic->mask = bishopMask;

        rookMagic->magic = rookMagicNumbers[square];
        bishopMagic->magic = bishopMagicNumbers[square];

        rookMagic->shiftOffset = 64 - setBits_rook;
        bishopMagic->shiftOffset = 64 - setBits_bishop;

        for(int i = 0; i < occ_rook; i++)
        {
            uint64_t occ = createOccupancy(i, rookMagic->mask);
            uint64_t attacks = naiveRook(square, occ, 1);
            int magicIndex = (occ * rookMagic->magic) >> rookMagic->shiftOffset;

            //collision
            assert(!rookMagic->attacks[magicIndex]);

            rookMagic->attacks[magicIndex] = attacks;
        }
        startIndex_rook += occ_rook;
        
        for(int i = 0; i < occ_bishop; i++)
        {
            uint64_t occ = createOccupancy(i, bishopMagic->mask);
            uint64_t attacks = naiveBishop(square, occ, 1);
            int magicIndex = (occ * bishopMagic->magic) >> bishopMagic->shiftOffset;

            //collision
            assert(!bishopMagic->attacks[magicIndex]);

            bishopMagic->attacks[magicIndex] = attacks;
        }
        startIndex_bishop += occ_bishop;
    }
}

uint64_t pawnAttacks[2][64];
void initPawnAttacks()
{
    for(int square = 0; square < 64; square++)
    {
        pawnAttacks[WHITE][square] = 0;
        pawnAttacks[BLACK][square] = 0;

        int row = getRow(square);
        int column = getColumn(square);

        if(row < 7)
        {
            if(column > 0) pawnAttacks[WHITE][square] |= singleBitMask(square+7);
            if(column < 7) pawnAttacks[WHITE][square] |= singleBitMask(square+9);
        }

        if(row > 0)
        {
            if(column > 0) pawnAttacks[BLACK][square] |= singleBitMask(square-9);
            if(column < 7) pawnAttacks[BLACK][square] |= singleBitMask(square-7);
        }
    }
}

uint64_t knightAttacks[64];
void initKnightMoveTable()
{
    for(int square = 0; square < 64; square++)
    {
        knightAttacks[square] = 0;
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

        if(column - 2 >= 0 && row + 1 <= 7) 
            knightAttacks[square]|=singleBitMask(square+6);
        if(column - 1 >= 0 && row + 2 <= 7) 
            knightAttacks[square]|=singleBitMask(square+15);
        if(column + 1 <= 7 && row + 2 <= 7) 
            knightAttacks[square]|=singleBitMask(square+17);
        if(column + 2 <= 7 && row + 1 <= 7)
            knightAttacks[square]|=singleBitMask(square+10);
        if(column + 2 <= 7 && row - 1 >= 0)
            knightAttacks[square]|=singleBitMask(square-6);
        if(column + 1 <= 7 && row - 2 >= 0)
            knightAttacks[square]|=singleBitMask(square-15);
        if(column - 1 >= 0 && row - 2 >= 0)
            knightAttacks[square]|=singleBitMask(square-17);
        if(column - 2 >= 0 && row - 1 >= 0)
            knightAttacks[square]|=singleBitMask(square-10);
    }
}

uint64_t kingAttacks[64];
void initKingMoveTable()
{
    
    for(int square = 0; square < 64; square++)
    {
        kingAttacks[square] = 0;
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
        if(column - 1 >= 0 && row + 1 <= 7) 
            kingAttacks[square]|=singleBitMask(square+7);
        if(row + 1 <= 7) 
            kingAttacks[square]|=singleBitMask(square+8);
        if(column + 1 <= 7 && row + 1 <= 7) 
            kingAttacks[square]|=singleBitMask(square+9);
        if(column - 1 >= 0) 
            kingAttacks[square]|=singleBitMask(square-1);
        if(column + 1 <= 7) 
            kingAttacks[square]|=singleBitMask(square+1);
        if(column - 1 >= 0 && row - 1 >= 0) 
            kingAttacks[square]|=singleBitMask(square-9);
        if(row - 1 >= 0) 
            kingAttacks[square]|=singleBitMask(square-8);
        if(column + 1 <= 7 && row - 1 >= 0) 
            kingAttacks[square]|=singleBitMask(square-7);
    }
}