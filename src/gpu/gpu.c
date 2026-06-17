#include "gpu_funcs.h"
#include <string.h>

hipContext hip_context = {
    .deviceID = 0,
    .queue = NULL,
    .module = NULL,
    .kernels = { { NULL } }
};

hipKernelArgs hip_mem = { { { NULL } } };
hipEvents hip_events = {
    .readLoss = NULL,
    .startEvents = { { NULL } },
    .endEvents = { { NULL } } 
};

float lr = MAX_LR;
int64_t cosineIntervalLength;
uint64_t cosineTimestamp;
uint64_t timestamp;
int intervalCount;
float rho_inf = (2.0 / (1.0 - ADAM_BETA2)) - 1.0;
float rho_timestamp = 0.0;
float rectificationTerm = 0.0;
float biasCorrection1 = 1.0;
float biasCorrection2 = 1.0;

size_t weight1Size = sizeof(((network_weights*)0)->weights1);
size_t weight2Size = sizeof(((network_weights*)0)->weights2);
size_t weight3Size = sizeof(((network_weights*)0)->weights3);
size_t weight4Size = sizeof(((network_weights*)0)->weights4);

size_t bias1Size = sizeof(((network_weights*)0)->weights1_bias);
size_t bias2Size = sizeof(((network_weights*)0)->weights2_bias);
size_t bias3Size = sizeof(((network_weights*)0)->weights3_bias);
size_t bias4Size = sizeof(((network_weights*)0)->weights4_bias);

size_t weight1Count = sizeof(((network_weights*)0)->weights1) / sizeof(float);
size_t weight2Count = sizeof(((network_weights*)0)->weights2) / sizeof(float);
size_t weight3Count = sizeof(((network_weights*)0)->weights3) / sizeof(float);
size_t weight4Count = sizeof(((network_weights*)0)->weights4) / sizeof(float);

size_t bias1Count = sizeof(((network_weights*)0)->weights1_bias) / sizeof(float);
size_t bias2Count = sizeof(((network_weights*)0)->weights2_bias) / sizeof(float);
size_t bias3Count = sizeof(((network_weights*)0)->weights3_bias) / sizeof(float);
size_t bias4Count = sizeof(((network_weights*)0)->weights4_bias) / sizeof(float);

short* host_activeInputs_A = NULL; 
float* host_expectedOutputs_A = NULL;
short* host_activeInputs_B = NULL;
float* host_expectedOutputs_B = NULL;
float* host_lossbuffer = NULL;

void* calculateAccumulatorArgs[4];
void* fpropArgs[10];
void* backpropArgs[13];
void* grad4Args[4];
void* grad3Args[4];
void* grad2Args[4];
void* grad1Args[4];
void* inputAdamArgs[8];
void* denseAdamArgs[32];

unsigned char* loadCompiledKernels(size_t* size)
{
    char* fileName;

    #if defined(__HIP_PLATFORM_NVIDIA__)
        fileName = PROJECT_CWD "/target/obj/gpu/kernels.nvptx";
    #else
        fileName = PROJECT_CWD "/target/obj/gpu/kernels.hsaco";
    #endif

    FILE* input = fopen(fileName, "rb"); 
    if(!input)
    {
        DEBUG_ERROR("Cannot locate GPU kernel functions.");
        return NULL;
    }
    
    fseek(input, 0, SEEK_END);
    *size = ftell(input);
    rewind(input);

    unsigned char* binary = malloc(*size);
    if(!binary) 
    { 
        DEBUG_ERROR("Failed malloc.");
        fclose(input);
        return NULL; 
    }

    if(fread(binary, 1, *size, input) < 1)
    {
        DEBUG_ERROR("Mismatched read size.");
        fclose(input);
        return NULL;
    }
    
    
    fclose(input);

    return binary;
}

hipError_t initHIP(network_weights* nnue_weights, short** h_active_A, float** h_expected_A,
                                                        short** h_active_B, float** h_expected_B,
                                                        float** h_lossbuffer)
{
    cosineIntervalLength = FIRST_INTERVAL;
    cosineTimestamp = 0;
    timestamp = 0;
    intervalCount = 0;

    hipError_t err;

    
    if((err = hipInit(0)) != hipSuccess) { DEBUG_ERROR("Failed to initialize HIP."); return err; }
    
    hip_context.deviceID = 0;
    if((err = hipSetDevice(hip_context.deviceID)) != hipSuccess) { DEBUG_ERROR("Failed to acquire GPU device.");  return err; }

    #ifdef PERFT_KERNELS
        err = hipStreamCreate(&hip_context.queue);
    #else
        err = hipStreamCreateWithFlags(&hip_context.queue, hipStreamNonBlocking);
    #endif
    if(err != hipSuccess) { DEBUG_ERROR("Failed to initialize queue."); return err; }
    
    size_t binarySize = 0;
    unsigned char* binaryKernels = loadCompiledKernels(&binarySize);
    if(!binaryKernels || (err = hipModuleLoadData(&hip_context.module, binaryKernels)) != hipSuccess) { DEBUG_ERROR("Failed to load compiled binaries."); return err;}
    free(binaryKernels);
    binaryKernels = NULL;

    err = hipModuleGetFunction(&hip_context.kernels.calculateAccumulator, hip_context.module, "calculateAccumulator");
    err = hipModuleGetFunction(&hip_context.kernels.forwardPropagate, hip_context.module, "forwardPropagate");
    err = hipModuleGetFunction(&hip_context.kernels.backpropagate, hip_context.module, "backpropagate"); 
    err = hipModuleGetFunction(&hip_context.kernels.calculateGradient4, hip_context.module, "calculateGradient4");
    err = hipModuleGetFunction(&hip_context.kernels.calculateGradient3, hip_context.module, "calculateGradient3");
    err = hipModuleGetFunction(&hip_context.kernels.calculateGradient2, hip_context.module, "calculateGradient2");
    err = hipModuleGetFunction(&hip_context.kernels.calculateGradient1, hip_context.module, "calculateGradient1");
    err = hipModuleGetFunction(&hip_context.kernels.adamw, hip_context.module, "adamW");
    err = hipModuleGetFunction(&hip_context.kernels.inputadamw, hip_context.module, "inputAdamW");
    err = hipModuleGetFunction(&hip_context.kernels.lookahead, hip_context.module, "lookahead_update");

    if(err != hipSuccess) { DEBUG_ERROR("Error getting kernels."); return 1; }

    err = hipEventCreate(&hip_events.readLoss);
    for(int i = 0; i < EVENT_TRACKED_KERNELS; i++) 
    {
        err = hipEventCreate(&hip_events.startEvents.arr[i]);
        err = hipEventCreate(&hip_events.endEvents.arr[i]);
    }
    if(err != hipSuccess) { DEBUG_ERROR("Error creating events."); return 1; }

    /** ALLOCATING **/
    //Host pinning (Staging buffers)
    hipHostMalloc((void**)&host_activeInputs_A, 64 * MINIBATCH_SIZE * sizeof(short), hipHostMallocDefault);
    hipHostMalloc((void**)&host_expectedOutputs_A, MINIBATCH_SIZE * sizeof(float), hipHostMallocDefault);
    hipHostMalloc((void**)&host_activeInputs_B, 64 * MINIBATCH_SIZE * sizeof(short), hipHostMallocDefault);
    hipHostMalloc((void**)&host_expectedOutputs_B, MINIBATCH_SIZE * sizeof(float), hipHostMallocDefault);
    hipHostMalloc((void**)&host_lossbuffer, MINIBATCH_SIZE * sizeof(float), hipHostMallocDefault);
    //Export pointers
    *h_active_A = host_activeInputs_A;
    *h_expected_A = host_expectedOutputs_A;
    *h_active_B = host_activeInputs_B;
    *h_expected_B = host_expectedOutputs_B;
    *h_lossbuffer = host_lossbuffer;
    //gpu buffers (copied from host-pinning)
    hipMalloc(&hip_mem.mem.activeInputs_A, 64 * MINIBATCH_SIZE * sizeof(short));
    hipMalloc(&hip_mem.mem.activeInputs_B, 64 * MINIBATCH_SIZE * sizeof(short));
    hipMalloc(&hip_mem.mem.expectedOutput_A, MINIBATCH_SIZE * sizeof(float));
    hipMalloc(&hip_mem.mem.expectedOutput_B, MINIBATCH_SIZE * sizeof(float));
    hipMalloc(&hip_mem.mem.loss, MINIBATCH_SIZE * sizeof(float));
    //Fast weights
    hipMalloc(&hip_mem.mem.weights1_fast, weight1Size);
    hipMalloc(&hip_mem.mem.weights2_fast, weight2Size);
    hipMalloc(&hip_mem.mem.weights3_fast, weight3Size);
    hipMalloc(&hip_mem.mem.weights4_fast, weight4Size);
    hipMalloc(&hip_mem.mem.bias1_fast, bias1Size);
    hipMalloc(&hip_mem.mem.bias2_fast, bias2Size);
    hipMalloc(&hip_mem.mem.bias3_fast, bias3Size);
    hipMalloc(&hip_mem.mem.bias4_fast, bias4Size);
    //Slow Weights
    hipMalloc(&hip_mem.mem.weights1_slow, weight1Size);
    hipMalloc(&hip_mem.mem.weights2_slow, weight2Size);
    hipMalloc(&hip_mem.mem.weights3_slow, weight3Size);
    hipMalloc(&hip_mem.mem.weights4_slow, weight4Size);
    hipMalloc(&hip_mem.mem.bias1_slow, bias1Size);
    hipMalloc(&hip_mem.mem.bias2_slow, bias2Size);
    hipMalloc(&hip_mem.mem.bias3_slow, bias3Size);
    hipMalloc(&hip_mem.mem.bias4_slow, bias4Size);
    //Intermediate Outputs
    hipMalloc(&hip_mem.mem.accumulatorOutput, MINIBATCH_SIZE * sizeof(float) * ACCUMULATOR_NODES);
    hipMalloc(&hip_mem.mem.h2Output, MINIBATCH_SIZE * sizeof(float) * SECOND_HIDDEN_LAYER_NODES);
    hipMalloc(&hip_mem.mem.h3Output, MINIBATCH_SIZE * sizeof(float) * THIRD_HIDDEN_LAYER_NODES);
    hipMalloc(&hip_mem.mem.finalOutput, MINIBATCH_SIZE * sizeof(float));
    //Deltas
    hipMalloc(&hip_mem.mem.delta4, sizeof(float) * MINIBATCH_SIZE);
    hipMalloc(&hip_mem.mem.delta3, sizeof(float) * MINIBATCH_SIZE * THIRD_HIDDEN_LAYER_NODES);
    hipMalloc(&hip_mem.mem.delta2, sizeof(float) * MINIBATCH_SIZE * SECOND_HIDDEN_LAYER_NODES);
    hipMalloc(&hip_mem.mem.delta1, sizeof(float) * MINIBATCH_SIZE * ACCUMULATOR_NODES);
    //Gradient Sums
    hipMalloc(&hip_mem.mem.gradient1Sum, weight1Size);
    hipMalloc(&hip_mem.mem.gradient2Sum, weight2Size);
    hipMalloc(&hip_mem.mem.gradient3Sum, weight3Size);
    hipMalloc(&hip_mem.mem.gradient4Sum, weight4Size);
    hipMalloc(&hip_mem.mem.gradientBias1Sum, bias1Size);
    hipMalloc(&hip_mem.mem.gradientBias2Sum, bias2Size);
    hipMalloc(&hip_mem.mem.gradientBias3Sum, bias3Size);
    hipMalloc(&hip_mem.mem.gradientBias4Sum, bias4Size);
    //Adam First moments
    hipMalloc(&hip_mem.mem.m_weights1, weight1Size);
    hipMalloc(&hip_mem.mem.m_weights2, weight2Size);
    hipMalloc(&hip_mem.mem.m_weights3, weight3Size);
    hipMalloc(&hip_mem.mem.m_weights4, weight4Size);
    hipMalloc(&hip_mem.mem.m_bias1, bias1Size);
    hipMalloc(&hip_mem.mem.m_bias2, bias2Size);
    hipMalloc(&hip_mem.mem.m_bias3, bias3Size);
    hipMalloc(&hip_mem.mem.m_bias4, bias4Size);
    //Adam Second moments
    hipMalloc(&hip_mem.mem.v_weights1, weight1Size);
    hipMalloc(&hip_mem.mem.v_weights2, weight2Size);
    hipMalloc(&hip_mem.mem.v_weights3, weight3Size);
    hipMalloc(&hip_mem.mem.v_weights4, weight4Size);
    hipMalloc(&hip_mem.mem.v_bias1, bias1Size);
    hipMalloc(&hip_mem.mem.v_bias2, bias2Size);
    hipMalloc(&hip_mem.mem.v_bias3, bias3Size);
    hipMalloc(&hip_mem.mem.v_bias4, bias4Size);

    /** COPYING **/
    //weights1
    hipMemcpyAsync(hip_mem.mem.weights1_slow, (void*)nnue_weights->weights1, weight1Size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.weights1_fast, hip_mem.mem.weights1_slow, weight1Size, hipMemcpyDeviceToDevice, hip_context.queue);
    //weights2
    float* transposedWeight2 = calloc(ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES, sizeof(float));
    for (int i = 0; i < ACCUMULATOR_NODES; i++) {
        for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) {
            // CPU [output][input] -> GPU [input][output]
            transposedWeight2[i * SECOND_HIDDEN_LAYER_NODES + j] = nnue_weights->weights2[j][i];
        }
    }
    hipMemcpyAsync(hip_mem.mem.weights2_slow, (void*)transposedWeight2, weight2Size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.weights2_fast, hip_mem.mem.weights2_slow, weight2Size, hipMemcpyDeviceToDevice, hip_context.queue);
    //weights3
    float* transposedWeight3 = calloc(SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES, sizeof(float));
    for (int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) {
        for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) {
            // CPU [output][input] -> GPU [input][output]
            transposedWeight3[i * THIRD_HIDDEN_LAYER_NODES + j] = nnue_weights->weights3[j][i];
        }
    }
    hipMemcpyAsync(hip_mem.mem.weights3_slow, (void*)transposedWeight3, weight3Size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.weights3_fast, hip_mem.mem.weights3_slow, weight3Size, hipMemcpyDeviceToDevice, hip_context.queue);
    //weights4
    hipMemcpyAsync(hip_mem.mem.weights4_slow, (void*)&nnue_weights->weights4, weight4Size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.weights4_fast, hip_mem.mem.weights4_slow, weight4Size, hipMemcpyDeviceToDevice, hip_context.queue);
    //bias1
    hipMemcpyAsync(hip_mem.mem.bias1_slow, (void*)&nnue_weights->weights1_bias, bias1Size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.bias1_fast, hip_mem.mem.bias1_slow, bias1Size, hipMemcpyDeviceToDevice, hip_context.queue);
    //bias2
    hipMemcpyAsync(hip_mem.mem.bias2_slow, (void*)&nnue_weights->weights2_bias, bias2Size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.bias2_fast, hip_mem.mem.bias2_slow, bias2Size, hipMemcpyDeviceToDevice, hip_context.queue);
    //bias3
    hipMemcpyAsync(hip_mem.mem.bias3_slow, (void*)&nnue_weights->weights3_bias, bias3Size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.bias3_fast, hip_mem.mem.bias3_slow, bias3Size, hipMemcpyDeviceToDevice, hip_context.queue);
    //bias4
    hipMemcpyAsync(hip_mem.mem.bias4_slow, (void*)&nnue_weights->weights4_bias, bias4Size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.bias4_fast, hip_mem.mem.bias4_slow, bias4Size, hipMemcpyDeviceToDevice, hip_context.queue);

    /** ZEROING **/
    //First moments 
    hipMemsetAsync(hip_mem.mem.m_weights1, 0, weight1Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.m_weights2, 0, weight2Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.m_weights3, 0, weight3Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.m_weights4, 0, weight4Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.m_bias1, 0, bias1Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.m_bias2, 0, bias2Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.m_bias3, 0, bias3Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.m_bias4, 0, bias4Size, hip_context.queue);
    //Second moments
    hipMemsetAsync(hip_mem.mem.v_weights1, 0, weight1Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.v_weights2, 0, weight2Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.v_weights3, 0, weight3Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.v_weights4, 0, weight4Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.v_bias1, 0, bias1Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.v_bias2, 0, bias2Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.v_bias3, 0, bias3Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.v_bias4, 0, bias4Size, hip_context.queue);
    //Gradients
    hipMemsetAsync(hip_mem.mem.gradient1Sum, 0, weight1Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.gradientBias1Sum, 0, bias1Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.gradient2Sum, 0, weight2Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.gradientBias2Sum, 0, bias2Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.gradient3Sum, 0, weight3Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.gradientBias3Sum, 0, bias3Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.gradient4Sum, 0, weight4Size, hip_context.queue);
    hipMemsetAsync(hip_mem.mem.gradientBias4Sum, 0, bias4Size, hip_context.queue);

    hipStreamSynchronize(hip_context.queue);
    free(transposedWeight2);
    free(transposedWeight3);

    /** Set static arguments */
    //calculateAccumulatorArgs[0] = active inputs
    calculateAccumulatorArgs[1] = &hip_mem.mem.weights1_fast;
    calculateAccumulatorArgs[2] = &hip_mem.mem.bias1_fast;
    calculateAccumulatorArgs[3] = &hip_mem.mem.accumulatorOutput;

    fpropArgs[0]  = &hip_mem.mem.accumulatorOutput;
    fpropArgs[1]  = &hip_mem.mem.weights2_fast;
    fpropArgs[2]  = &hip_mem.mem.bias2_fast;
    fpropArgs[3]  = &hip_mem.mem.h2Output;
    fpropArgs[4]  = &hip_mem.mem.weights3_fast;
    fpropArgs[5]  = &hip_mem.mem.bias3_fast;
    fpropArgs[6]  = &hip_mem.mem.h3Output;
    fpropArgs[7]  = &hip_mem.mem.weights4_fast;
    fpropArgs[8]  = &hip_mem.mem.bias4_fast;
    fpropArgs[9] = &hip_mem.mem.finalOutput;

    backpropArgs[0]  = &hip_mem.mem.finalOutput;
    //backpropArgs[1] = expectedOutput;
    backpropArgs[2]  = &hip_mem.mem.h3Output;
    backpropArgs[3]  = &hip_mem.mem.h2Output;
    backpropArgs[4]  = &hip_mem.mem.accumulatorOutput;
    backpropArgs[5]  = &hip_mem.mem.weights4_fast;
    backpropArgs[6]  = &hip_mem.mem.weights3_fast;
    backpropArgs[7]  = &hip_mem.mem.weights2_fast;
    backpropArgs[8]  = &hip_mem.mem.delta4;
    backpropArgs[9]  = &hip_mem.mem.delta3;
    backpropArgs[10] = &hip_mem.mem.delta2;
    backpropArgs[11] = &hip_mem.mem.delta1;
    backpropArgs[12] = &hip_mem.mem.loss;

    grad4Args[0] = &hip_mem.mem.delta4;
    grad4Args[1] = &hip_mem.mem.h3Output;
    grad4Args[2] = &hip_mem.mem.gradient4Sum;
    grad4Args[3] = &hip_mem.mem.gradientBias4Sum;

    grad3Args[0] = &hip_mem.mem.delta3;
    grad3Args[1] = &hip_mem.mem.h2Output;
    grad3Args[2] = &hip_mem.mem.gradient3Sum;
    grad3Args[3] = &hip_mem.mem.gradientBias3Sum;

    grad2Args[0] = &hip_mem.mem.delta2;
    grad2Args[1] = &hip_mem.mem.accumulatorOutput;
    grad2Args[2] = &hip_mem.mem.gradient2Sum;
    grad2Args[3] = &hip_mem.mem.gradientBias2Sum;

    //grad1Args[0] = active inputs;
    grad1Args[1] = &hip_mem.mem.delta1;
    grad1Args[2] = &hip_mem.mem.gradient1Sum;
    grad1Args[3] = &hip_mem.mem.gradientBias1Sum;

    inputAdamArgs[0] = &hip_mem.mem.weights1_fast;
    inputAdamArgs[1] = &hip_mem.mem.gradient1Sum;
    inputAdamArgs[2] = &hip_mem.mem.m_weights1;
    inputAdamArgs[3] = &hip_mem.mem.v_weights1;
    inputAdamArgs[4] = &lr;
    inputAdamArgs[5] = &biasCorrection1;
    inputAdamArgs[6] = &biasCorrection2;
    inputAdamArgs[7] = &rectificationTerm;

    denseAdamArgs[0]  = &hip_mem.mem.weights2_fast;       denseAdamArgs[16] = &hip_mem.mem.bias2_fast;
    denseAdamArgs[1]  = &hip_mem.mem.gradient2Sum;        denseAdamArgs[17] = &hip_mem.mem.gradientBias2Sum;
    denseAdamArgs[2]  = &hip_mem.mem.m_weights2;          denseAdamArgs[18] = &hip_mem.mem.m_bias2;
    denseAdamArgs[3]  = &hip_mem.mem.v_weights2;          denseAdamArgs[19] = &hip_mem.mem.v_bias2;
    denseAdamArgs[4]  = &hip_mem.mem.weights3_fast;       denseAdamArgs[20] = &hip_mem.mem.bias3_fast;
    denseAdamArgs[5]  = &hip_mem.mem.gradient3Sum;        denseAdamArgs[21] = &hip_mem.mem.gradientBias3Sum;
    denseAdamArgs[6]  = &hip_mem.mem.m_weights3;          denseAdamArgs[22] = &hip_mem.mem.m_bias3;
    denseAdamArgs[7]  = &hip_mem.mem.v_weights3;          denseAdamArgs[23] = &hip_mem.mem.v_bias3;
    denseAdamArgs[8]  = &hip_mem.mem.weights4_fast;       denseAdamArgs[24] = &hip_mem.mem.bias4_fast;
    denseAdamArgs[9]  = &hip_mem.mem.gradient4Sum;        denseAdamArgs[25] = &hip_mem.mem.gradientBias4Sum;
    denseAdamArgs[10] = &hip_mem.mem.m_weights4;          denseAdamArgs[26] = &hip_mem.mem.m_bias4;
    denseAdamArgs[11] = &hip_mem.mem.v_weights4;          denseAdamArgs[27] = &hip_mem.mem.v_bias4;
    denseAdamArgs[12] = &hip_mem.mem.bias1_fast;          denseAdamArgs[28] = &lr;
    denseAdamArgs[13] = &hip_mem.mem.gradientBias1Sum;    denseAdamArgs[29] = &biasCorrection1;
    denseAdamArgs[14] = &hip_mem.mem.m_bias1;             denseAdamArgs[30] = &biasCorrection2;
    denseAdamArgs[15] = &hip_mem.mem.v_bias1;             denseAdamArgs[31] = &rectificationTerm;

    return hipSuccess;
}

void freeHIP()
{
    hipEventDestroy(hip_events.readLoss); 
    hip_events.readLoss = NULL;
    for(int i = 0; i < EVENT_TRACKED_KERNELS; i++) 
    {
        if(hip_events.startEvents.arr[i]) 
        {
            hipEventDestroy(hip_events.startEvents.arr[i]);
            hip_events.startEvents.arr[i] = NULL;
        }
        if(hip_events.endEvents.arr[i]) 
        {
            hipEventDestroy(hip_events.endEvents.arr[i]);
            hip_events.endEvents.arr[i] = NULL;
        }
    }

    if(hip_context.module) { hipModuleUnload(hip_context.module); hip_context.module = NULL; }
    for (int i = 0; i < KERNEL_COUNT; i++) { hip_context.kernels.arr[i] = NULL; }

    if(host_activeInputs_A) hipHostFree(host_activeInputs_A);
    if(host_activeInputs_B) hipHostFree(host_activeInputs_B);
    if(host_expectedOutputs_A) hipHostFree(host_expectedOutputs_A);
    if(host_expectedOutputs_B) hipHostFree(host_expectedOutputs_B);
    if(host_lossbuffer) hipHostFree(host_lossbuffer);

    host_activeInputs_A = NULL;
    host_activeInputs_B = NULL;
    host_expectedOutputs_A = NULL;
    host_expectedOutputs_B = NULL;
    host_lossbuffer = NULL;

    for (int i = 0; i < MEM_COUNT; i++) 
    {
        if(hip_mem.mem.arr[i]) 
        {
            hipFree(hip_mem.mem.arr[i]);
            hip_mem.mem.arr[i] = NULL;
        }
    }

    if(hip_context.queue) { hipStreamDestroy(hip_context.queue); hip_context.queue = NULL; }
}

float print_prof(const char* name, hipEvent_t start, hipEvent_t stop)
{
    float execute_ms = 0.0f;
    hipError_t err = hipEventElapsedTime(&execute_ms, start, stop);
    if (err == hipSuccess) 
    {
        printf("%-24s | %0.4f ms\n", name, execute_ms);
        return execute_ms;
    }
    else
    {
        printf("%-24s | Profiling Error\n", name);
        return 0.0f;
    }
}

void enqueueKernels(int bufferSide, int doBackprop)
{
    //cosine annealing
    lr = MIN_LR + 0.5 * (MAX_LR - MIN_LR) * (1.0 + cos(PI * (cosineTimestamp++) / cosineIntervalLength));
    if(cosineTimestamp >= cosineIntervalLength && intervalCount < MAX_INTERVALS)
    {
        cosineTimestamp = 0;
        cosineIntervalLength*=INTERVAL_SCALE;
        intervalCount++;
    }

    hipMemcpyAsync((bufferSide == INPUT_GROUP_A) ? hip_mem.mem.activeInputs_A : hip_mem.mem.activeInputs_B, 
                    (bufferSide == INPUT_GROUP_A) ? host_activeInputs_A : host_activeInputs_B, 
                    64 * MINIBATCH_SIZE * sizeof(short), hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync((bufferSide == INPUT_GROUP_A) ? hip_mem.mem.expectedOutput_A : hip_mem.mem.expectedOutput_B, 
                    (bufferSide == INPUT_GROUP_A) ? host_expectedOutputs_A : host_expectedOutputs_B, 
                    MINIBATCH_SIZE * sizeof(float), hipMemcpyHostToDevice, hip_context.queue);
    
    hipMemsetAsync(hip_mem.mem.loss, 0, MINIBATCH_SIZE * sizeof(float), hip_context.queue);

    calculateAccumulatorArgs[0] = (bufferSide == INPUT_GROUP_A) ? &hip_mem.mem.activeInputs_A : &hip_mem.mem.activeInputs_B;
    ENQUEUE_EVENT(hip_events.startEvents.calcAccum, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.calculateAccumulator,
                            MINIBATCH_SIZE * 2, 1, 1,
                            32, 1, 1,
                            0, hip_context.queue, calculateAccumulatorArgs, NULL);

    ENQUEUE_EVENT(hip_events.endEvents.calcAccum, hip_context.queue);

    ENQUEUE_EVENT(hip_events.startEvents.fprop, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.forwardPropagate,
                            MINIBATCH_SIZE, 1, 1,
                            32, 1, 1,
                            0, hip_context.queue, fpropArgs, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.fprop, hip_context.queue);


    backpropArgs[1] = (bufferSide == INPUT_GROUP_A) ? &hip_mem.mem.expectedOutput_A : &hip_mem.mem.expectedOutput_B;
    ENQUEUE_EVENT(hip_events.startEvents.backprop, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.backpropagate,
                            MINIBATCH_SIZE, 1, 1,
                            ACCUMULATOR_NODES_PER_SIDE, 1, 1,
                            0, hip_context.queue, backpropArgs, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.backprop, hip_context.queue);


    hipMemcpyAsync(host_lossbuffer, hip_mem.mem.loss, MINIBATCH_SIZE * sizeof(float), hipMemcpyDeviceToHost, hip_context.queue);
    hipEventRecord(hip_events.readLoss, hip_context.queue); 
    if(!doBackprop) return;

    ENQUEUE_EVENT(hip_events.startEvents.gradient4, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.calculateGradient4,
                            32, 1, 1,
                            32, 1, 1,
                            0, hip_context.queue, grad4Args, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.gradient4, hip_context.queue);


    ENQUEUE_EVENT(hip_events.startEvents.gradient3, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.calculateGradient3,
                            32, 1, 1,
                            32, 1, 1,
                            0, hip_context.queue, grad3Args, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.gradient3, hip_context.queue);


    ENQUEUE_EVENT(hip_events.startEvents.gradient2, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.calculateGradient2,
                            SECOND_HIDDEN_LAYER_NODES / 16, ACCUMULATOR_NODES / 16, 1,
                            16, 16, 1,
                            0, hip_context.queue, grad2Args, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.gradient2, hip_context.queue);

    
    grad1Args[0] = (bufferSide == INPUT_GROUP_A) ? &hip_mem.mem.activeInputs_A: &hip_mem.mem.activeInputs_B;
    ENQUEUE_EVENT(hip_events.startEvents.gradient1, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.calculateGradient1,
                            MINIBATCH_SIZE, 1, 1,
                            ACCUMULATOR_NODES_PER_SIDE, 1, 1,
                            0, hip_context.queue, grad1Args, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.gradient1, hip_context.queue);


    timestamp++;
    biasCorrection1 = powf(ADAM_BETA1, timestamp);
    biasCorrection2 = powf(ADAM_BETA2, timestamp);
    rho_timestamp = rho_inf - (2.0f * timestamp * biasCorrection2) / (1.0f - biasCorrection2);
    if(rho_timestamp > 5.0f) rectificationTerm = sqrt(((rho_timestamp - 4.0f) * (rho_timestamp - 2.0f) * rho_inf) / ((rho_inf - 4.0f) * (rho_inf - 2.0f) * rho_timestamp));
    else rectificationTerm = 0.0f;

    ENQUEUE_EVENT(hip_events.startEvents.denseUpdate, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.adamw,
                            weight2Count / 32, 1, 1,
                            32, 1, 1,
                            0, hip_context.queue, denseAdamArgs, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.denseUpdate, hip_context.queue);

 
    ENQUEUE_EVENT(hip_events.startEvents.inputUpdate, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.inputadamw,
                            weight1Count / 32, 1, 1,
                            32, 1, 1,
                            0, hip_context.queue, inputAdamArgs, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.inputUpdate, hip_context.queue);


    if(timestamp % LOOKAHEAD_RANGE == 0)
    {
        LOOKAHEAD_UPDATE(hip_mem.mem.weights1_fast, hip_mem.mem.weights1_slow, weight1Count);
        LOOKAHEAD_UPDATE(hip_mem.mem.weights2_fast, hip_mem.mem.weights2_slow, weight2Count);
        LOOKAHEAD_UPDATE(hip_mem.mem.weights3_fast, hip_mem.mem.weights3_slow, weight3Count);
        LOOKAHEAD_UPDATE(hip_mem.mem.weights4_fast, hip_mem.mem.weights4_slow, weight4Count);

        LOOKAHEAD_UPDATE(hip_mem.mem.bias1_fast, hip_mem.mem.bias1_slow, bias1Count);
        LOOKAHEAD_UPDATE(hip_mem.mem.bias2_fast, hip_mem.mem.bias2_slow, bias2Count);
        LOOKAHEAD_UPDATE(hip_mem.mem.bias3_fast, hip_mem.mem.bias3_slow, bias3Count);
        LOOKAHEAD_UPDATE(hip_mem.mem.bias4_fast, hip_mem.mem.bias4_slow, bias4Count);
    }

    #ifdef PERFT_KERNELS
        //Profiling disallows the efficient usage of ping-pong buffers, which slows down execution. 
        //It doesn't really matter since profiling only gets run for a short period.
        hipEventSynchronize(hip_events.endEvents.inputUpdate); 

        //Overlap is prevented with explicit synchronization.
        //This increases overhead while perfting, so the true time is somewhere between sum and total.
        float total_time_ms = 0.0f;
        float sum_time_ms = 0.0f;
        hipEventElapsedTime(&total_time_ms, hip_events.startEvents.calcAccum, hip_events.endEvents.inputUpdate);

        printf("\n--- Profiling ---\n");
        sum_time_ms += print_prof("Accumulator", hip_events.startEvents.calcAccum, hip_events.endEvents.calcAccum);
        sum_time_ms += print_prof("Forward Propagation", hip_events.startEvents.fprop, hip_events.endEvents.fprop);
        sum_time_ms += print_prof("Backpropagation", hip_events.startEvents.backprop, hip_events.endEvents.backprop);
        sum_time_ms += print_prof("Gradient 4", hip_events.startEvents.gradient4, hip_events.endEvents.gradient4);
        sum_time_ms += print_prof("Gradient 3", hip_events.startEvents.gradient3, hip_events.endEvents.gradient3);
        sum_time_ms += print_prof("Gradient 2", hip_events.startEvents.gradient2, hip_events.endEvents.gradient2);
        sum_time_ms += print_prof("Gradient 1", hip_events.startEvents.gradient1, hip_events.endEvents.gradient1);
        sum_time_ms += print_prof("Dense", hip_events.startEvents.denseUpdate, hip_events.endEvents.denseUpdate);
        sum_time_ms += print_prof("W1", hip_events.startEvents.inputUpdate, hip_events.endEvents.inputUpdate);
        printf("%-24s | %0.4f ms\n", "Sum", sum_time_ms);
        printf("%-24s | %0.4f ms\n", "Total", total_time_ms);

        printf("-----------------\n");    
    #endif
}

void getWeights(network_weights* weights)
{
    float* transposedWeights2 = malloc(weight2Size);
    float* transposedWeights3 = malloc(weight3Size);
    
    hipMemcpyDtoHAsync(weights->weights1, hip_mem.mem.weights1_slow, weight1Size, hip_context.queue);
    hipMemcpyDtoHAsync(transposedWeights2, hip_mem.mem.weights2_slow, weight2Size, hip_context.queue);
    hipMemcpyDtoHAsync(transposedWeights3, hip_mem.mem.weights3_slow, weight3Size, hip_context.queue);
    hipStreamSynchronize(hip_context.queue);
    
    for (int i = 0; i < ACCUMULATOR_NODES; i++) {
        for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) {
            //GPU [input][output] -> CPU [output][input]
            weights->weights2[j][i] = transposedWeights2[i * SECOND_HIDDEN_LAYER_NODES + j];
        }
    }
    
    for (int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) {
        for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) {
            //GPU [input][output] -> CPU [output][input]
            weights->weights3[j][i] = transposedWeights3[i * THIRD_HIDDEN_LAYER_NODES + j];
        }
    }
    
    hipMemcpyDtoHAsync(weights->weights4, hip_mem.mem.weights4_slow, weight4Size, hip_context.queue);
    hipMemcpyDtoHAsync(weights->weights1_bias, hip_mem.mem.bias1_slow, bias1Size, hip_context.queue);
    hipMemcpyDtoHAsync(weights->weights2_bias, hip_mem.mem.bias2_slow, bias2Size, hip_context.queue);
    hipMemcpyDtoHAsync(weights->weights3_bias, hip_mem.mem.bias3_slow, bias3Size, hip_context.queue);
    hipMemcpyDtoHAsync(&weights->weights4_bias, hip_mem.mem.bias4_slow, bias4Size, hip_context.queue);

    hipStreamSynchronize(hip_context.queue);
}