#ifndef DATA_GENERATOR
#define DATA_GENERATOR

#include "types.h"
#include "binpack/viri_binpack.h"

void generate(const char* path);
uint32_t rng_xorshift32(uint32_t* seed);
THREAD_RETURN generateWorkerThread(THREAD_PARAM param);

#endif