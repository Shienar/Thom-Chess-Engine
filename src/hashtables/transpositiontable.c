#include "hashtables/transpositiontable.h"
#include "debug.h"
#include "board/bitboard.h"
#include <string.h>

hashtable_tt* transpositionTable = NULL;
uint64_t tt_size_entries = (1024 * 1024 * 4) / sizeof(table_entry_tt);

hashtable_tt* create_hashTable_tt()
{
    hashtable_tt* newTable = calloc(1, sizeof(hashtable_tt));
    if(!newTable) return NULL;

    newTable->capacity = tt_size_entries;

    newTable->array = calloc(newTable->capacity, sizeof(table_entry_tt));
    if(!newTable->array)
    {
        free(newTable);
        return NULL;
    }

    return newTable;
}

void destroy_hashTable_tt(hashtable_tt* ht)
{
    if(!ht) return;
    if(ht->array) free(ht->array);
    free(ht);
}


void clear_tt(hashtable_tt* tt)
{
    if(tt && tt->array) memset(tt->array, 0, tt->capacity * sizeof(table_entry_tt));
}

table_entry_tt transposition_table_get(bitboard* board, hashtable_tt* tt, uint8_t* hit, int ply)
{
    if(board && tt)
    {
        uint64_t hashCode = board->hashCode;
        size_t index = hashCode%tt->capacity;

        uint64_t existingHash = tt->array[index].hashCode;
        uint64_t existingData = tt->array[index].data;

        if((existingHash ^ existingData) == hashCode)
        {
            *hit = 1;
            table_entry_tt hitEntry = {
                .hashCode = hashCode,
                .data = existingData
            };

            if (hitEntry.evaluation > MIN_MATE_SCORE) hitEntry.evaluation -= ply;
            else if (hitEntry.evaluation < -MIN_MATE_SCORE) hitEntry.evaluation += ply;

            return hitEntry;
        }
    }

    *hit = 0;
    return (table_entry_tt){0};
}


void transposition_table_set(hashtable_tt* tt, table_entry_tt entry, int ply)
{
    assert(tt);

    size_t index = entry.hashCode%tt->capacity;

    table_entry_tt existingEntry = {
        .data = tt->array[index].data
    };

    if(entry.age < existingEntry.age + entry.depth || entry.depth < existingEntry.depth) return;

    if(entry.evaluation > MIN_MATE_SCORE) entry.evaluation += ply; 
    else if(entry.evaluation < -MIN_MATE_SCORE)  entry.evaluation -= ply;

    //XOR used in place of checksum for lockless multithreaded access.
    tt->array[index].hashCode = entry.data ^ entry.hashCode;
    tt->array[index].data = entry.data;
}