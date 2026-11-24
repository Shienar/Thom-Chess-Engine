#include "transposition/transpositiontable.h"
#include "analyze/search.h"
#include <stdint.h>
#include <stdlib.h>

hashtable_tt * transposition_table = NULL;
uint64_t tt_bytes = 1024 * 1024 * 256;

const uint64_t index_mask = (1ull << (48)) - 1;

hashtable_tt *create_tt_table(void)
{
	hashtable_tt *new_table = calloc(1, sizeof(hashtable_tt));

	if (!new_table)
		return NULL;

	new_table->capacity = tt_bytes / sizeof(tt_bucket);

	new_table->array = calloc(new_table->capacity, sizeof(tt_bucket));
	if (!new_table->array) {
		free(new_table);
		return NULL;
	}

	return new_table;
}

void destroy_tt_table(hashtable_tt *tt)
{
	if (!tt)
		return;
	if (tt->array)
		free(tt->array);
	free(tt);
}

//How much older is age1 compared to age2, assuming that age1 is older.
uint8_t get_age_difference(uint8_t age1, uint8_t age2)
{
	return (age1 - age2) & TT_AGE_MASK;
}

int get_tt_hashfull(hashtable_tt *tt)
{
	int hits = 0;

	for (int b = 0; b < 1000; b++)
		for (int i = 0; i < TT_ENTRIES_PER_BUCKET; i++)
			if (tt->array[b].entries[i].data && tt->age == tt->array[b].entries[i].age)
				hits++;

	return hits / TT_ENTRIES_PER_BUCKET;
}

void clear_tt(hashtable_tt *tt)
{
	if (tt && tt->array) {
		memset(tt->array, 0, tt->capacity * sizeof(tt_bucket));
		tt->age = 0;

		#ifdef SEARCHINFO
		tt->overwrites = 0;
		tt->collisions = 0;
		tt->shallow_collisions = 0;
		tt->fills = 0;
		#endif
	}
}

void tt_age(hashtable_tt *tt)
{
	tt->age++;
}

void tt_prefetch(hashtable_tt *tt, uint64_t hash_code)
 {
	__builtin_prefetch(&tt->array[hash_code%tt->capacity]);
}

tt_entry tt_get(bitboard *board, hashtable_tt *tt, uint8_t *hit, int ply)
{
	assert(tt);
	assert(board);

	uint64_t hash_msb = board->hash_code >> 48;
	size_t index = (board->hash_code&index_mask)%tt->capacity;

	tt_entry *bucket_entries = tt->array[index].entries;

	for (int i = 0; i < TT_ENTRIES_PER_BUCKET && bucket_entries[i].data; i++) {
		if (bucket_entries[i].hash_msb == hash_msb) {
			*hit = 1;
			tt->array[index].entries[i].age = tt->age;

			//Create a copy to make evaluation editing safe.
			tt_entry returned_entry = bucket_entries[i];

			if (returned_entry.evaluation >= MIN_MATE_SCORE)
				returned_entry.evaluation -= ply;
			else if (returned_entry.evaluation <= -MIN_MATE_SCORE)
				returned_entry.evaluation += ply;

			return returned_entry;
		}
	}

	*hit = 0;
	return (tt_entry){0};
}

void tt_set(hashtable_tt *tt, tt_entry entry, uint64_t hash_code64, int ply)
{
	assert(tt);

	entry.age = tt->age;
	entry.hash_msb = hash_code64 >> 48;

	size_t index = (hash_code64&index_mask)%tt->capacity;

	tt_entry *bucket_entries = tt->array[index].entries;
	tt_entry *existing_entry = bucket_entries;

	for (int i = 0; i < TT_ENTRIES_PER_BUCKET; i++) {
		//Empty bucket or matching hash code
		if (!bucket_entries[i].data || bucket_entries[i].hash_msb == entry.hash_msb) {
			existing_entry = &bucket_entries[i];
			break;
		}
		//Existing entry for different position
		else if (bucket_entries[i].depth - get_age_difference(tt->age, bucket_entries[i].age) + 2 * (bucket_entries[i].node_type == NODE_BOUND_EXACT) <
				existing_entry->depth - get_age_difference(tt->age, existing_entry->age) + 2 * (existing_entry->node_type == NODE_BOUND_EXACT))
					existing_entry = &bucket_entries[i];
	}

	if (existing_entry->data) {

		#ifdef SEARCHINFO
		tt->collisions++;
		#endif

		//Don't overwrite existing with unknown nodes
		if (entry.node_type == NODE_BOUND_UNKNOWN) {
			#ifdef SEARCHINFO
			tt->shallow_collisions++;
			#endif
			return;
		}


		//Same position replacement rules
		if (entry.hash_msb == existing_entry->hash_msb) {
			if (existing_entry->depth > entry.depth + 2) {
				if (!existing_entry->best_move.raw)
					existing_entry->best_move = entry.best_move;
				return;
			}

			if (!entry.best_move.raw)
				entry.best_move = existing_entry->best_move;

			#ifdef SEARCHINFO
			tt->overwrites++;
			#endif
		}
	}
	#ifdef SEARCHINFO
	else
		tt->fills++;
	#endif


	if (entry.evaluation >= MIN_MATE_SCORE)
		entry.evaluation += ply;
	else if (entry.evaluation <= -MIN_MATE_SCORE)
		entry.evaluation -= ply;
	existing_entry->data = entry.data;
}
