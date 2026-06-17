CC = gcc
CFLAGS = -Wall -g -std=c99 -march=native -fopenmp -MMD -MP

HIP_ROOT = $(subst \,/,$(HIP_PATH))
CFLAGS += -I"$(HIP_ROOT)/include"
LIBS = -L"$(HIP_ROOT)/lib" -lamdhip64

# Speedup, harder debugging.
ifndef DEBUG
CFLAGS += -O3
CFLAGS += -DNDEBUG
endif

# Profiling
ifdef KPERFT
	CFLAGS += -DPERFT_KERNELS
endif
# gprof ./ChessBot.exe -P_mcount_private -P__fentry__ -b > output.txt
ifdef PROF
	CFLAGS += -pg -no-pie
endif

SRC_DIR = src
TGT_DIR = target
OBJ_DIR = $(TGT_DIR)/obj

CFLAGS += -I$(SRC_DIR)

# Use "make NEW=1" to create a copy. Used for testing elo differences between versions.
ifdef NEW
	TARGET = $(TGT_DIR)/Thom_new.exe
else
	TARGET = $(TGT_DIR)/Thom.exe
endif

# Use "make NVIDIA=1" to compile on NVIDIA gpus. (untested)
ifdef NVIDIA
	CFLAGS +=-D__HIP_PLATFORM_NVIDIA__
	KC = nvcc
	KFLAGS = -O3 --ptx -D__HIP_PLATFORM_NVIDIA__
	KTARGET = $(OBJ_DIR)/gpu/kernels.nvptx
else
	CFLAGS +=-D__HIP_PLATFORM_AMD__
	KC = hipcc
	KFLAGS = -O3 --genco --offload-arch=gfx1034 -D__HIP_PLATFORM_AMD__
	KTARGET = $(OBJ_DIR)/gpu/kernels.hsaco
endif

# define PROJECT_CWD as a string literal evaluating to the current path. Used for fopen-ing.
CFLAGS += -DPROJECT_CWD="\"$(CURDIR)\""

SRCFILES = $(wildcard src/*.c) \
		   $(wildcard src/analyze/*.c) \
		   $(wildcard src/board/*.c) \
		   $(wildcard src/hashtables/*.c) \
		   $(wildcard src/pyrrhic/tbprobe.c) \
		   $(wildcard src/gpu/*.c)

OBJFILES := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCFILES))
DEPS     := $(OBJFILES:.o=.d)

OBJ_DIRS := $(sort $(dir $(OBJFILES)))

# Linux / MacOS specific libaries
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LIBS += -pthread -lm
endif
ifeq ($(UNAME_S),Darwin)
	LIBS += -pthread
endif

.PHONY: all clean directories

all: directories $(TARGET) $(KTARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c Makefile | directories
	$(CC) $(CFLAGS) -c -o $@ $<

directories:
	@mkdir -p $(OBJ_DIRS)

$(KTARGET): src/gpu/kernels.hip
	$(KC) $(KFLAGS) src/gpu/kernels.hip -o $(KTARGET)

clean:
	rm -rf $(TGT_DIR)

-include $(DEPS)