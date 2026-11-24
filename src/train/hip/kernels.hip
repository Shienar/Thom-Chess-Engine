#include <hip/hip_runtime.h>
#include "analyze/nnue/neuralnet.h"
#include "train/train.h"

/******* UTIL *******/

__device__ inline float screlu(float x)
{
	float y = fminf(fmaxf(x, 0.0f), 1.0f);
	return y * y;
}
__device__ inline float screlu_derivative(float x)
{
	return (((x > 0.0f) && (x < 1.0f)) ? (2.0f*x) : 0.0 );
}

__device__ inline float sigmoid(float x)
{
	return 1.0f / (1.0f + __expf(-x));
}

#define QAT
#ifdef QAT
__device__ inline float fake_quantize(float val, int scale)
{
	return rintf(val * scale) / scale;
}
__device__ inline void fake_quantize4(float4* val, int scale)
{
	val->x = fake_quantize(val->x, scale);
	val->y = fake_quantize(val->y, scale);
	val->z = fake_quantize(val->z, scale);
	val->w = fake_quantize(val->w, scale);
	return;
}
#else
__device__ inline float fake_quantize(float val, float scale)
{
	return val;
}
__device__ inline void fake_quantize4(float4* val, float scale)
{
	return;
}
#endif

/******* Forward Propagation *******/

#define MULT_ADD_WEIGHT_SUM_VECTOR(sum, weight, vector_index) 								\
		float4 weight; 																		\
		float4 weight##_acc = accumulator_weights[base_index + vector_index]; 				\
		float4 weight##_fact = factorizer_weights[base_index_factorizer + vector_index]; 	\
		weight.w = weight##_acc.w + weight##_fact.w; 										\
		weight.x = weight##_acc.x + weight##_fact.x; 										\
		weight.y = weight##_acc.y + weight##_fact.y; 										\
		weight.z = weight##_acc.z + weight##_fact.z; 										\
		fake_quantize4(&weight, QA); 														\
		sum.w += weight.w; 																	\
		sum.x += weight.x; 																	\
		sum.y += weight.y; 																	\
		sum.z += weight.z

//Grid Dimensions:  MINIBATCH_SIZE * 2,		 1, 1
//Block Dimensions: 32, 1, 1
//float 4 vectors processed per thread = (ACCUMULATOR_NODES_PER_SIDE / 4) / 32 = 2
extern "C" __global__ void calculate_accumulator(const short* active_inputs, // [MINIBATCH_SIZE][2][32]
												const float4* accumulator_weights,
												const float4* factorizer_weights,
												const float4* accumulator_bias,
												float4* accumulator_output)
{
	int group_id = blockIdx.x; // 2 groups per batch item
	int local_id = threadIdx.x;

	int batch_index = group_id >> 1; // groupId / 2
	int side = group_id & 1;  // groupId % 2

	int offset = batch_index * 64 + 32 * side;

	int vector1_index = local_id;
	int vector2_index = local_id + 32;

	float4 sum1 = accumulator_bias[vector1_index];
	float4 sum2 = accumulator_bias[vector2_index];

	fake_quantize4(&sum1, QA);
	fake_quantize4(&sum2, QA);

	int base_index;
	int base_index_factorizer;

	#pragma unroll
	for (int f = 0; f < 32; f++) {
		//This is executed within a single wavefront so a shared local group buffer
		//with broadcast is slower than just loading it directly.
		short current_feature = active_inputs[offset + f];

		if (current_feature == -1)
			break;

		base_index = current_feature * VECTOR_ACCUMULATOR_NODES_PER_SIDE;
		base_index_factorizer = (current_feature % BITS_PER_KING_BUCKET) * VECTOR_ACCUMULATOR_NODES_PER_SIDE;

		MULT_ADD_WEIGHT_SUM_VECTOR(sum1, w1, vector1_index);
		MULT_ADD_WEIGHT_SUM_VECTOR(sum2, w2, vector2_index);
	}

	base_index = group_id * VECTOR_ACCUMULATOR_NODES_PER_SIDE;
	accumulator_output[base_index + vector1_index] = sum1;
	accumulator_output[base_index + vector2_index] = sum2;
}

//Grid Dimensions:  MINIBATCH_SIZE, 1, 1
//Block Dimensions: 32,			 1, 1
extern "C" __global__ void calculate_output(const float* accumulator_output,
											float* output_weights, float* output_bias,
											float* outputs, char* output_bucket)
{
	int batch_index = blockIdx.x;
	int local_id = threadIdx.x; //0-31

	int base_index = batch_index * ACCUMULATOR_NODES;

	float my_output_contribution = 0.0f;
	int bucket = output_bucket[batch_index];
	int bucket_offset = bucket * ACCUMULATOR_NODES;

	#pragma unroll
	for (int i = local_id; i < ACCUMULATOR_NODES; i+=blockDim.x)
		my_output_contribution += screlu(accumulator_output[base_index + i]) * fake_quantize(output_weights[bucket_offset + i], QB);

	__syncthreads();

	#pragma unroll
	for (int offset = 16; offset > 0; offset /= 2)
		my_output_contribution += __shfl_down(my_output_contribution, offset, 32);

	if (local_id == 0) {
		float final_output = my_output_contribution + fake_quantize(output_bias[bucket], (QA * QB));
		outputs[batch_index] = sigmoid(final_output);
	}
}

/******* Calculate Deltas *******/

//Grid Dimensions:  MINIBATCH_SIZE,			 1, 1
//Block Dimensions: ACCUMULATOR_NODES_PER_SIDE, 1, 1
extern "C" __global__ void calculate_deltas(const float* output_nodes, const float* expected_outputs,
							const float* accumulator_outputs,
							const float* output_weights,
							float* output_delta,
							float* accumulator_delta,
							float* loss,
							char* output_bucket)
{
	int batch_index = blockIdx.x;
	int local_id = threadIdx.x;

	int bucket = output_bucket[batch_index];

	__shared__ float shared_output_delta;

	//output_delta
	if (local_id == 0) {
		float output = output_nodes[batch_index];
		float expected = expected_outputs[batch_index];

		shared_output_delta = output - expected;

		loss[batch_index] = (shared_output_delta * shared_output_delta);
		shared_output_delta *= (output * (1.0f - output));

		output_delta[batch_index] = shared_output_delta;
	}
	__syncthreads();

	//accumulator_delta
	for (int node_index = local_id; node_index < ACCUMULATOR_NODES; node_index+=blockDim.x) {
		float sum = shared_output_delta * fake_quantize(output_weights[bucket * ACCUMULATOR_NODES + node_index], QB);
		accumulator_delta[batch_index * ACCUMULATOR_NODES + node_index] = sum * screlu_derivative(accumulator_outputs[batch_index * ACCUMULATOR_NODES + node_index]);
	}
}

/******* Calculate & Accumulate Edge Weight Gradients *******/

//Grid Dimensions:  OUTPUT_BUCKETS * ACCUMULATOR_NODES, 1, 1
//Block Dimensions: 32,								 1, 1
extern "C" __global__ void calculate_output_gradient(const float* output_delta,
									const float* accumulator_outputs,
									float* output_weights_gradient_sum,
									float* output_bias_gradient_sum,
									char* output_bucket)
{
	int weight_index = blockIdx.x;
	int input_node_index = weight_index % ACCUMULATOR_NODES;
	int input_node_bucket = weight_index / ACCUMULATOR_NODES;

	int local_id = threadIdx.x;

	float partial_sum = 0.0f;
	float partial_bias_sum = 0.0f;

	for (int batch_index = local_id; batch_index < MINIBATCH_SIZE; batch_index += blockDim.x) {
		float delta = output_delta[batch_index];
		int bucket = output_bucket[batch_index];

		if (bucket == input_node_bucket) {
			partial_sum += delta * screlu(accumulator_outputs[ACCUMULATOR_NODES * batch_index + input_node_index]);
			if (input_node_index == 0)
				partial_bias_sum += delta;
		}
	}

	#pragma unroll
	for (int offset = 16; offset > 0; offset /= 2) {
		partial_sum += __shfl_down(partial_sum, offset, 32);
		if (input_node_index == 0)
			partial_bias_sum += __shfl_down(partial_bias_sum, offset, 32);
	}

	if (local_id == 0) {
		output_weights_gradient_sum[weight_index] = partial_sum;
		if (input_node_index == 0) output_bias_gradient_sum[input_node_bucket] = partial_bias_sum;
	}
}

//Grid Dimensions:  ACCUMULATOR_NODES_PER_SIDE, 1, 1
//Block Dimensions: 32,						 1, 1
extern "C" __global__ void calculator_accumulator_bias_gradient(const float* accumulator_delta, float* accumulator_bias_gradient_sum)
{
	int node_index = blockIdx.x;
	int local_id = threadIdx.x;

	float sum = 0;
	for (int batch_index = local_id; batch_index < MINIBATCH_SIZE; batch_index+=blockDim.x) {
		sum += accumulator_delta[batch_index * ACCUMULATOR_NODES + node_index];
		sum += accumulator_delta[batch_index * ACCUMULATOR_NODES + ACCUMULATOR_NODES_PER_SIDE + node_index];
	}
	accumulator_bias_gradient_sum[node_index] = sum;
}

//Grid Dimensions:  MINIBATCH_SIZE, 1, 1
//Block Dimensions: 64,			 1, 1
extern "C" __global__ void calculate_accumulator_gradient(const short* active_inputs, //[MINIBATCH_SIZE][2][32]
									const float* accumulator_delta,
									float* factorizer_gradient_sum,
									float* accumulator_gradient_sum)
{
	// This is heavily bottlenecked by atomics, but it is still faster than all the other variants that I've tried.
	// A minibatch can get processed in 11ms. This is 7ms.
	// With a local size of ACCUMULATOR_NODES_PER_SIDE & less deltas per thread, it executes in 9ms.
	int batch_index = blockIdx.x;
	int local_id = threadIdx.x;
	int side_id = local_id & 31;
	int side = local_id >= 32;

	__shared__ short shared_features[64];
	shared_features[local_id] = active_inputs[batch_index * 64 + local_id];
	__syncthreads();

	int offset = batch_index * ACCUMULATOR_NODES + ACCUMULATOR_NODES_PER_SIDE * side;

	//32 threads per side handling the deltas for 256 nodes per side.
	float node_deltas[8] = {0};
	for (int i = 0; i < 8; i++)
		node_deltas[i] = accumulator_delta[offset + side_id + 32 * i];

	int f = 0;
	while (f < 32 && shared_features[f] != -1) {
		short feature_index = shared_features[f + 32 * side];
		int feature_offset = feature_index * ACCUMULATOR_NODES_PER_SIDE;
		int factorizer_feature_offset = (feature_index % BITS_PER_KING_BUCKET) * ACCUMULATOR_NODES_PER_SIDE;

		for (int i = 0; i < 8; i++) {
			atomicAdd(&accumulator_gradient_sum[feature_offset + side_id + 32 * i], node_deltas[i]);
			atomicAdd(&factorizer_gradient_sum[factorizer_feature_offset + side_id + 32 * i], node_deltas[i]);
		}

		f++;
	}
}

/******* Adam Update *******/
#define UPDATE_ADAMW(weight, gradient_sum, first_moment, second_moment, size, min, max) 				\
	if (i < size) 																						\
	{ 																									\
		float grad = gradient_sum[i] / MINIBATCH_SIZE; 													\
		first_moment[i] = ADAM_BETA1 * first_moment[i] + (1.0f - ADAM_BETA1) * grad; 					\
		second_moment[i] = ADAM_BETA2 * second_moment[i] + (1.0f - ADAM_BETA2) * grad * grad; 			\
		float corrected_first_moment = first_moment[i] / (1.0f - bias_correction_1); 					\
		if (rectification_term > 0.0f) { 																\
			float denominator = sqrtf(second_moment[i] / (1.0f - bias_correction_2)) + ADAM_EPSILON; 	\
			weight[i] -= lr * rectification_term * (corrected_first_moment / denominator); 				\
		} else weight[i] -= lr * corrected_first_moment; 												\
		weight[i] -= lr * ADAM_WEIGHT_DECAY * weight[i]; 												\
		weight[i] = fminf(max, fmaxf(min, weight[i])); 													\
		gradient_sum[i] = 0.0f; 																		\
	}

//Grid Dimensions:  output_weights_count / 32, 1, 1
//Block Dimensions: 32,				1, 1
extern "C" __global__ void adam_w(float* output_weights, float* output_gradients, float* m_output, float* v_output,
									float* acc_bias_weights, float* acc_bias_gradients, float* m_acc_bias, float* v_acc_bias,
									float* output_bias_weights, float* output_bias_gradients, float* m_output_bias, float* v_output_bias,
									float lr, float bias_correction_1, float bias_correction_2, float rectification_term)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;

	UPDATE_ADAMW(output_weights, output_gradients, m_output, v_output, (OUTPUT_BUCKETS * ACCUMULATOR_NODES), MIN_WEIGHT, MAX_WEIGHT);
	UPDATE_ADAMW(acc_bias_weights, acc_bias_gradients, m_acc_bias, v_acc_bias, ACCUMULATOR_NODES_PER_SIDE, MIN_WEIGHT, MAX_WEIGHT);
	UPDATE_ADAMW(output_bias_weights, output_bias_gradients, m_output_bias, v_output_bias, OUTPUT_BUCKETS, MIN_WEIGHT, MAX_WEIGHT);
}

//Grid Dimensions:  accumulator_weights_count / 32, 1, 1
//Block Dimensions: 32,				1, 1
extern "C" __global__ void input_adam_w(float* acc_weights, float* acc_gradients, float* m_acc, float* v_acc,
										float* factorizer_weights, float* factorizer_gradients, float* m_factorizer, float* v_factorizer,
										float lr, float bias_correction_1, float bias_correction_2, float rectification_term)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	int size = gridDim.x * blockDim.x;

	UPDATE_ADAMW(acc_weights, acc_gradients, m_acc, v_acc, size, MIN_WEIGHT / 2, MAX_WEIGHT / 2);
	UPDATE_ADAMW(factorizer_weights, factorizer_gradients, m_factorizer, v_factorizer, (BITS_PER_KING_BUCKET * ACCUMULATOR_NODES_PER_SIDE), MIN_WEIGHT / 2, MAX_WEIGHT / 2);
}

extern "C" __global__ void lookahead_update(float* fast_weights, float* slow_weights)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;;

	slow_weights[i] += LOOKAHEAD_ALPHA * (fast_weights[i] - slow_weights[i]);
	fast_weights[i] = slow_weights[i];
}
