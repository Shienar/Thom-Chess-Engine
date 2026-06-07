#include "transpositiontable.h"
#include "../debug.h"
#include "../board/bitboard.h"
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

uint8_t generateChecksum(table_entry_tt* entry)
{
    const uint8_t* ptr = (const uint8_t*)entry;
    uint8_t checksum = 0;
    for(int i = 0; i < sizeof(entry) - 1; i++)  checksum^=ptr[i];
    return checksum;
}

table_entry_tt* transposition_table_get(bitboard* board, hashtable_tt* tt)
{
    if(!board || !tt) return NULL;

    uint64_t hashCode = board->hashCode;
    size_t index = hashCode%tt->capacity;

    if(tt->array[index].hashCode == hashCode)
    {
        uint8_t checkSum = generateChecksum(&tt->array[index]);
        if(tt->array[index].checkSum == checkSum) return &tt->array[index];
    }

    return NULL;
}


void transposition_table_set(hashtable_tt* tt, table_entry_tt entry)
{
     assert(tt);

    size_t index = entry.hashCode%tt->capacity;

    (&entry)->checkSum = generateChecksum(&entry); 

    if(tt->array[index].hashCode == entry.hashCode && tt->array[index].checkSum == entry.checkSum)
    {
        //Update
        if(entry.depth >= tt->array[index].depth) tt->array[index] = entry;
        return;
    }

    tt->array[index] = entry;
    
}