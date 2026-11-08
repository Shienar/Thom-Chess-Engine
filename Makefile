# Compiler and flags
CC = gcc
CFLAGS = -Wall -std=c99 -mavx2 -fopenmp -MMD -MP

# Add debug or optimization flags to compiler flags
ifdef DEBUG
	CFLAGS += -g
else

ifdef VERIFY
$(error VERIFY cannot be compiled without DEBUG)	
endif
	CFLAGS += -O3
	CFLAGS += -DNDEBUG
	CFLAGS += -static
endif

# NNUE validation.
ifdef VERIFY
	CFLAGS += -DVERIFY
endif

ifdef SEARCHINFO
	CFLAGS += -DSEARCHINFO
endif

# Define SPSA to compile with extra uci options.
ifdef SPSA
	CFLAGS += -DSPSA
endif

# Declare & include source/target.
SRC_DIR = src
TGT_DIR = target
OBJ_DIR = $(TGT_DIR)/obj

CFLAGS += -I$(SRC_DIR)

# Simple method to compile a copy for SPRT testing.
ifdef NEW
	TARGET = $(TGT_DIR)/Thom_new.exe
else
	TARGET = $(TGT_DIR)/Thom.exe
endif

SRCFILES = $(wildcard src/*.c) \
		   $(wildcard src/analyze/*.c) \
		   $(wildcard src/board/*.c) \
		   $(wildcard src/transposition/*.c) \
		   $(wildcard src/pyrrhic/tbprobe.c) \
 		   $(wildcard src/binpack/*c) \
		   $(wildcard src/analyze/hce/*.c) \
		   $(wildcard src/analyze/nnue/*.c)

# Note: NVIDIA HIP has not been tested.
ifdef TRAIN
	SRCFILES += $(wildcard src/train/*.c)
	CFLAGS += -DTRAIN
	ifdef HIP_PATH

		SRCFILES += $(wildcard src/train/hip/*.c)

		HIP_ROOT = $(subst \,/,$(HIP_PATH))
		CFLAGS += -I"$(HIP_ROOT)/include" 
		LIBS += -L"$(HIP_ROOT)/lib"
		KFLAGS = -ffast-math -O3 --genco --offload-arch=native -mprintf-kind=buffered 
		KC = hipcc
		

		ifneq ($(shell which nvcc 2>/dev/null),)
			CFLAGS +=  -D__HIP_PLATFORM_NVIDIA__
			LIBS += -lcuda -lcudart
			KTARGET = $(OBJ_DIR)/train/hip/kernels.ptx
		else
			CFLAGS += -D__HIP_PLATFORM_AMD__
			LIBS += -lamdhip64
			KTARGET = $(OBJ_DIR)/train/hip/kernels.hsaco
		endif


	else
		SRCFILES += $(wildcard src/train/opencl/*.c)
		CFLAGS += -DUSE_OPENCL
		LIBS += -lOpenCL
		CFLAGS := $(filter-out -static,$(CFLAGS))
	endif
	
	ifdef KPERFT
		CFLAGS += -DPERFT_KERNELS
	endif
endif

ASMFILES = $(wildcard src/incbin/*.s)

OBJFILES := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCFILES))
ASMOBJFILES := $(patsubst $(SRC_DIR)/%.s, $(OBJ_DIR)/%.o, $(ASMFILES))
DEPS     := $(OBJFILES:.o=.d)
OBJFILES += $(ASMOBJFILES)

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

ifdef TRAIN
all: directories $(TARGET) $(KTARGET)
else
all: directories $(TARGET)
endif

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c -o $@ $< $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s import/weights.bin | directories
	$(CC) $(CFLAGS) -c -o $@ $< $(LIBS)

directories:
	@mkdir -p $(OBJ_DIRS)

$(KTARGET): src/train/hip/kernels.hip
	$(KC) $(KFLAGS) src/train/hip/kernels.hip -o $(KTARGET)

ifdef TRAIN
clean:
	rm -f $(OBJFILES) $(DEPS) $(TARGET) $(KTARGET)
else 
clean:
	rm -f $(OBJFILES) $(DEPS) $(TARGET)
endif

-include $(DEPS)