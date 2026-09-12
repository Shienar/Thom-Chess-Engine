#ifndef GPU_FUNCS
#define GPU_FUNCS

#if !defined(__HIP_PLATFORM_AMD__) && !defined(__HIP_PLATFORM_NVIDIA__)
    #define __HIP_PLATFORM_AMD__
#endif

#include <hip/hip_runtime_api.h>
#include "debug.h"
#include "analyze/nnue/neuralnet.h"
#include "train/train.h"

//Profiling
#ifdef PERFT_KERNELS
    #define ENQUEUE_EVENT(event, queue) hipStreamSynchronize(queue); hipEventRecord(event, queue)
#else
    #define ENQUEUE_EVENT(event, queue)
#endif

//Ping-pong buffers
#define INPUT_GROUP(block) (block&1)
#define INPUT_GROUP_A 0
#define INPUT_GROUP_B 1

#define KERNEL_COUNT 8
#define MEM_COUNT 36
#define EVENT_TRACKED_KERNELS (KERNEL_COUNT - 1)
typedef struct {
    int deviceID;
    hipStream_t queue;
    hipModule_t module;

    union 
    {
        struct {
            hipFunction_t calculateAccumulator;
            hipFunction_t calculateOutput;
            hipFunction_t calculateDeltas;
            hipFunction_t calculate_output_gradient;
            hipFunction_t calculate_accumulator_gradient;
            hipFunction_t adamw;
            hipFunction_t inputadamw;
            hipFunction_t lookahead;
        };
        hipFunction_t arr[KERNEL_COUNT];
    } kernels;
} hipContext;

typedef struct {
    union
    {
        struct
        {
            void* activeInputs_A;
            void* expectedOutput_A;
            void* outputBucket_A;

            void* activeInputs_B;
            void* expectedOutput_B;
            void* outputBucket_B;

            void* factorizer_weights_fast;
            void* factorizer_weights_slow;

            void* accumulator_weights_fast;
            void* accumulator_weights_slow;

            void* accumulator_bias_fast;
            void* accumulator_bias_slow;
            
            void* output_weights_fast;
            void* output_weights_slow;

            void* output_bias_fast;
            void* output_bias_slow;
            
            void* accumulatorOutput;
            void* finalOutput;
            
            void* loss;
            
            void* output_delta;
            void* accumulator_delta;
            
            void* factorizer_weights_gradient_sum;

            void* accumulator_weights_gradient_sum;
            void* accumulator_bias_gradient_sum;

            void* output_weights_gradient_sum;
            void* output_bias_gradient_sum;

            //Adam first moments
            void* m_factorizer_weights;

            void* m_accumulator_weights;
            void* m_accumulator_bias;

            void* m_output_weights;
            void* m_output_bias;

            //Adam second moments
            void* v_factorizer_weights;

            void* v_accumulator_weights;
            void* v_accumulator_bias;

            void* v_output_weights;
            void* v_output_bias;
        };
        void* arr[MEM_COUNT];
    } mem;

} hipKernelArgs;

typedef struct {
    hipEvent_t readLoss;

    union {
        struct {
            hipEvent_t calcAccum;
            hipEvent_t calculateOutput;
            hipEvent_t calculateDelta;
            hipEvent_t calculate_accumulator_gradient;
            hipEvent_t calculate_output_gradient;
            hipEvent_t denseUpdate;
            hipEvent_t inputUpdate;
        };
        hipEvent_t arr[EVENT_TRACKED_KERNELS];
    } startEvents;
    
    union {
        struct {
            hipEvent_t calcAccum;
            hipEvent_t calculateOutput;
            hipEvent_t calculateDelta;
            hipEvent_t calculate_accumulator_gradient;
            hipEvent_t calculate_output_gradient;
            hipEvent_t denseUpdate;
            hipEvent_t inputUpdate;
        };
        hipEvent_t arr[EVENT_TRACKED_KERNELS];
    } endEvents;
} hipEvents;

extern hipContext hip_context;
extern hipKernelArgs hip_args;
extern hipEvents hip_events;

hipError_t initHIP(training_weights* raw_weights, const char* compiledKernelPath,
                    short** h_active_A, float** h_expected_A, char** h_bucket_A,
                    short** h_active_B, float** h_expected_B, char** h_bucket_B,
                    float** h_lossbuffer);
void freeHIP();

#define LOOKAHEAD_UPDATE(fastWeights, slowWeights, size) \
    do { \
        void* lookaheadArgs[2] = { &fastWeights, &slowWeights }; \
        hipModuleLaunchKernel(hip_context.kernels.lookahead, \
                                (size + 32 - 1) / 32, 1, 1,  \
                                32, 1, 1, \
                                0, hip_context.queue, lookaheadArgs, NULL); \
    }while(0)

void enqueueKernels(int bufferSide);
void getWeights(training_weights* weights);
#endif