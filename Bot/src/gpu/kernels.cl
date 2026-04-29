#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable

#define MINIBATCH_SIZE 16384
#define INPUT_BITS 20480
#define HALF_INPUT_BITS 10240
#define ACCUMULATOR_NODES 512
#define ACCUMULATOR_NODES_PER_SIDE 256
#define SECOND_HIDDEN_LAYER_NODES 32
#define THIRD_HIDDEN_LAYER_NODES 32
#define OUTPUT_LAYER_NODES 1
#define ADAM_BETA1 0.9
#define ADAM_BETA2 0.999
#define ADAM_EPSILON 1e-8
#define ADAM_WEIGHT_DECAY 1e-2
#define LEAK_FACTOR 0.01f
#define EVAL_SCALE 400.0f

/******* UTIL *******/

inline float screlu_leaky(float x)
{
    return ((x <= 0.0f) ? LEAK_FACTOR * x : ((x >= 1.0f) ? 1.0f + LEAK_FACTOR * (x - 1.0f) : x*x));
}

//Avoid extra memory usage by only storing activated values.
//x is an activated value that gets the sqrt() taken from it if needed.
inline float screlu_leaky_derivative(float x)
{
    return ((x <= 0.0 || x >= 1.0) ? (LEAK_FACTOR) : (2.0*native_sqrt(x)));
}

inline float crelu_leaky(float x)
{
    return ((x <= 0.0f) ? LEAK_FACTOR * x : ((x >= 1.0f) ? 1.0f + LEAK_FACTOR * (x - 1.0f) : x));
}

inline float crelu_leaky_derivative(float x)
{
    return ((x <= 0.0f || x >= 1.0f) ? (LEAK_FACTOR) : 1.0f);
}

inline float sigmoid(float x)
{
    return 1.0f / (1.0f + exp(-x));
}

//From https://streamhpc.com/blog/2016-02-09/atomic-operations-for-floats-in-opencl-improved/
inline void AtomicAddFloat(volatile __global float* source, const float operand )
{
    union { unsigned int intVal; float floatVal; } newVal;
    union { unsigned int intVal; float floatVal; } prevVal;
    do {
        prevVal.floatVal = *source;
        newVal.floatVal = prevVal.floatVal + operand;
    }
    while (atomic_cmpxchg( (volatile __global unsigned int*)source, prevVal.intVal, newVal.intVal ) != prevVal.intVal);
}
inline void AtomicAddFloatLocal(volatile __local float* source, const float operand )
{
    union { unsigned int intVal; float floatVal; } newVal;
    union { unsigned int intVal; float floatVal; } prevVal;
    do {
        prevVal.floatVal = *source;
        newVal.floatVal = prevVal.floatVal + operand;
    }
    while (atomic_cmpxchg( (volatile __local unsigned int*)source, prevVal.intVal, newVal.intVal ) != prevVal.intVal);
}
inline void AtomicAddDouble(volatile __global double* source, const double operand)
{
    union { unsigned long long u64; double f64; } newVal;
    union { unsigned long long u64; double f64; } prevVal;
    
    do {
        prevVal.f64 = *source;
        newVal.f64 = prevVal.f64 + operand;
    }
    while (atom_cmpxchg((volatile __global ulong*)source, prevVal.u64, newVal.u64) != prevVal.u64);
}

/******* Forward Propagation *******/

//~ 5 ms
__kernel void calculateAccumulator(__global const short* activeInputs,       //contains indexes of weights of every active input in entire batch - [MINIBATCH_SIZE][2][32] - max 32 active features
                                    __global const float* weights,          //shared
                                    __global const float* bias,             //shared
                                    __global float* output)  //contains every activated output in entire batch
{
    int batchIndex = get_global_id(0);
    int outputIndex = get_global_id(1);

    int side = outputIndex / ACCUMULATOR_NODES_PER_SIDE;
    int sideOutputIndex = outputIndex % ACCUMULATOR_NODES_PER_SIDE;

    __global const short* myActiveInputs = &activeInputs[batchIndex * 64 + side * 32];

    float sum = bias[sideOutputIndex];
    int activeIndex = 0;
    while(myActiveInputs[activeIndex] != -1 && activeIndex < 32)
    {
        sum += weights[myActiveInputs[activeIndex] * ACCUMULATOR_NODES_PER_SIDE + sideOutputIndex];
        activeIndex++;
    }
    output[batchIndex * ACCUMULATOR_NODES + outputIndex] = screlu_leaky(sum); 
}

//~0.75 ms
__kernel void forwardPropagate(__global const float* accumulatorOutput,
                                __global float* h2_weights, __global float* h2_bias, __global float* h2_output,
                                __global float* h3_weights, __global float* h3_bias, __global float* h3_output,
                                __global float* output_weights, __global float* output_bias, __global float* outputs)
{
    int batchIndex = get_global_id(0);
    int localID = get_local_id(1); //0-63. Threads 32-63 help with loading / reduction. Threads 0-31 get an output node.

    //each local thread contributes to loading part of the input.
    //this same array will get reused later on, even if its too big to store outputs for h2/h3
    __local float shared[ACCUMULATOR_NODES];
    for(int i = localID; i < ACCUMULATOR_NODES; i+= 64)
    {
        shared[i] = accumulatorOutput[batchIndex * ACCUMULATOR_NODES + i];
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    //calculate h2
    float sum = 0.0;
    if (localID < SECOND_HIDDEN_LAYER_NODES) 
    {
        sum = h2_bias[localID];
        for (int inputIndex = 0; inputIndex < ACCUMULATOR_NODES; inputIndex++) 
        {
            sum += shared[inputIndex] * h2_weights[inputIndex * SECOND_HIDDEN_LAYER_NODES + localID];
        }
        sum = crelu_leaky(sum);


        shared[localID] = sum; //faster retrieval for use in this function.
        h2_output[batchIndex * SECOND_HIDDEN_LAYER_NODES + localID] = sum; //for use in backprop
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    //calculate h3
    sum = 0.0;
    if(localID < THIRD_HIDDEN_LAYER_NODES)
    {
        sum = h3_bias[localID];
        for (int inputIndex = 0; inputIndex < SECOND_HIDDEN_LAYER_NODES; inputIndex++) 
        {
            sum += shared[inputIndex] * h3_weights[inputIndex * THIRD_HIDDEN_LAYER_NODES + localID];
        }
        sum = crelu_leaky(sum);
        
        shared[localID] = sum;
        h3_output[batchIndex * THIRD_HIDDEN_LAYER_NODES + localID] = sum;
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    //calculate output
    sum = 0.0;
    if (localID < THIRD_HIDDEN_LAYER_NODES) 
    {
        shared[localID] = shared[localID] * output_weights[localID];
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    // Parallel Reduction for the final sum
    for(int stride = THIRD_HIDDEN_LAYER_NODES / 2; stride > 0; stride /= 2) 
    {
        if(localID < stride) 
        {
            shared[localID] += shared[localID + stride];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(localID == 0) 
    {
        outputs[batchIndex] = sigmoid((shared[0] + *output_bias) / (EVAL_SCALE));
    }
}

/******* Backpropagation (Delta calculations) *******/

//~5.5 ms
__kernel void backpropagate( __global const float* outputNodes, __global const float* expectedOutputs,
                            __global const float* h3, 
                            __global const float* h2,
                            __global const float* h1,
                            __global const float* weights4,
                            __global const float* weights3,
                            __global const float* weights2,
                            __global float* delta4,
                            __global float* delta3,
                            __global float* delta2,
                            __global float* delta1,
                            __global double* sumSquaredError)
{
    int batchIndex = get_global_id(0);
    int localID = get_local_id(1);

    __local float shared_delta[64]; 
    __local double shared_sse[64];

    //delta4
    float d4 = 0.0f;
    if(localID == 0) 
    {
        d4 = outputNodes[batchIndex] - expectedOutputs[batchIndex];

        shared_sse[0] = (d4 * d4);
        d4 *= (outputNodes[batchIndex] * (1.0f - outputNodes[batchIndex])); //sigmoid derivative.
        delta4[batchIndex] = d4;
        shared_delta[0] = d4;
    } 
    else shared_sse[localID] = 0.0;
    barrier(CLK_LOCAL_MEM_FENCE);
    
    //delta3
    d4 = shared_delta[0];
    if(localID < THIRD_HIDDEN_LAYER_NODES) 
    {
        float h3_val = h3[batchIndex * THIRD_HIDDEN_LAYER_NODES + localID];
        float d3 = d4 * weights4[localID] * crelu_leaky_derivative(h3_val);
        delta3[batchIndex * THIRD_HIDDEN_LAYER_NODES + localID] = d3;
        shared_delta[localID] = d3;
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    //delta2
    if(localID < SECOND_HIDDEN_LAYER_NODES) 
    {
        float sum = 0.0f;
        for (int outputIndex = 0; outputIndex < THIRD_HIDDEN_LAYER_NODES; outputIndex++) 
        {
            sum += shared_delta[outputIndex] * weights3[localID * THIRD_HIDDEN_LAYER_NODES + outputIndex];
        }
        float h2_val = h2[batchIndex * SECOND_HIDDEN_LAYER_NODES + localID];
        float d2 = sum * crelu_leaky_derivative(h2_val);
        delta2[batchIndex * SECOND_HIDDEN_LAYER_NODES + localID] = d2;
        shared_delta[localID] = d2;
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    //delta 1
    //512 delta / 64 threads = 8 deltas per
    for (int nodeIndex = localID; nodeIndex < ACCUMULATOR_NODES; nodeIndex+=64) 
    {
        float sum = 0.0f;
        for (int outputIndex = 0; outputIndex < SECOND_HIDDEN_LAYER_NODES; outputIndex++) 
        {
            sum += shared_delta[outputIndex] * weights2[nodeIndex * SECOND_HIDDEN_LAYER_NODES + outputIndex];
        }
        delta1[batchIndex * ACCUMULATOR_NODES + nodeIndex] = sum * screlu_leaky_derivative(h1[batchIndex * ACCUMULATOR_NODES + nodeIndex]);
    }

    //SSE accumulation
    if (localID == 0)  AtomicAddDouble(sumSquaredError, shared_sse[0]);
}

/******* Calculate & Accumulate Edge Weight Gradients *******/

//0.069 ms
__kernel void calculateGradient4(__global const float* delta4, //1 per batch sample
                                    __global const float* h3, //THIRD_HIDDEN_LAYER_NODES per batch sample
                                    __global float* gradient4_Sum, //THIRD_HIDDEN_LAYER_NODES shared
                                    __global float* bias4_Sum) //1 shared
{
    int weightIndex = get_global_id(0); 
    int localID = get_local_id(1);
    int localSize = get_local_size(1);

    float partialSum = 0.0;
    float partialBiasSum = 0.0; //Only managed by the threads with weightIndex == 0.

    for(int batchIndex = localID; batchIndex < MINIBATCH_SIZE; batchIndex+=localSize) 
    {
        partialSum += delta4[batchIndex] * h3[THIRD_HIDDEN_LAYER_NODES * batchIndex + weightIndex];
        if(weightIndex == 0) partialBiasSum += delta4[batchIndex];
    }

    __local float tempSum[64];
    __local float tempBias[64];
    tempSum[localID] = partialSum;
    if(weightIndex == 0) tempBias[localID] = partialBiasSum;
    barrier(CLK_LOCAL_MEM_FENCE);

    for(int stride = localSize / 2; stride > 0; stride/=2)
    {
        if(localID < stride)
        {
            tempSum[localID] += tempSum[localID + stride];
            if(weightIndex == 0) tempBias[localID] += tempBias[localID + stride];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(localID == 0)
    {
        AtomicAddFloat(&gradient4_Sum[weightIndex], tempSum[0]);
        if(weightIndex == 0) AtomicAddFloat(bias4_Sum, tempBias[0]);
    }
}


//0.48ms
__kernel void calculateGradient3(__global const float* delta3, //THIRD_HIDDEN_LAYER_NODES per batch sample
                                __global const float* h2, //SECOND_HIDDEN_LAYER_NODES per batch sample
                                __global float* gradient3_Sum, //SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES shared
                                __global float* bias3_Sum) //THIRD_HIDDEN_LAYER_NODES shared
{
    int flatWeightIndex = get_global_id(0);

    int outIndex = flatWeightIndex % THIRD_HIDDEN_LAYER_NODES;
    int inIndex  = flatWeightIndex / THIRD_HIDDEN_LAYER_NODES;

    float partialSum = 0.0;
    float partialBiasSum = 0.0; //Only managed by the threads with inIndex == 0.
    
    for(int batchIndex = 0; batchIndex < MINIBATCH_SIZE; batchIndex++)  
    {
        partialSum += delta3[batchIndex * THIRD_HIDDEN_LAYER_NODES + outIndex] * h2[batchIndex * SECOND_HIDDEN_LAYER_NODES + inIndex];
        if(inIndex == 0) partialBiasSum += delta3[batchIndex * THIRD_HIDDEN_LAYER_NODES + outIndex];
    }

    gradient3_Sum[flatWeightIndex] += partialSum;

    if(inIndex == 0) bias3_Sum[outIndex] += partialBiasSum;
}

//~5.8ms
__kernel void calculateGradient2(__global const float* delta2, //SECOND_HIDDEN_LAYER_NODES per batch sample
                                __global const float* h1, //ACCUMULATOR_NODES per batch sample
                                __global float* gradient2_Sum, //ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES shared
                                __global float* bias2_Sum) //SECOND_HIDDEN_LAYER_NODES shared
{
    int inputNodeIndex = get_group_id(0); // 0 to 511
    int localID = get_local_id(0);        // 0 to 63

    __local float shared_weight[32][64];
    __local float shared_bias[32][64];

    float partialGradientSums[32] = {0.0f};
    float partialBiasSums[32] = {0.0f};

    //64 threads each work on subsets of the batch.
    for (int batchIndex = localID; batchIndex < MINIBATCH_SIZE; batchIndex += 64) 
    {
        float h1_val = h1[batchIndex * ACCUMULATOR_NODES + inputNodeIndex];
        
        for(int outputWeight = 0; outputWeight < SECOND_HIDDEN_LAYER_NODES; outputWeight++) {
            partialGradientSums[outputWeight] += h1_val * delta2[batchIndex * 32 + outputWeight];
            if(inputNodeIndex == 0) partialBiasSums[outputWeight] += delta2[batchIndex * 32 + outputWeight];
        }
    }

    //share the private partialGradientSums
    for(int n = 0; n < 32; n++) {
        shared_weight[n][localID] = partialGradientSums[n];
        if(inputNodeIndex == 0) shared_bias[n][localID] = partialBiasSums[n];
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    //Use 32 threads to sum the values from the 64 threads.
    if (localID < 32) {
        float finalWeightGradient = 0.0f;
        float finalBiasGradient = 0.0f;
        for(int i = 0; i < 64; i++) {
            finalWeightGradient += shared_weight[localID][i];
            if(inputNodeIndex == 0) finalBiasGradient += shared_weight[localID][i];
        }
        AtomicAddFloat(&gradient2_Sum[inputNodeIndex * 32 + localID], finalWeightGradient);
        if(inputNodeIndex == 0) AtomicAddFloat(&bias2_Sum[localID], finalBiasGradient);
    }
}

//~3.5ms
__kernel void calculateGradient1(__global const short* activeInputs,       //contains indexes of weights of every active input in entire batch - [MINIBATCH_SIZE][2][32] - max 32 active features
                                    __global const float* delta1, // ACCUMULATOR_NODES per batch sample
                                    __global float* gradient1_Sum, // ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS shared
                                    __global float* bias1_Sum)    // ACCUMULATOR_NODES_PER_SIDE shared
{
    int batchIndex = get_global_id(0);
    int localID = get_local_id(1);

    // Shared storage for the 64 active features of this board position
    __local short sharedFeatures[64];

    sharedFeatures[localID] = activeInputs[batchIndex * 64 + localID];

    barrier(CLK_LOCAL_MEM_FENCE);


    //Process all 256 nodes in chunks of 64
    for (int outIndex = localID; outIndex < ACCUMULATOR_NODES_PER_SIDE; outIndex+=64) 
    {
        float d1_w = delta1[batchIndex * ACCUMULATOR_NODES + outIndex];
        float d1_b = delta1[batchIndex * ACCUMULATOR_NODES + ACCUMULATOR_NODES_PER_SIDE + outIndex];

        AtomicAddFloat(&bias1_Sum[outIndex], d1_b + d1_w);

        int f = 0;
        while(sharedFeatures[f] != -1 && f < 32)
        {
            AtomicAddFloat(&gradient1_Sum[sharedFeatures[f] * ACCUMULATOR_NODES_PER_SIDE + outIndex], d1_w);
            AtomicAddFloat(&gradient1_Sum[sharedFeatures[32 + f] * ACCUMULATOR_NODES_PER_SIDE + outIndex], d1_b);
            f++;
        }
    }
}

/******* Adam Update *******/

//0.001 ms on weights 2-4
__kernel void adamW(__global float* weights,
                    __global float* gradientSum,
                    __global float* firstMoment,
                    __global float* secondMoment,
                    float learningRate,
                    float biasCorrection1,
                    float biasCorrection2)
{
    // Flattened array of weights.
    int i = get_global_id(0);

    float grad = gradientSum[i] / MINIBATCH_SIZE;

    firstMoment[i] = ADAM_BETA1 * firstMoment[i] + (1.0f - ADAM_BETA1) * grad;
    secondMoment[i] = ADAM_BETA2 * secondMoment[i] + (1.0f - ADAM_BETA2) * grad * grad;

    float correctedFirstMoment = firstMoment[i] / (1.0f - biasCorrection1);
    float correctedSecondMoment = secondMoment[i] / (1.0f - biasCorrection2);

    weights[i] -= learningRate * (correctedFirstMoment / (sqrt(correctedSecondMoment) + ADAM_EPSILON));
    weights[i] -= learningRate * ADAM_WEIGHT_DECAY * weights[i];
}

//0.475 ms on sparse input layer.
__kernel void lazyAdam(__global float* weights,
                    __global float* gradientSum,
                    __global float* firstMoment,
                    __global float* secondMoment,
                    float learningRate,
                    float biasCorrection1,
                    float biasCorrection2)
{
    // Flattened array of weights.
    int i = get_global_id(0);

    float grad = gradientSum[i] / MINIBATCH_SIZE;
    if(!grad) return; //saves about 0.1ms since its a very sparse input.

    firstMoment[i] = ADAM_BETA1 * firstMoment[i] + (1.0f - ADAM_BETA1) * grad;
    secondMoment[i] = ADAM_BETA2 * secondMoment[i] + (1.0f - ADAM_BETA2) * grad * grad;

    float correctedFirstMoment = firstMoment[i] / (1.0f - biasCorrection1);
    float correctedSecondMoment = secondMoment[i] / (1.0f - biasCorrection2);

    weights[i] -= learningRate * (correctedFirstMoment / (sqrt(correctedSecondMoment) + ADAM_EPSILON));
    weights[i] -= learningRate * ADAM_WEIGHT_DECAY * weights[i];
}