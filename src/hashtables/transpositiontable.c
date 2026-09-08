#include "hashtables/transpositiontable.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include <string.h>

hashtable_tt* transpositionTable = NULL;
uint64_t tt_bytes = 1024 * 1024 * 256;

hashtable_tt* create_hashTable_tt()
{
    hashtable_tt* newTable = calloc(1, sizeof(hashtable_tt));
    if(!newTable) 
        return NULL;

    newTable->capacity = tt_bytes / sizeof(tt_entry);

    newTable->array = calloc(newTable->capacity, sizeof(tt_entry));
    if(!newTable->array)
    {
        free(newTable);
        return NULL;
    }

    return newTable;
}

void destroy_hashTable_tt(hashtable_tt* tt)
{
    if(!tt) return;
    if(tt->array) free(tt->array);
    free(tt);
}

int getHashFull(hashtable_tt* tt)
{
    int hits = 0;
    for(int i = 0; i < 1000; i++)
        if(tt->array[i].hashCode)
            hits++;

    return hits;
}

void clear_tt(hashtable_tt* tt)
{
    if(tt && tt->array) 
    {
        memset(tt->array, 0, tt->capacity * sizeof(tt_entry));
        tt->age = 0;
    }
}

void tt_age(hashtable_tt* tt) { tt->age++; }

void tt_prefetch(hashtable_tt* tt, uint64_t hashCode) {  __builtin_prefetch(&tt->array[hashCode%tt->capacity]); }

tt_entry transposition_table_get(bitboard* board, hashtable_tt* tt, uint8_t* hit, int ply)
{
    assert(tt);
    assert(board);

    uint64_t hashCode = board->hashCode;
    size_t index = hashCode%tt->capacity;

    tt_entry existingEntry = tt->array[index];

    if((existingEntry.hashCode ^ existingEntry.data) == hashCode)
    {
        *hit = 1;
        existingEntry.age = tt->age;
        existingEntry.hashCode = existingEntry.data ^ hashCode;
        tt->array[index] = existingEntry;

        tt_entry hitEntry = {
            .hashCode = hashCode,
            .data = existingEntry.data
        };

        if(hitEntry.evaluation > MIN_MATE_SCORE) hitEntry.evaluation -= ply;
        else if(hitEntry.evaluation < -MIN_MATE_SCORE) hitEntry.evaluation += ply;

        return hitEntry;
    }
    

    *hit = 0;
    return (tt_entry){0};
}

void transposition_table_set(hashtable_tt* tt, tt_entry entry, uint64_t hashCode64, int ply)
{
    assert(tt);
    entry.age = tt->age;
    
    entry.hashCode = hashCode64;

    size_t index = entry.hashCode%tt->capacity;

    tt_entry existingEntry = {
        .data = tt->array[index].data,
        .hashCode = tt->array[index].hashCode
    };

    if(existingEntry.hashCode)
    {
        //Don't overwrite existing with unknown nodes
        if(entry.nodeType == NODE_BOUND_UNKNOWN)
            return;
        
        //Replacement rules for same position.
        if(entry.hashCode == (existingEntry.hashCode ^ existingEntry.data))
        {
            if(existingEntry.depth >= entry.depth + 2)
            {
                if(!existingEntry.bestMove)
                    tt->array[index].bestMove = entry.bestMove;
                return;
            }
            
            if(!entry.bestMove)
                entry.bestMove = existingEntry.bestMove;
        }
    }

    if(entry.evaluation > MIN_MATE_SCORE) entry.evaluation += ply; 
    else if(entry.evaluation < -MIN_MATE_SCORE) entry.evaluation -= ply;

    //XOR used in place of checksum for lockless multithreaded access.
    tt->array[index].hashCode = entry.data ^ entry.hashCode;
    tt->array[index].data = entry.data;
}