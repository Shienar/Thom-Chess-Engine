#pragma once

#if !defined(__HIP_PLATFORM_AMD__) && !defined(__HIP_PLATFORM_NVIDIA__)
	#define __HIP_PLATFORM_AMD__
#endif

#include <hip/hip_runtime_api.h>

//Profiling
#ifdef PERFT_KERNELS
	#define ENQUEUE_EVENT(event, queue) 	\
		do {								\
			hipStreamSynchronize(queue); 	\
			hipEventRecord(event, queue)	\
		} while (0);
#else
	#define ENQUEUE_EVENT(event, queue)
#endif

#define KERNEL_COUNT 9
#define MEM_COUNT 36
#define EVENT_TRACKED_KERNELS (KERNEL_COUNT - 1)
typedef struct hip_context_struct {
	int device_id;
	hipStream_t queue;
	hipModule_t module;

	union {
		struct {
			hipFunction_t calculate_accumulator;
			hipFunction_t calculate_output;
			hipFunction_t calculate_deltas;
			hipFunction_t calculate_output_gradient;
			hipFunction_t calculate_accumulator_gradient;
			hipFunction_t calculate_accumulator_bias_gradient;
			hipFunction_t adamw;
			hipFunction_t inputadamw;
			hipFunction_t lookahead;
		};
		hipFunction_t arr[KERNEL_COUNT];
	} kernels;
} hip_context_struct;

typedef struct hip_kernel_args {
	union {
		struct {
			void *active_inputs_A;
			void *expected_output_A;
			void *output_bucket_A;

			void *active_inputs_B;
			void *expected_output_B;
			void *output_bucket_B;

			void *factorizer_weights_fast;
			void *factorizer_weights_slow;

			void *accumulator_weights_fast;
			void *accumulator_weights_slow;

			void *accumulator_bias_fast;
			void *accumulator_bias_slow;

			void *output_weights_fast;
			void *output_weights_slow;

			void *output_bias_fast;
			void *output_bias_slow;

			void *accumulator_output;
			void *final_output;

			void *loss;

			void *output_delta;
			void *accumulator_delta;

			void *factorizer_weights_gradient_sum;

			void *accumulator_weights_gradient_sum;
			void *accumulator_bias_gradient_sum;

			void *output_weights_gradient_sum;
			void *output_bias_gradient_sum;

			//Adam first moments
			void *m_factorizer_weights;

			void *m_accumulator_weights;
			void *m_accumulator_bias;

			void *m_output_weights;
			void *m_output_bias;

			//Adam second moments
			void *v_factorizer_weights;

			void *v_accumulator_weights;
			void *v_accumulator_bias;

			void *v_output_weights;
			void *v_output_bias;
		};
		void *arr[MEM_COUNT];
	} mem;

} hip_kernel_args;

typedef struct hip_event_struct {
	hipEvent_t read_loss;

	union {
		struct {
			hipEvent_t calc_accum;
			hipEvent_t calculate_output;
			hipEvent_t calculate_delta;
			hipEvent_t calculate_accumulator_gradient;
			hipEvent_t calculate_accumulator_bias_gradient;
			hipEvent_t calculate_output_gradient;
			hipEvent_t dense_update;
			hipEvent_t input_update;
		};
		hipEvent_t arr[EVENT_TRACKED_KERNELS];
	} start_events;

	union {
		struct {
			hipEvent_t calc_accum;
			hipEvent_t calculate_output;
			hipEvent_t calculate_delta;
			hipEvent_t calculate_accumulator_gradient;
			hipEvent_t calculate_accumulator_bias_gradient;
			hipEvent_t calculate_output_gradient;
			hipEvent_t dense_update;
			hipEvent_t input_update;
		};
		hipEvent_t arr[EVENT_TRACKED_KERNELS];
	} end_events;
} hip_event_struct;

extern hip_context_struct hip_context;
extern hip_kernel_args hip_args;
extern hip_event_struct hip_events;

hipError_t init_kernels(const char *compiled_kernel_path,
					short **h_active_A, float **h_expected_A, char **h_bucket_A,
					short **h_active_B, float **h_expected_B, char **h_bucket_B,
					float **h_lossbuffer);
void free_kernels(void);

#define LOOKAHEAD_UPDATE(fast_weights, slow_weights, size)					\
	do {																	\
		void *lookahead_args[2] = { &fast_weights, &slow_weights };			\
		hipModuleLaunchKernel(hip_context.kernels.lookahead,				\
								(size + 32 - 1) / 32, 1, 1,					\
								32, 1, 1,									\
								0, hip_context.queue, lookahead_args, NULL); \
	} while (0)

void enqueue_kernels(int buffer_side);
void wait_for_loss_read(void);
void save_gpu_weights(void);
