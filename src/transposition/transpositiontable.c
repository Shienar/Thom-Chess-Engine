#include "transposition/transpositiontable.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include <string.h>

hashtable_tt* transpositionTable = NULL;
uint64_t tt_bytes = 1024 * 1024 * 256;

const uint64_t indexMask = singleBitMask(48) - 1;

hashtable_tt* create_hashTable_tt()
{
    hashtable_tt* newTable = calloc(1, sizeof(hashtable_tt));
    if(!newTable) 
        return NULL;

    newTable->capacity = tt_bytes / sizeof(tt_bucket);

    newTable->array = calloc(newTable->capacity, sizeof(tt_bucket));
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

//How much older is age1 compared to age2, assuming that age1 is older.
uint8_t getAgeDifference(uint8_t age1, uint8_t age2)
{
    return (age1 - age2) & TT_AGE_MASK;
}

int getHashFull(hashtable_tt* tt)
{
    int hits = 0;
    for(int b = 0; b < 1000; b++)
        for(int i = 0; i < TT_ENTRIES_PER_BUCKET; i++)
            if(tt->array[b].entries[i].data && tt->age == tt->array[b].entries[i].age)
                hits++;

    return hits / TT_ENTRIES_PER_BUCKET;
}

void clear_tt(hashtable_tt* tt)
{
    if(tt && tt->array) 
    {
        memset(tt->array, 0, tt->capacity * sizeof(tt_bucket));
        tt->age = 0;
    }
}

void tt_age(hashtable_tt* tt) { tt->age++; }

void tt_prefetch(hashtable_tt* tt, uint64_t hashCode) {  __builtin_prefetch(&tt->array[hashCode%tt->capacity]); }

tt_entry transposition_table_get(bitboard* board, hashtable_tt* tt, uint8_t* hit, int ply)
{
    assert(tt);
    assert(board);

    uint64_t hashMSB = board->hashCode >> 48;
    size_t index = (board->hashCode&indexMask)%tt->capacity;

    tt_entry* bucket_entries = tt->array[index].entries;

    for(int i = 0; i < TT_ENTRIES_PER_BUCKET && bucket_entries[i].data; i++)
    {
        if(bucket_entries[i].hashMSB == hashMSB)
        {
            *hit = 1;
            tt->array[index].entries[i].age = tt->age;

            if(bucket_entries[i].evaluation >= MIN_MATE_SCORE) bucket_entries[i].evaluation -= ply;
            else if(bucket_entries[i].evaluation <= -MIN_MATE_SCORE) bucket_entries[i].evaluation += ply;

            return bucket_entries[i];
        }
    }
    
    *hit = 0;
    return (tt_entry){0};
}

void transposition_table_set(hashtable_tt* tt, tt_entry entry, uint64_t hashCode64, int ply)
{
    assert(tt);

    entry.age = tt->age;
    entry.hashMSB = hashCode64 >> 48;

    size_t index = (hashCode64&indexMask)%tt->capacity;

    tt_entry* bucket_entries = tt->array[index].entries;
    tt_entry* existingEntry = bucket_entries;

    for(int i = 0; i < TT_ENTRIES_PER_BUCKET; i++)
    {
        //Empty bucket or matching hash code
        if(!bucket_entries[i].data || bucket_entries[i].hashMSB == entry.hashMSB)
        {
            existingEntry = &bucket_entries[i];
            break;
        }
        //Existing entry for different position
        else if(bucket_entries[i].depth - getAgeDifference(tt->age, bucket_entries[i].age) + 2 * (bucket_entries[i].nodeType == NODE_BOUND_EXACT) < 
                existingEntry->depth - getAgeDifference(tt->age, existingEntry->age) + 2 * (existingEntry->nodeType == NODE_BOUND_EXACT))
                    existingEntry = &bucket_entries[i];
    }

    if(existingEntry->data)
    {
        //Don't overwrite existing with unknown nodes, unless they are from an older search
        if(entry.nodeType == NODE_BOUND_UNKNOWN)
            return;

        //Same position replacement rules
        if(entry.hashMSB == existingEntry->hashMSB)
        {
            if(existingEntry->depth > entry.depth + 2)
            {
                if(!existingEntry->bestMove.raw)
                    existingEntry->bestMove = entry.bestMove;
                return;
            }
            
            if(!entry.bestMove.raw)
                entry.bestMove = existingEntry->bestMove;
        }
    }


    if(entry.evaluation >= MIN_MATE_SCORE) entry.evaluation += ply; 
    else if(entry.evaluation <= -MIN_MATE_SCORE) entry.evaluation -= ply;
    existingEntry->data = entry.data;
}