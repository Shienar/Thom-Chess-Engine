#include "transpositiontable.h"
#include "../debug.h"
#include <string.h>

hashtable_tt* transpositionTable = NULL;

hashtable_tt* create_hashTable_tt()
{
    hashtable_tt* newTable = CALLOC(1, sizeof(hashtable_tt));
    if(!newTable) return NULL;

    newTable->capacity = TT_TABLE_SIZE;

    newTable->array = CALLOC(newTable->capacity, sizeof(table_entry_tt));
    if(!newTable->array)
    {
        FREE(newTable);
        return NULL;
    }

    return newTable;
}

void destroy_hashTable_tt(hashtable_tt* ht)
{
    if(!ht) return;
    if(ht->array) FREE(ht->array);
    FREE(ht);
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
        if(entry.evaluationDepth > tt->array[index].evaluationDepth || (entry.nodeType == NODE_TYPE_PV && entry.nodeType != tt->array[index].nodeType)) tt->array[index] = entry;
        return;
    }

    tt->array[index] = entry;
    
}