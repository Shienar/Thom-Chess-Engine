#define POSITIONS_PER_FILE 16384
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
#define ADAM_WEIGHT_DECAY 1e-5

#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable

/******* UTIL *******/

inline float screlu_leaky(float x)
{
    return ((x <= 0.0f) ? 0.01f * x : ((x >= 1.0f) ? 1.0f + 0.01f * (x - 1.0f) : x*x));
}

//Avoid extra memory usage by only storing activated values.
//x is an activated value that gets the sqrt() taken from it if needed.
inline float screlu_leaky_derivative(float x)
{
    return ((x <= 0.0 || x >= 1.0) ? (0.01) : (2.0*sqrt(x)));
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
__kernel void calculateAccumulator(__global const short* activeInputs,       //contains indexes of weights of every active input in entire batch - [POSITIONS_PER_FILE][2][32] - max 32 active features
                                    __global const char* activeCount,      //how many active features. [POSITIONS_PER_FILE]
                                    __global const float* weights,          //shared
                                    __global const float* bias,             //shared
                                    __global float* output)  //contains every activated output in entire batch
{
    int batchIndex = get_global_id(0);
    int outputIndex = get_global_id(1);

    int side = outputIndex / ACCUMULATOR_NODES_PER_SIDE;
    int sideOutputIndex = outputIndex % ACCUMULATOR_NODES_PER_SIDE;

    __global const short* myActiveInputs = &activeInputs[batchIndex * 2 * 32 + side * 32];

    float sum = bias[sideOutputIndex];
    for(int activeIndex = 0; activeIndex < activeCount[batchIndex]; activeIndex++)
    {
        sum += weights[myActiveInputs[activeIndex] * ACCUMULATOR_NODES_PER_SIDE + sideOutputIndex];
    }
    output[batchIndex * ACCUMULATOR_NODES + outputIndex] = screlu_leaky(sum); 
}

__kernel void calculateH2(__global const float* inputBatch,
                            __global const float* weights,
                            __global const float* bias,
                            __global float* output)
{
    int batchIndex = get_global_id(0);
    int outputIndex = get_global_id(1);
    int localID = get_local_id(1);

    //each local thread contributes to loading part of sharedinput.
    __local float sharedInput[ACCUMULATOR_NODES];
    for(int i = localID; i < ACCUMULATOR_NODES; i+= SECOND_HIDDEN_LAYER_NODES)
    {
        sharedInput[i] = inputBatch[batchIndex * ACCUMULATOR_NODES + i];
    }
    
    barrier(CLK_LOCAL_MEM_FENCE);

    float sum = bias[outputIndex];
    for (int inputIndex = 0; inputIndex < ACCUMULATOR_NODES; inputIndex++) 
    {
        sum += sharedInput[inputIndex] * weights[inputIndex * SECOND_HIDDEN_LAYER_NODES + outputIndex];
    }
    output[batchIndex * SECOND_HIDDEN_LAYER_NODES + outputIndex] = screlu_leaky(sum); 
}

__kernel void calculateH3(__global const float* inputBatch,
                            __global const float* weights,
                            __global const float* bias,
                            __global float* output)
{
    int batchIndex = get_global_id(0);
    int outputIndex = get_global_id(1);
    int localID = get_local_id(0); 
    
    __local float sharedInput[SECOND_HIDDEN_LAYER_NODES];
    sharedInput[localID] = inputBatch[batchIndex * SECOND_HIDDEN_LAYER_NODES + localID];
    
    barrier(CLK_LOCAL_MEM_FENCE);
    
    float sum = bias[outputIndex];

    for (int inputIndex = 0; inputIndex < SECOND_HIDDEN_LAYER_NODES; inputIndex++) {
        sum += sharedInput[inputIndex] * weights[inputIndex * THIRD_HIDDEN_LAYER_NODES + outputIndex];
    }
    output[batchIndex * THIRD_HIDDEN_LAYER_NODES + outputIndex] = screlu_leaky(sum); 
}


__kernel void calculateOutput(__global const float* inputBatch,
                                __global const float* weights,
                                __global const float* bias,
                                __global float* output)
{
    int batchIndex = get_global_id(0);
    int localID = get_local_id(1);

    __local float sum[32];
    sum[localID] = inputBatch[batchIndex * 32 + localID] * weights[localID];
    
    barrier(CLK_LOCAL_MEM_FENCE);
    
    __global const float* myInput = &inputBatch[batchIndex * THIRD_HIDDEN_LAYER_NODES];

    for(int stride = THIRD_HIDDEN_LAYER_NODES / 2; stride > 0; stride/=2)
    {
        if(localID < stride)
        {
            sum[localID] += sum[localID + stride];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }
    if(localID == 0) 
    {
        output[batchIndex] = sum[0] + *bias;
    }
}

/******* Backpropagation (Delta calculations) *******/
__kernel void calculateDelta4(__global const float* outputNodes,
                                __global const float* expectedOutputs,
                                __global float* delta4,
                                __global double* sumSquaredError)
{
    int batchIndex = get_global_id(0);
    int localID = get_local_id(0);
    int localSize = get_local_size(0);
    int groupID = get_group_id(0);

    double error = outputNodes[batchIndex] - expectedOutputs[batchIndex];
    delta4[batchIndex] = error;

    __local double temp[256];
    temp[localID] = error * error;
    barrier(CLK_LOCAL_MEM_FENCE);
    
    //parallel reduction of partial sum within group.
    for(int stride = localSize / 2; stride > 0; stride/=2)
    {
        if(localID < stride) temp[localID] += temp[localID + stride]; 
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(localID == 0) 
    {
        AtomicAddDouble(sumSquaredError, temp[0]);
    }
}

__kernel void calculateDelta3(__global const float* delta4,
                                __global const float* weights,
                                __global const float* outputs,
                                __global float* delta3)
{
    int batchIndex = get_global_id(0);
    int nodeIndex = get_global_id(1);

    delta3[batchIndex * THIRD_HIDDEN_LAYER_NODES + nodeIndex] = delta4[batchIndex] * weights[nodeIndex] * screlu_leaky_derivative(outputs[batchIndex * THIRD_HIDDEN_LAYER_NODES + nodeIndex]);
}

__kernel void calculateDelta2(__global const float* delta3,
                                __global const float* weights,
                                __global const float* outputs,
                                __global float* delta2)
{
    int batchIndex = get_global_id(0);
    int nodeIndex = get_global_id(1);
    int localID = get_local_id(1);

    __local float shared_delta3[32];
    shared_delta3[localID] = delta3[batchIndex * THIRD_HIDDEN_LAYER_NODES + localID];
    barrier(CLK_LOCAL_MEM_FENCE);
    
    float sum = 0;
    for(int i = 0; i < 32; i++)
    {
        sum += shared_delta3[i] * weights[i * THIRD_HIDDEN_LAYER_NODES + nodeIndex];
    }           
    delta2[batchIndex * THIRD_HIDDEN_LAYER_NODES + nodeIndex] = sum * screlu_leaky_derivative(outputs[batchIndex * THIRD_HIDDEN_LAYER_NODES + nodeIndex]);

}

__kernel void calculateDelta1(__global const float* delta2,
                                __global const float* weights,
                                __global const float* outputs,
                                __global float* delta1)
{
    int batchIndex = get_global_id(0);
    int nodeIndex = get_global_id(1);
    int localID = get_local_id(1);

    int side = nodeIndex / ACCUMULATOR_NODES_PER_SIDE;
    int localNodeIndex = nodeIndex % ACCUMULATOR_NODES_PER_SIDE;

    __local float shared_delta2[32];
    if(localID < 32) shared_delta2[localID] = delta2[batchIndex * SECOND_HIDDEN_LAYER_NODES + localID];
    barrier(CLK_LOCAL_MEM_FENCE);


    float sum = 0;
    for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
    {
        sum+= shared_delta2[j] * weights[nodeIndex * SECOND_HIDDEN_LAYER_NODES + j];
    }           
    delta1[batchIndex * ACCUMULATOR_NODES + nodeIndex] = sum * screlu_leaky_derivative(outputs[batchIndex * ACCUMULATOR_NODES + nodeIndex]);
}

/******* Calculate & Accumulate Edge Weight Deltas *******/
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

    for(int batchIndex = localID; batchIndex < POSITIONS_PER_FILE; batchIndex+=localSize) 
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
            if(weightIndex == 0) partialBiasSum += tempBias[localID + stride];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(localID == 0)
    {
        AtomicAddFloat(&gradient4_Sum[weightIndex], tempSum[0]);
        if(weightIndex == 0) AtomicAddFloat(&bias4_Sum[weightIndex], tempBias[0]);
    }
}

__kernel void calculateGradient3(__global const float* delta3, //THIRD_HIDDEN_LAYER_NODES per batch sample
                                __global const float* h2, //SECOND_HIDDEN_LAYER_NODES per batch sample
                                __global float* gradient3_Sum, //SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES shared
                                __global float* bias3_Sum) //THIRD_HIDDEN_LAYER_NODES shared
{
    int flatWeightIndex = get_global_id(0);
    int localID = get_local_id(1);
    int localSize = get_local_size(1);

    int outIndex = flatWeightIndex / THIRD_HIDDEN_LAYER_NODES;
    int inIndex  = flatWeightIndex % THIRD_HIDDEN_LAYER_NODES;

    float partialSum = 0.0;
    float partialBiasSum = 0.0; //Only managed by the threads with weightIndex == 0.
    
    for(int batchIndex = localID; batchIndex < POSITIONS_PER_FILE; batchIndex+=localSize)  
    {
        partialSum += delta3[batchIndex * THIRD_HIDDEN_LAYER_NODES + outIndex] * h2[batchIndex * SECOND_HIDDEN_LAYER_NODES + inIndex];
        if(inIndex == 0) partialBiasSum += delta3[batchIndex * THIRD_HIDDEN_LAYER_NODES + outIndex];
    }

    __local float tempSum[64];
    __local float tempBias[64];
    tempSum[localID] = partialSum;
    if(inIndex == 0) tempBias[localID] = partialBiasSum;
    barrier(CLK_LOCAL_MEM_FENCE);

    for(int stride = localSize / 2; stride > 0; stride/=2)
    {
        if(localID < stride)
        {
            tempSum[localID] += tempSum[localID + stride];
            if(inIndex == 0) tempBias[localID] += tempBias[localID + stride];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(localID == 0)
    {
        AtomicAddFloat(&gradient3_Sum[inIndex * 32 + outIndex], tempSum[0]);
        if(inIndex == 0) AtomicAddFloat(&bias3_Sum[outIndex], tempBias[0]);
    }
}

__kernel void calculateGradient2(__global const float* delta2, //SECOND_HIDDEN_LAYER_NODES per batch sample
                                __global const float* h1, //ACCUMULATOR_NODES per batch sample
                                __global float* gradient2_Sum, //ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES shared
                                __global float* bias2_Sum) //SECOND_HIDDEN_LAYER_NODES shared
{
    int flatWeightIndex = get_global_id(0);
    int localID = get_local_id(1);
    int localSize = get_local_size(1);

    int outIndex = flatWeightIndex / 32;
    int inIndex  = flatWeightIndex % 32;

    float partialSum = 0.0;
    float partialBiasSum = 0.0; //Only managed by the threads with weightIndex == 0.
    
    for(int batchIndex = localID; batchIndex < POSITIONS_PER_FILE; batchIndex+=localSize)  
    {
        partialSum += delta2[batchIndex * SECOND_HIDDEN_LAYER_NODES + outIndex] * h1[batchIndex * ACCUMULATOR_NODES + inIndex];
        if(inIndex == 0) partialBiasSum += delta2[batchIndex * SECOND_HIDDEN_LAYER_NODES + outIndex];
    }

    __local float tempSum[64];
    __local float tempBias[64];
    tempSum[localID] = partialSum;
    if(inIndex == 0) tempBias[localID] = partialBiasSum;
    barrier(CLK_LOCAL_MEM_FENCE);

    for(int stride = localSize / 2; stride > 0; stride/=2)
    {
        if(localID < stride)
        {
            tempSum[localID] += tempSum[localID + stride];
            if(inIndex == 0) tempBias[localID] += tempBias[localID + stride];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(localID == 0)
    {
        AtomicAddFloat(&gradient2_Sum[inIndex * 32 + outIndex], tempSum[0]);
        if(inIndex == 0) AtomicAddFloat(&bias2_Sum[outIndex], tempBias[0]);
    }
}

__kernel void calculateGradient1(__global const short* activeInputs,       //contains indexes of weights of every active input in entire batch - [POSITIONS_PER_FILE][2][32] - max 32 active features
                                    __global const char* activeCount,      //how many active features per side. [POSITIONS_PER_FILE]
                                    __global const float* delta1, // ACCUMULATOR_NODES per batch sample
                                    __global float* gradient1_Sum, // ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS shared
                                    __global float* bias1_Sum)    // ACCUMULATOR_NODES_PER_SIDE shared
{
    int batchIndex = get_global_id(0); 
    int outIndex = get_global_id(1); // 0-255

    //Bias update
    float d1_w = delta1[batchIndex * ACCUMULATOR_NODES + outIndex]; //Delta contributed from first half of input nodes / accumulator.
    float d1_b = delta1[batchIndex * ACCUMULATOR_NODES + ACCUMULATOR_NODES_PER_SIDE + outIndex]; //Delta contributed from second half of input nodes / accumulator.
    AtomicAddFloat(&bias1_Sum[outIndex], d1_w + d1_b);

    for(int side = 0; side < 2; side++)
    {
        __global const short* myActiveInputs = &activeInputs[batchIndex * 64 + side * 32];
        float currentDelta = (side == 0) ? d1_w : d1_b;
        for(int i = 0; i < activeCount[batchIndex * 2]; i++)
        {
            AtomicAddFloat(&gradient1_Sum[myActiveInputs[i] * ACCUMULATOR_NODES_PER_SIDE + outIndex], currentDelta);
        }
    }
}

/******* Adam Update *******/
__kernel void adam(__global float* weights,
                    __global float* gradientSum,
                    __global float* firstMoment,
                    __global float* secondMoment,
                    float learningRate,
                    float biasCorrection1,
                    float biasCorrection2)
{
    // Flattened array of weights.
    int i = get_global_id(0);

    if(gradientSum[i] == 0) return;

    float grad = gradientSum[i] / POSITIONS_PER_FILE;

    firstMoment[i] = ADAM_BETA1 * firstMoment[i] + (1.0f - ADAM_BETA1) * grad;
    secondMoment[i] = ADAM_BETA2 * secondMoment[i] + (1.0f - ADAM_BETA2) * grad * grad;

    float correctedFirstMoment = firstMoment[i] / (1.0f - biasCorrection1);
    float correctedSecondMoment = secondMoment[i] / (1.0f - biasCorrection2);

    weights[i] -= learningRate * (correctedFirstMoment / (sqrt(correctedSecondMoment) + ADAM_EPSILON) + weights[i] * ADAM_WEIGHT_DECAY);
}