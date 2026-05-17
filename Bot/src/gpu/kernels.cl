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
#define ADAM_WEIGHT_DECAY 1e-4

#define LEAK_FACTOR 0.01f

#define EVAL_SCALE 400.0f

#define LOOKAHEAD_ALPHA 0.5f

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
    union { unsigned int intVal; float floatVal; } newVal, prevVal, currentVal;
    currentVal.floatVal = *source;
    do {
        prevVal.floatVal = currentVal.floatVal;
        newVal.floatVal = prevVal.floatVal + operand;
        currentVal.intVal = atomic_cmpxchg( (volatile __global unsigned int*)source, prevVal.intVal, newVal.intVal ); // if source == prev, source = new
    }
    while (currentVal.intVal != prevVal.intVal);
}
inline void AtomicAddFloatLocal(volatile __local float* source, const float operand )
{
    union { unsigned int intVal; float floatVal; } newVal, prevVal, currentVal;
    currentVal.floatVal = *source;
    do {
        prevVal.floatVal = currentVal.floatVal;
        newVal.floatVal = prevVal.floatVal + operand;
        currentVal.intVal = atomic_cmpxchg( (volatile __local unsigned int*)source, prevVal.intVal, newVal.intVal ); // if source == prev, source = new
    }
    while (currentVal.intVal != prevVal.intVal);
}
inline void AtomicAddDouble(volatile __global double* source, const double operand)
{
    union { ulong u64; double f64; } newVal, prevVal, currentVal;
    currentVal.f64 = *source;
    do {
        prevVal.f64 = currentVal.f64;
        newVal.f64 = prevVal.f64 + operand;
        currentVal.u64 = atom_cmpxchg( (volatile __global ulong*)source, (ulong) prevVal.u64, (ulong) newVal.u64 ); // if source == prev, source = new
    }
    while (currentVal.u64 != prevVal.u64);
}

/******* Forward Propagation *******/

//~ 3.2 ms
__kernel void calculateAccumulator(__global const short* activeInputs,       //contains indexes of weights of every active input in entire batch - [MINIBATCH_SIZE][2][32] - max 32 active features
                                    __global const float* weights,          //shared
                                    __global const float* bias,             //shared
                                    __global float* output)  //contains every activated output in entire batch
{
    int globalId = get_global_id(0); // All output nodes.
    int localId  = get_local_id(0);  // The output node for our side. (0-255)

    int totalSideIndex = globalId / ACCUMULATOR_NODES_PER_SIDE;
    int batchIndex = totalSideIndex / 2; //2 sides per batch.
    int side = totalSideIndex % 2;

    __local short mySideActiveInputs[32];
    
    if(localId < 32)   mySideActiveInputs[localId] = activeInputs[batchIndex * 64 + (side * 32) + localId];
    barrier(CLK_LOCAL_MEM_FENCE);

    float sum = bias[localId];

    #pragma unroll
    for (int activeIndex = 0; activeIndex < 32; activeIndex++)
    {
        short input = mySideActiveInputs[activeIndex];
        if (input == -1) break;
        sum += weights[input * ACCUMULATOR_NODES_PER_SIDE + localId];
    }
    output[globalId] = screlu_leaky(sum);  
}

//~0.73 ms
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

//~3.65 ms
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
    int batchIndex = get_global_id(0) / get_local_size(0);
    int localID = get_local_id(0);

    __local float shared_delta[32]; 
    __local double sse;

    //delta4
    float d4 = 0.0f;
    if(localID == 0) 
    {
        d4 = outputNodes[batchIndex] - expectedOutputs[batchIndex];

        sse = (d4 * d4);
        d4 *= 2.0f * (outputNodes[batchIndex] * (1.0f - outputNodes[batchIndex])); //sigmoid derivative, MSE derivative.
        delta4[batchIndex] = d4;
        shared_delta[0] = d4;

        //if(batchIndex < 10) printf("\nExpected %f, Received %f", expectedOutputs[batchIndex], outputNodes[batchIndex]);
    } 
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
        barrier(CLK_LOCAL_MEM_FENCE);
        shared_delta[localID] = d2;
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    //delta 1
    //512 delta / 256 threads = 2 deltas per
    for (int nodeIndex = localID; nodeIndex < ACCUMULATOR_NODES; nodeIndex+=256) 
    {
        float sum = 0.0f;
        for (int outputIndex = 0; outputIndex < SECOND_HIDDEN_LAYER_NODES; outputIndex++) 
        {
            sum += shared_delta[outputIndex] * weights2[nodeIndex * SECOND_HIDDEN_LAYER_NODES + outputIndex];
        }
        delta1[batchIndex * ACCUMULATOR_NODES + nodeIndex] = sum * screlu_leaky_derivative(h1[batchIndex * ACCUMULATOR_NODES + nodeIndex]);
    }

    //SSE accumulation
    if (localID == 0)  AtomicAddDouble(sumSquaredError, sse);
}

/******* Calculate & Accumulate Edge Weight Gradients *******/

//0.5 ms
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


//0.0006ms
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

//~1.1ms
__kernel void calculateGradient2(__global const float* delta2, //SECOND_HIDDEN_LAYER_NODES per batch sample
                                __global const float* h1, //ACCUMULATOR_NODES per batch sample
                                __global float* gradient2_Sum, //ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES shared
                                __global float* bias2_Sum) //SECOND_HIDDEN_LAYER_NODES shared
{

    //16,384 total threads in global group. One per nonbias weight.
    //256 threads in one group, sharing local data and loading together.
    //row == 0 thread calculates bias for their output column.
    int col = get_global_id(0); // 0 to 31. Two column groups
    int row = get_global_id(1); // 0 to 511. 32 row groups
    int tileX = get_local_id(0); //0 to 15 (local col)
    int tileY = get_local_id(1); //0 to 15 (local row)

    __local float tile_h1[16][16]; // batch index x row/inputs
    __local float tile_delta2[16][16]; // batch index x column/outputs

    float accumulator = 0.0f;
    float biasAccumulator = 0.0f; 

    for (int batchIndex = 0; batchIndex < MINIBATCH_SIZE; batchIndex+=16) 
    {
        // load into local memory
        tile_h1[tileY][tileX] = h1[(batchIndex + tileY) * ACCUMULATOR_NODES + row];
        tile_delta2[tileY][tileX] = delta2[(batchIndex + tileY) * 32 + col];
        barrier(CLK_LOCAL_MEM_FENCE);

        for (int batchOffset = 0; batchOffset < 16; batchOffset++)  
        {
            accumulator += tile_h1[batchOffset][tileY] * tile_delta2[batchOffset][tileX];
            if(row == 0) biasAccumulator += tile_delta2[batchOffset][tileX];
        }
        
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // Write the result out to global memory
    if (row < 512 && col < 32) 
    {
        gradient2_Sum[row * 32 + col] = accumulator;
        if(row == 0) bias2_Sum[col] = biasAccumulator;
    }
}

//~4-5ms. Drops only to 3.7ms if you treat atmoicadd as nonatomic add.
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


    //Process all 512 nodes in chunks of 64
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

#define UPDATE_ADAMW(weight, gradientSum, firstMoment, secondMoment, size) \
    if (i < size) \
    { \
        float grad = gradientSum[i] / MINIBATCH_SIZE; \
        firstMoment[i] = ADAM_BETA1 * firstMoment[i] + (1.0f - ADAM_BETA1) * grad; \
        secondMoment[i] = ADAM_BETA2 * secondMoment[i] + (1.0f - ADAM_BETA2) * grad * grad; \
        float correctedFirstMoment = firstMoment[i] / (1.0f - biasCorrection1); \
        if(rectificationTerm > 0.0f) { \
            float denominator = sqrt(secondMoment[i] / (1.0f - biasCorrection2)) + ADAM_EPSILON; \
            weight[i] -= lr * rectificationTerm * (correctedFirstMoment / denominator); \
        } else { \
            weight[i] -= lr * correctedFirstMoment; \
        } \
        weight[i] -= lr * ADAM_WEIGHT_DECAY * weight[i]; \
        gradientSum[i] = 0.0f; \
    }

//~0.006 ms
__kernel void adamW(__global float* w2, __global float* g2, __global float* m2, __global float* v2,
                    __global float* w3, __global float* g3, __global float* m3, __global float* v3,
                    __global float* w4, __global float* g4, __global float* m4, __global float* v4,
                    __global float* b1, __global float* gb1, __global float* mb1, __global float* vb1,
                    __global float* b2, __global float* gb2, __global float* mb2, __global float* vb2,
                    __global float* b3, __global float* gb3, __global float* mb3, __global float* vb3,
                    __global float* b4, __global float* gb4, __global float* mb4, __global float* vb4,
                    float lr, float biasCorrection1, float biasCorrection2, float rectificationTerm) 
{
    int i = get_global_id(0);

    //Update weights and biases in descending order based on size
    UPDATE_ADAMW(w2, g2, m2, v2, 16384); // 512 * 32
    UPDATE_ADAMW(w3, g3, m3, v3, 1024);  // 32 * 32
    UPDATE_ADAMW(b1, gb1, mb1, vb1, 256); // 256
    UPDATE_ADAMW(w4, g4, m4, v4, 32);    // 32 * 1
    UPDATE_ADAMW(b2, gb2, mb2, vb2, 32);  // 32
    UPDATE_ADAMW(b3, gb3, mb3, vb3, 32);  // 32
    UPDATE_ADAMW(b4, gb4, mb4, vb4, 1);   // 1
    
}

//0.5ms on sparse input layer. Would be 3.4ms with pown instead of native_powr
__kernel void lazyAdam(__global float* weights,
                    __global int* timestamps,
                    __global float* gradientSum,
                    __global float* firstMoment,
                    __global float* secondMoment,
                    float learningRate,
                    float rho_inf)
{
    // Flattened array of weights.
    int i = get_global_id(0);

    float grad = gradientSum[i] / MINIBATCH_SIZE;
    
    if(!grad) return;

    firstMoment[i] = ADAM_BETA1 * firstMoment[i] + (1.0f - ADAM_BETA1) * grad;
    secondMoment[i] = ADAM_BETA2 * secondMoment[i] + (1.0f - ADAM_BETA2) * grad * grad;

    int t = ++timestamps[i];
    float b1 = native_powr(ADAM_BETA1, t);
    float b2 = native_powr(ADAM_BETA2, t);
    float correctedFirstMoment = firstMoment[i] / (1.0f - b1);
 
    float rho_timestamp = rho_inf - (2.0f * t * b2) / (1.0f - b2);

    if (rho_timestamp > 5.0) 
    {
        float rectificationTerm = sqrt(((rho_timestamp - 4.0f) * (rho_timestamp - 2.0f) * rho_inf) / ((rho_inf - 4.0f) * (rho_inf - 2.0f) * rho_timestamp));
        
        float correctedSecondMoment = secondMoment[i] / (1.0f - b2);
        weights[i] -= learningRate * rectificationTerm * (correctedFirstMoment / (sqrt(correctedSecondMoment) + ADAM_EPSILON));
    } 
    else weights[i] -= learningRate * correctedFirstMoment;

    gradientSum[i] = 0.0f; 
}

__kernel void lookahead_update(__global float* fast_weights, __global float* slow_weights)
{
    int i = get_global_id(0);
    
    slow_weights[i] += LOOKAHEAD_ALPHA * (fast_weights[i] - slow_weights[i]);
    fast_weights[i] = slow_weights[i];
}