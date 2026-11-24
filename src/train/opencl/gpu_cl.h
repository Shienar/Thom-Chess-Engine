#pragma once

#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>

//Profiling
#ifdef PERFT_KERNELS
	#define ENQUEUE_EVENT(x) (&x)
#else
	#define ENQUEUE_EVENT(x) NULL
#endif

#define KERNEL_COUNT 9
#define MEM_COUNT 42
typedef struct opencl_context_struct {
	cl_platform_id platform;
	cl_device_id device;
	cl_context context;
	cl_command_queue queue;
	cl_program program;

	union {
		struct {
			cl_kernel calculate_accumulator;
			cl_kernel calculate_output;
			cl_kernel calculate_deltas;
			cl_kernel calculate_output_gradient;
			cl_kernel calculate_accumulator_gradient;
			cl_kernel calculate_accumulator_bias_gradient;
			cl_kernel adamw;
			cl_kernel inputadamw;
			cl_kernel lookahead;
		};
		cl_kernel arr[KERNEL_COUNT];
	} kernels;
} opencl_context_struct;

typedef struct cl_kernel_args {
	union {
		struct {
			cl_mem host_active_inputs_A;
			cl_mem host_expected_output_A;
			cl_mem host_output_bucket_A;

			cl_mem host_active_inputs_B;
			cl_mem host_expected_output_B;
			cl_mem host_output_bucket_B;

			cl_mem active_inputs_A;
			cl_mem expected_output_A;
			cl_mem output_bucket_A;

			cl_mem active_inputs_B;
			cl_mem expected_output_B;
			cl_mem output_bucket_B;

			cl_mem factorizer_weights_fast;
			cl_mem factorizer_weights_slow;

			cl_mem accumulator_weights_fast;
			cl_mem accumulator_weights_slow;

			cl_mem accumulator_bias_fast;
			cl_mem accumulator_bias_slow;

			cl_mem output_weights_fast;
			cl_mem output_weights_slow;

			cl_mem output_bias_fast;
			cl_mem output_bias_slow;

			cl_mem accumulator_output;
			cl_mem final_output;

			cl_mem loss;

			cl_mem output_delta;
			cl_mem accumulator_delta;

			cl_mem factorizer_weights_gradient_sum;

			cl_mem accumulator_weights_gradient_sum;
			cl_mem accumulator_bias_gradient_sum;

			cl_mem output_weights_gradient_sum;
			cl_mem output_bias_gradient_sum;

			//Adam first moments
			cl_mem m_factorizer_weights;

			cl_mem m_accumulator_weights;
			cl_mem m_accumulator_bias;

			cl_mem m_output_weights;
			cl_mem m_output_bias;

			//Adam second moments
			cl_mem v_factorizer_weights;

			cl_mem v_accumulator_weights;
			cl_mem v_accumulator_bias;

			cl_mem v_output_weights;
			cl_mem v_output_bias;
		};
		cl_mem arr[MEM_COUNT];
	} mem;

} cl_kernel_args;

typedef struct opencl_events {
	cl_event read_loss;

	cl_event calc_accum;
	cl_event calculate_output;
	cl_event calculate_delta;
	cl_event calculate_accumulator_gradient;
	cl_event calculate_accumulator_bias_gradient;
	cl_event calculate_output_gradient;
	cl_event dense_update;
	cl_event input_update;

} opencl_events;

extern opencl_context_struct opencl_context;
extern cl_kernel_args cl_args;
extern opencl_events cl_events;

int init_kernels(const char *kernel_path,
					short **h_active_A, float **h_expected_A, char **h_bucket_A,
					short **h_active_B, float **h_expected_B, char **h_bucket_B,
					float **h_lossbuffer);
void free_kernels(void);

#define LOOKAHEAD_UPDATE(fast_weights, slow_weights, size)																						\
	do {																																		\
		size_t local_size = 32;																													\
		size_t global_size = 32 * ((size + 32 - 1) / 32);																						\
																																				\
		clSetKernelArg(opencl_context.kernels.lookahead, 0, sizeof(cl_mem), &(fast_weights));													\
		clSetKernelArg(opencl_context.kernels.lookahead, 1, sizeof(cl_mem), &(slow_weights));													\
		clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.lookahead, 1, NULL, &global_size, &local_size, 0, NULL, NULL);		\
	} while (0)

void enqueue_kernels(int buffer_side);
void wait_for_loss_read(void);
void save_gpu_weights(void);
