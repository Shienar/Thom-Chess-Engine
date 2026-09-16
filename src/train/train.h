#ifndef NEURALNET_TRAIN
#define NEURALNET_TRAIN

#include "analyze/nnue/neuralnet.h"
#include "train/gpu_trainer.h"

#define LOOKAHEAD_RANGE 10

extern float max_lr;
extern float min_lr;
extern uint64_t max_cosine_anneal_timestamp;

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f

//expectedScore = LAMBDA * sigmoid(cp / EVAL_SCALE) + (1 - LAMBDA) * relativeResult;
#define LAMBDA 0.9f
#define SIGMOID(x) (1.0 / (1.0 + exp(-(x))))

#define MINIBATCH_SIZE 16384
#define MINIBATCHES_PER_EPOCH 6104 // 6,104 * 16,384 = 100,007,936

#define FEN_SKIP_TRAINING 0

void initializeTrainingWeights(training_weights* raw_weights);
void quantizeWeights(training_weights* raw, nnue_weights* quantized);
void saveRawWeights(training_weights* weights, const char* path);
void saveQuantizedWeights(nnue_weights* weights, const char* path);

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
void train(int epochs, float min_learningRate, float max_learningRate, const char* binpackFileName, const char* kernelFileName);

#endif