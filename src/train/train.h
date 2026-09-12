#ifndef NEURALNET_TRAIN
#define NEURALNET_TRAIN

#include "analyze/nnue/neuralnet.h"
#include "train/gpu_funcs.h"

#define MAX_LR 8e-4f
#define MIN_LR 2.5e-5f
#define MAX_COSINE_ANNEAL_TIMESTAMP (60 * MINIBATCHES_PER_EPOCH)
#define LOOKAHEAD_RANGE 10

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f

//expectedScore = LAMBDA * sigmoid(cp / EVAL_SCALE) + (1 - LAMBDA) * relativeResult;
#define LAMBDA 0.9f
#define SIGMOID(x) (1.0 / (1.0 + exp(-(x))))

#define MINIBATCH_SIZE 16384
#define MINIBATCHES_PER_EPOCH 6104 // 6,104 * 16,384 = 100,007,936

#define FEN_SKIP_TRAINING 3

/**
 * References / Algorithms:
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
void train(int maxIterations, const char* binpackFileName, const char* kernelFileName);

#endif