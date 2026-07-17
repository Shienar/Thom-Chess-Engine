#ifndef DATA_QUEUE
#define DATA_QUEUE

#include "types.h"
#include "binpack/viri_binpack.h"
#include "train/train.h"

#define BINPACK_QUEUE_CAPACITY 32
typedef struct {
    short activeInputs[MINIBATCH_SIZE * 64];
    float expectedOutputs[MINIBATCH_SIZE];
    char outputBuckets[MINIBATCH_SIZE];
} PreparedMinibatch;

typedef struct {
    PreparedMinibatch slots[BINPACK_QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    int stop_signal;
    mutex_t mutex;
    cond_t not_full;
    cond_t not_empty;
} MinibatchQueue;

typedef struct {
    binpackDetails* details;
    MinibatchQueue* queue;
    uint32_t seed;
    int fenSkip;
    int readerIndex;
} dataWorkerArgs;

void binpack_queue_init(MinibatchQueue* queue);
void binpack_queue_push(MinibatchQueue* queue, PreparedMinibatch* batch);
int binpack_queue_pop(MinibatchQueue* queue, PreparedMinibatch* popped);

void processSingleMinibatch(binpackDetails* details, PreparedMinibatch* batch, uint32_t* xorRNGState, int readerIndex, int fenSkip);

THREAD_RETURN fillMinibatchQueue(THREAD_PARAM param);

#endif