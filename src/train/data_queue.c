#include "train/data_queue.h"
#include "binpack/viri_binpack.h"
#include "analyze/nnue/accumulator.h"
#include "train/train.h"

void binpack_queue_init(MinibatchQueue* queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->stop_signal = 0;
    CREATE_MUTEX(queue->mutex);
    CREATE_COND_VARIABLE(queue->not_full);
    CREATE_COND_VARIABLE(queue->not_empty);
}

void binpack_queue_push(MinibatchQueue* queue, PreparedMinibatch* batch) {
    LOCK_MUTEX(queue->mutex);
    while(queue->count == BINPACK_QUEUE_CAPACITY && !queue->stop_signal)
        WAIT_COND_VARIABLE(queue->not_full, queue->mutex);

    if(queue->stop_signal) 
    {
        UNLOCK_MUTEX(queue->mutex);
        return;
    }
    queue->slots[queue->tail] = *batch;
    queue->tail = (queue->tail + 1) % BINPACK_QUEUE_CAPACITY;
    queue->count++;
    SIGNAL_COND_VARIABLE(queue->not_empty);
    UNLOCK_MUTEX(queue->mutex);
}

int binpack_queue_pop(MinibatchQueue* queue, PreparedMinibatch* popped) 
{
    LOCK_MUTEX(queue->mutex);
    while(queue->count == 0 && !queue->stop_signal)
        WAIT_COND_VARIABLE(queue->not_empty, queue->mutex);

    if(queue->count == 0 && queue->stop_signal) 
    {
        UNLOCK_MUTEX(queue->mutex);
        return 0;
    }

    *popped = queue->slots[queue->head];
    queue->head = (queue->head + 1) % BINPACK_QUEUE_CAPACITY;
    queue->count--;
    SIGNAL_COND_VARIABLE(queue->not_full);
    UNLOCK_MUTEX(queue->mutex);
    return 1;
}

uint32_t rng_xorshift32(uint32_t* state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}
int offsetGenerator_xorshift32(uint32_t* state, int kingBucket)
{
    uint32_t y = (rng_xorshift32(state)) % 1000;
    if(y < PERMUTE_BUCKET_PROBABILITY)
    {
        if(kingBucket >= 0)
            return neighboringKingBuckets[kingBucket][y % 5];
        else
        {
            if(y < PERMUTE_BUCKET_PROBABILITY / 2) return -1;
            else return 1;
        }
    }
    return 0;
}

void processSingleMinibatch(binpackDetails* details, PreparedMinibatch* batch, uint32_t* xorRNGState, int readerIndex, int fenSkip)
{
    bitboard board;

    //Jump to a random game in the reader's section.
    uint64_t sectionSize = details->headerEntries / details->numReaders;
    uint64_t indexOffset = rng_xorshift32(xorRNGState) % sectionSize;
    uint64_t byteOffset = details->headerOffsets[readerIndex * sectionSize + indexOffset];
    
    details->readerInfo[readerIndex].current_ptr = details->start_ptr + byteOffset;

    assert(details->readerInfo[readerIndex].current_ptr >= details->readerInfo[readerIndex].start_section);
    assert(details->readerInfo[readerIndex].current_ptr <= details->readerInfo[readerIndex].end_section);
    
    readPackedBoard(details, readerIndex);

    for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
    {
        Viri_Score score;
        uint8_t result;
        
        binpack_next(details, readerIndex, &board, &score, &result, 1, fenSkip);

        float relativeResult = 0.5f;
        if(result == VICTOR_DRAW_GENERIC) relativeResult = 0.5f;
        else if(result == VICTOR_WHITE)
        {
            if(board.turn == WHITE) relativeResult = 1.0f;
            else relativeResult = 0.0f;
        }
        else if(result == VICTOR_BLACK)
        {
            if(board.turn == BLACK) relativeResult = 1.0f;
            else relativeResult = 0.0f;
        }
        
        batch->expectedOutputs[entryNumber] = LAMBDA * (SIGMOID(score / EVAL_SCALE)) + (1.0f - LAMBDA) * relativeResult;

        uint64_t inputs[2 * PIECE_COUNT] = {0};
        int trackedPiecesPerColor = PIECE_COUNT / 2;
        for(int i = 0; i < PIECE_COUNT / 2; i++)
        {
            inputs[i] = board.pieces[2 * i];
            inputs[trackedPiecesPerColor + i] = board.pieces[2 * i + 1];

            inputs[PIECE_COUNT + i] = FLIP_MASK(board.pieces[2 * i + 1]);
            inputs[PIECE_COUNT + trackedPiecesPerColor + i] = FLIP_MASK(board.pieces[2 * i]);
        }

        if(getColumn(board.kingSquare[WHITE]) > 3)
            for(int p = 0; p < PIECE_COUNT; p++) 
                inputs[p] = mirrorBoard(inputs[p]);
        if(getColumn(board.kingSquare[BLACK]) > 3)
            for(int p = PIECE_COUNT; p < 2 * PIECE_COUNT; p++) 
                inputs[p] = mirrorBoard(inputs[p]);
        
        for(int color = 0; color < 2; color++)
        {
            #ifndef COMPRESS_KING_BUCKET
            int baseIndex = (color == WHITE) ? kingBuckets[board.kingSquare[WHITE]] : kingBuckets[FLIP_SQUARE(board.kingSquare[BLACK])];
            baseIndex += offsetGenerator_xorshift32(xorRNGState, baseIndex);
            baseIndex = clamp(baseIndex, 0, KING_BUCKETS - 1);
            baseIndex *= BITS_PER_KING_BUCKET;
            #else
            int baseIndex = 0;
            #endif

            int trackedInputs = 0;
            int side = (color != board.turn);
            
            for(int piece = 0; piece < PIECE_COUNT; piece++)
            {
                uint64_t mask = inputs[PIECE_COUNT * color + piece];
                while(mask)
                {
                    batch->activeInputs[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                    trackedInputs++;
                    mask &= (mask - 1);
                }
            }

            #ifndef COMPRESS_OUTPUT_BUCKET
            int outputBucket = ((trackedInputs - 1) / 4) + offsetGenerator_xorshift32(xorRNGState, -1);
            outputBucket = clamp(outputBucket, 0, OUTPUT_BUCKETS - 1);
            batch->outputBuckets[entryNumber] = outputBucket;
            #else
            batch->outputBuckets[entryNumber] = 0;
            #endif

            while(trackedInputs < 32)
            {
                batch->activeInputs[entryNumber * 64 + 32 * side + trackedInputs] = -1;
                trackedInputs++;
            }
        }
    }
}

THREAD_RETURN fillMinibatchQueue(THREAD_PARAM param)
{
    dataWorkerArgs* args = (dataWorkerArgs*)param;
    
    uint32_t xorRNGSeed = args->seed;
    PreparedMinibatch* localBatch = calloc(1, sizeof(PreparedMinibatch));

    while(!args->queue->stop_signal) 
    {
        processSingleMinibatch(args->details, localBatch, &xorRNGSeed, args->readerIndex, args->fenSkip);
        binpack_queue_push(args->queue, localBatch);
    }
    free(localBatch);
    return 0;
}