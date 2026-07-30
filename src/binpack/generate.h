#ifndef DATA_GENERATOR
#define DATA_GENERATOR

#include "types.h"
#include "binpack/viri_binpack.h"

#define MAX_POSITIONS_PER_GAME 250

void generate(const char* path);
THREAD_RETURN generateWorkerThread(THREAD_PARAM param);

#endif