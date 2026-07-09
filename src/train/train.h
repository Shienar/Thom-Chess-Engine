#ifndef NEURALNET_TRAIN
#define NEURALNET_TRAIN

#include "analyze/neuralnet.h"
#include "train/gpu_funcs.h"

#define MAX_LR 8e-4f
#define MIN_LR 2.5e-6f
#define MAX_COSINE_ANNEAL_TIMESTAMP (180 * MINIBATCHES_PER_EPOCH)
#define LOOKAHEAD_RANGE 10

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f

#define EVAL_SCALE 400.0f
#define LAMBDA 0.9f
#define SIGMOID(x) (1.0 / (1.0 + exp(-(x))))

#define MINIBATCH_SIZE 16384
#define MINIBATCHES_PER_EPOCH 6104 // 6,104 * 16,384 = 100,007,936

//Binpack should have a larger size to account for fen skipping & variety.
#define VALIDATION_BINPACK_MINIBATCHES (1000000 / MINIBATCH_SIZE)

#define FEN_SKIP_TRAINING 15
#define FEN_SKIP_VALIDATION 25

//N in 1000 chance to shift the king or output bucket.
//An attempt at blending the difference between neighboring buckets.
#define PERMUTE_BUCKET_PROBABILITY 50

//Only train and use part of the network, broadcast trained weights to other buckets.
//These get left commented out once a compressed version gets trained. Uncommenting them will overwrite any bucketed waights.
//#define COMPRESS_KING_BUCKET
//#define COMPRESS_OUTPUT_BUCKET

/**
 * 2 x ((10 x 768) -> 512) -> (8 x 1)
 * 
 * Training progression (Train until plateau at each, then continue with QAT, then verify it with SPRT):
 * 1. Force single input/output buckets
 *      - 2 x (768->512) -> 1.
 * 2. Expand to multiple king buckets.
 *      - 2 x ((10 x 768) -> 512) -> 1
 * 3. Expand to multiple output buckets
 *      - 2 x ((10 x 768) -> 512) -> (8 x 1)
 * 
 * References / Algorithms:
 * 
 *      Ranger
 *          - https://arxiv.org/pdf/2106.13731
 *      - Adaptive Moment Estimation 
 *          - https://arxiv.org/abs/1412.6980v8
 *      - Weight Decay
 *      - RAdam
 *          - https://arxiv.org/pdf/1908.03265
 *      - Lookahead
 *          - https://proceedings.neurips.cc/paper_files/paper/2019/file/90fd4f88f588ae64038134f1eeaa023f-Paper.pdf
 */
void train(int maxIterations, float maxAllowedError);

void compressKingBucket(training_weights* weights);
void compressOutputBucket(training_weights* weights);
void broadcastKingBucket(training_weights* weights);
void broadcastOutputBucket(training_weights* weights);

#endif