#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable

#define MINIBATCH_SIZE 16384
#define INPUT_BITS 12800
#define HALF_INPUT_BITS (INPUT_BITS / 2)
#define ACCUMULATOR_NODES 512
#define ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES / 2)
#define VECTOR_ACCUMULATOR_NODES (ACCUMULATOR_NODES / 8)
#define VECTOR_ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES_PER_SIDE / 8)
#define SECOND_HIDDEN_LAYER_NODES 32
#define THIRD_HIDDEN_LAYER_NODES 32
#define OUTPUT_BUCKETS 1

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f
#define ADAM_EPSILON 1e-8f
#define ADAM_WEIGHT_DECAY 1e-2f

#define EVAL_SCALE 400.0f

#define LOOKAHEAD_ALPHA 0.5f

/******* UTIL *******/

inline float screlu(float x) { float y = clamp(x, 0.0f, 1.0f); return y * y; }
inline float screlu_derivative(float x) { return (((x > 0.0f) & (x < 1.0f)) ? (2.0f*x) : 0.0f ); }
inline float crelu_derivative(float x) { return (float) ((x > 0.0f) & (x < 1.0f)); }
inline float sigmoid(float x) { return 1.0f / (1.0f + native_exp(-x)); }

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

//~ 0.95 ms
__kernel void calculateAccumulator(__global const short* activeInputs,       //contains indexes of weights of every active input in entire batch - [MINIBATCH_SIZE][2][32] - max 32 active features
                                    __global const float8* weights,          //shared
                                    __global const float8* bias,             //shared
                                    __global float8* output)  //contains every raw output in entire batch
{
    int groupID = get_group_id(0);  // 2 groups per batch item
    int localID = get_local_id(0);  // 0 to 31

    int batchIndex = groupID >> 1;      // groupId / 2
    int side       = groupID & 1;       // groupId % 2

    int offset = batchIndex * 64 + 32 * side;
    short myFeature = activeInputs[offset + localID];

    float8 sum = bias[localID];

    #pragma unroll
    for (int f = 0; f < 32; f++) {
        short currentFeature = sub_group_broadcast(myFeature, f);
        if(currentFeature == -1) break;
        sum += weights[currentFeature * VECTOR_ACCUMULATOR_NODES_PER_SIDE + localID];
    }

    output[(groupID * VECTOR_ACCUMULATOR_NODES_PER_SIDE) + localID] = sum;  
}

//~0.77 ms
__kernel void forwardPropagate(__global const float* accumulatorOutput,
                                __global float* h2_weights, __global float* h2_bias, __global float* h2_output,
                                __global float* h3_weights, __global float* h3_bias, __global float* h3_output,
                                __global float* output_weights, __global float* output_bias, 
                                __global char* outputBuckets,
                                __global float* outputs)
{
    int batchIndex = get_global_id(0);
    int localID = get_local_id(1); //0-63. Threads 32-63 help with loading / reduction. Threads 0-31 get an output node.

    int bucket = (int)outputBuckets[batchIndex];
    int bucketOffset = bucket * ACCUMULATOR_NODES_PER_SIDE;
    int baseIndex = batchIndex * ACCUMULATOR_NODES;

    //each local thread contributes to loading part of the input.
    //this same array will get reused later on, even if its too big to store outputs for h2/h3
    __local float shared[ACCUMULATOR_NODES];
    for(int i = localID; i < ACCUMULATOR_NODES; i+= 64)
    {
        shared[i] = screlu(accumulatorOutput[baseIndex + i]);
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
        sum = crelu(sum);


        shared[localID] = sum; //faster retrieval for use in this function.
        h2_output[batchIndex * SECOND_HIDDEN_LAYER_NODES + localID] = sum; //for use in backprop
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    //calculate h3
    sum = 0.0;
    if(localID < THIRD_HIDDEN_LAYER_NODES)
    {
        sum = h3_bias[localID];
        #pragma unroll
        for (int inputIndex = 0; inputIndex < SECOND_HIDDEN_LAYER_NODES; inputIndex++) 
        {
            sum += shared[inputIndex] * h3_weights[inputIndex * THIRD_HIDDEN_LAYER_NODES + localID];
        }
        sum = crelu(sum);
        
        shared[localID] = sum;
        h3_output[batchIndex * THIRD_HIDDEN_LAYER_NODES + localID] = sum;
    }
    barrier(CLK_LOCAL_MEM_FENCE);


    //calculate output
    sum = 0.0;
    if (localID < THIRD_HIDDEN_LAYER_NODES) 
    {
        shared[localID] = shared[localID] * output_weights[bucket * THIRD_HIDDEN_LAYER_NODES + localID];
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    // Parallel Reduction for the third layer -> output layer.
    for(int stride = 16; stride > 0; stride /= 2) 
    {
        if(localID < stride) shared[localID] += shared[localID + stride];
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(localID == 0)  
    {
        outputs[batchIndex] = sigmoid((shared[0] + output_bias[bucket]) / (EVAL_SCALE));
    }
}

/******* Backpropagation (Delta calculations) *******/

//~2.25 ms
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
                            __global const char* outputBuckets,
                            __global double* loss)
{
    int batchIndex = get_group_id(0); //0-16383
    int localID = get_local_id(0); //0-255

    __local float shared_d4;
    __local float shared_d3[32];
    __local float shared_d2[32]; 
    __local char bucket;

    //delta4
    if(localID == 0) 
    {
        float output = outputNodes[batchIndex];
        float expected = expectedOutputs[batchIndex];

        shared_d4 = output - expected;
        
        loss[batchIndex] = (double) (shared_d4 * shared_d4);
        shared_d4 *= (output * (1.0f - output)); //sigmoid derivative

        delta4[batchIndex] = shared_d4;

        bucket = outputBuckets[batchIndex];

        //if(batchIndex < 10) printf("Expected %f, Received %f", expected, output);
    } 
    barrier(CLK_LOCAL_MEM_FENCE);
    
    //delta3
    if(localID < THIRD_HIDDEN_LAYER_NODES) 
    {
        float h3_val = h3[batchIndex * THIRD_HIDDEN_LAYER_NODES + localID];
        float d3 = shared_d4 * weights4[bucket * THIRD_HIDDEN_LAYER_NODES + localID] * crelu_derivative(h3_val);
        delta3[batchIndex * THIRD_HIDDEN_LAYER_NODES + localID] = d3;
        shared_d3[localID] = d3;
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    //delta2
    if(localID < SECOND_HIDDEN_LAYER_NODES) 
    {
        float sum = 0.0f;
        #pragma unroll
        for (int outputIndex = 0; outputIndex < THIRD_HIDDEN_LAYER_NODES; outputIndex++) 
        {
            sum += shared_d3[outputIndex] * weights3[localID * THIRD_HIDDEN_LAYER_NODES + outputIndex];
        }
        float h2_val = h2[batchIndex * SECOND_HIDDEN_LAYER_NODES + localID];
        float d2 = sum * crelu_derivative(h2_val);
        delta2[batchIndex * SECOND_HIDDEN_LAYER_NODES + localID] = d2;
        shared_d2[localID] = d2;
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    //delta 1
    //512 delta / 256 threads = 2 deltas per
    for (int nodeIndex = localID; nodeIndex < ACCUMULATOR_NODES; nodeIndex+=256) 
    {
        float sum = 0.0f;
        #pragma unroll
        for (int outputIndex = 0; outputIndex < SECOND_HIDDEN_LAYER_NODES; outputIndex++) 
        {
            sum += shared_d2[outputIndex] * weights2[nodeIndex * SECOND_HIDDEN_LAYER_NODES + outputIndex];
        }
        delta1[batchIndex * ACCUMULATOR_NODES + nodeIndex] = sum * screlu_derivative(h1[batchIndex * ACCUMULATOR_NODES + nodeIndex]);
    }
}

/******* Calculate & Accumulate Edge Weight Gradients *******/

//0.5 ms
__kernel void calculateGradient4(__global const float* delta4, //1 per batch sample
                                    __global const float* h3, //THIRD_HIDDEN_LAYER_NODES per batch sample
                                    __global float* gradient4_Sum, //THIRD_HIDDEN_LAYER_NODES * OUTPUT_BUCKETS shared
                                    __global float* bias4_Sum, //8 shared
                                    __global const char* outputBuckets) //MINIBATCH_SIZE
{
    int weightIndex = get_global_id(0); //0-31
    int localID = get_local_id(1); //0-63
    float partialSum[8] = { 0.0 };
    float partialBiasSum[8] = { 0.0 }; //Only managed by the threads with weightIndex == 0.

    for(int batchIndex = localID; batchIndex < MINIBATCH_SIZE; batchIndex+=64) 
    {
        char bucket = outputBuckets[batchIndex];
        float d4 = delta4[batchIndex];

        partialSum[bucket] += d4 * h3[THIRD_HIDDEN_LAYER_NODES * batchIndex + weightIndex];
        if(weightIndex == 0) partialBiasSum[bucket] += d4;
    }

    __local float tempSum[8][64];
    __local float tempBias[8][64];
    #pragma unroll
    for(int b = 0; b < 8; b++) 
    {
        tempSum[b][localID] = partialSum[b];
        if(weightIndex == 0) tempBias[b][localID] = partialBiasSum[b];
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    for(int stride = 64 / 2; stride > 0; stride/=2)
    {
        if(localID < stride)
        {
            #pragma unroll
            for(int b = 0; b < 8; b++) 
            {
                tempSum[b][localID] += tempSum[b][localID + stride];
                if(weightIndex == 0) tempBias[b][localID] += tempBias[b][localID + stride];
            }
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(localID == 0)
    {
        #pragma unroll
        for(int b = 0; b < 8; b++) 
        {
            AtomicAddFloat(&gradient4_Sum[b * THIRD_HIDDEN_LAYER_NODES + weightIndex], tempSum[b][0]);
            if(weightIndex == 0) AtomicAddFloat(&bias4_Sum[b], tempBias[b][0]);
        }
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

//~0.85ms
__kernel void calculateGradient2(__global const float* delta2, //SECOND_HIDDEN_LAYER_NODES per batch sample
                                __global const float* h1, //ACCUMULATOR_NODES per batch sample
                                __global float* gradient2_Sum, //ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES shared
                                __global float* bias2_Sum) //SECOND_HIDDEN_LAYER_NODES shared
{

    //16,384 total threads in global group. One per nonbias weight.
    //256 (16 x 16) threads in one group, sharing local data and loading together.
    //row == 0 thread calculates bias for their output column.
    int col = get_global_id(0); // 0 to 31. Two column groups (16 per)
    int row = get_global_id(1); // 0 to 511. 32 row groups (16 per)
    int tileX = get_local_id(0); //0 to 15 (local col)
    int tileY = get_local_id(1); //0 to 15 (local row)

    __local float tile_h1[16][16];
    __local float tile_delta2[16][16];

    float accumulator = 0.0f;
    float biasAccumulator = 0.0f; 

    for (int batchIndex = 0; batchIndex < MINIBATCH_SIZE; batchIndex+=16) 
    {
        /**
          * Load into local memory.
          *
          * The extra activation here is faster than storing it activated and
          * using native_sqrt() in the derivative function.
          *
          * Tile_h1[batchIndex][row/inputs]
          *     - TileY is local row
          *     - TileX is used as a placeholder for batchIndex
          *
          * Tile_delta2[batchIndex][column/outputs]
          *     - TileX is local column
          *     - TileY is used as a placeholder for batchIndex
          *
          */

        tile_h1[tileX][tileY] = screlu(h1[(batchIndex + tileX) * ACCUMULATOR_NODES + row]); 
        tile_delta2[tileY][tileX] = delta2[(batchIndex + tileY) * 32 + col];

        barrier(CLK_LOCAL_MEM_FENCE);

        #pragma unroll
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

//4ms. Drops to 3ms if atomic adds treated as nonatomic.
__kernel void calculateGradient1(__global const short* activeInputs,       //contains indexes of weights of every active input in entire batch - [MINIBATCH_SIZE][2][32] - max 32 active features
                                    __global const float* delta1, // ACCUMULATOR_NODES per batch sample
                                    __global float* gradient1_Sum, // ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS shared
                                    __global float* bias1_Sum)    // ACCUMULATOR_NODES_PER_SIDE shared
{
    int batchIndex = get_global_id(0);
    int localID = get_local_id(1);

    // Shared storage for the 64 active features of this board position
    __local short sharedFeatures[64];
    __local float d1[512];

    sharedFeatures[localID] = activeInputs[batchIndex * 64 + localID];
    
    int offset = batchIndex * ACCUMULATOR_NODES;
    
    d1[localID]       = delta1[offset + localID];
    d1[localID + 64]  = delta1[offset + localID + 64];
    d1[localID + 128] = delta1[offset + localID + 128];
    d1[localID + 192] = delta1[offset + localID + 192];
    d1[localID + 256] = delta1[offset + localID + 256];
    d1[localID + 320] = delta1[offset + localID + 320];
    d1[localID + 384] = delta1[offset + localID + 384];
    d1[localID + 448] = delta1[offset + localID + 448];

    barrier(CLK_LOCAL_MEM_FENCE);

    int f = 0;
    while(sharedFeatures[f] != -1 && f < 32)
    {
        for(int outIndex = localID; outIndex < ACCUMULATOR_NODES_PER_SIDE; outIndex+=64)
        {
            AtomicAddFloat(&gradient1_Sum[sharedFeatures[f] * ACCUMULATOR_NODES_PER_SIDE + outIndex], d1[outIndex]);
            AtomicAddFloat(&gradient1_Sum[sharedFeatures[32 + f] * ACCUMULATOR_NODES_PER_SIDE + outIndex], d1[ACCUMULATOR_NODES_PER_SIDE + outIndex]);
        }
        f++;
    }

    //Bias
    for(int outIndex = localID; outIndex < ACCUMULATOR_NODES_PER_SIDE; outIndex+=64)
    {
        AtomicAddFloat(&bias1_Sum[outIndex], d1[outIndex] + d1[outIndex + ACCUMULATOR_NODES_PER_SIDE]);
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
    
//~0.005 ms
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

    UPDATE_ADAMW(w2, g2, m2, v2, 16384); // 512 * 32
    UPDATE_ADAMW(w3, g3, m3, v3, 1024);  // 32 * 32
    UPDATE_ADAMW(b1, gb1, mb1, vb1, 256);
    UPDATE_ADAMW(b2, gb2, mb2, vb2, 32);
    UPDATE_ADAMW(b3, gb3, mb3, vb3, 32);
    UPDATE_ADAMW(w4, g4, m4, v4, 32);
    UPDATE_ADAMW(b4, gb4, mb4, vb4, 8);
    
}

//0.28ms
//Roughly half (e.g. ~778k / 1,638,400) will be active.
__kernel void inputAdamW(__global float* w1, __global float* g1, __global float* m1, __global float* v1,
                            float lr, float biasCorrection1, float biasCorrection2, float rectificationTerm)
{
    int i = get_global_id(0);
    int size = get_global_size(0);

    UPDATE_ADAMW(w1, g1, m1, v1, size);
}

__kernel void lookahead_update(__global float* fast_weights, __global float* slow_weights)
{
    int i = get_global_id(0);
    
    slow_weights[i] += LOOKAHEAD_ALPHA * (fast_weights[i] - slow_weights[i]);
    fast_weights[i] = slow_weights[i];
}