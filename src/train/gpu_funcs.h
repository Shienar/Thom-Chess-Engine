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

//CPU fills one group while gpu uses other group.
//All copy instructinos get placed in a queue so this seema redundant, 
//but using ping-pong buffers increases positions/second by approximately 9.375%. 
//I'd guess that this is because it can safely overlap copying with kernel execution since
//its copying into a buffer that isn't being used, but I can't be certain. Weight updates are
//at the end of the queue for a minibatch and they don't use the shared buffers.
#define INPUT_GROUP(block) (block&1)
#define INPUT_GROUP_A 0
#define INPUT_GROUP_B 1

#define KERNEL_COUNT 8
#define MEM_COUNT 31
#define EVENT_TRACKED_KERNELS (KERNEL_COUNT - 1)
typedef struct {
    int deviceID;
    hipStream_t queue;
    hipModule_t module;

    //One kernel per function to run.
    //A union is used for iteration.
    union 
    {
        struct {
            hipFunction_t calculateAccumulator;
            hipFunction_t forwardPropagate;
            hipFunction_t backpropagate;
            hipFunction_t calculateGradient2;
            hipFunction_t calculateGradient1;
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

            void* weights1_fast;
            void* weights2_fast;

            void* bias1_fast;
            void* bias2_fast;

            void* weights1_slow;
            void* weights2_slow;

            void* bias1_slow;
            void* bias2_slow;
            
            void* accumulatorOutput;
            void* finalOutput;
            
            void* loss;
            
            void* delta2;
            void* delta1;
            
            void* gradient1Sum;
            void* gradient2Sum;

            void* gradientBias1Sum;
            void* gradientBias2Sum;

            //Adam first moments
            void* m_weights1;
            void* m_weights2;

            void* m_bias1;
            void* m_bias2;

            //Adam second moments
            void* v_weights1;
            void* v_weights2;

            void* v_bias1;
            void* v_bias2;
        };
        void* arr[MEM_COUNT];
    } mem;

} hipKernelArgs;

typedef struct {
    hipEvent_t readLoss;

    union {
        struct {
            hipEvent_t calcAccum;
            hipEvent_t fprop;
            hipEvent_t backprop;
            hipEvent_t gradient1;
            hipEvent_t gradient2;
            hipEvent_t denseUpdate;
            hipEvent_t inputUpdate;
        };
        hipEvent_t arr[EVENT_TRACKED_KERNELS];
    } startEvents;
    
    union {
        struct {
            hipEvent_t calcAccum;
            hipEvent_t fprop;
            hipEvent_t backprop;
            hipEvent_t gradient1;
            hipEvent_t gradient2;
            hipEvent_t denseUpdate;
            hipEvent_t inputUpdate;
        };
        hipEvent_t arr[EVENT_TRACKED_KERNELS];
    } endEvents;
} hipEvents;

extern hipContext hip_context;
extern hipKernelArgs hip_args;
extern hipEvents hip_events;

hipError_t initHIP(training_weights* raw_weights, short** h_active_A, float** h_expected_A, char** h_bucket_A,
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

void enqueueKernels(int bufferSide, int doBackprop);
void getWeights(training_weights* weights);

#endif