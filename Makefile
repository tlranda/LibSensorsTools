# Compilers
#GPUCC = nvcc # NOT USED
CPUCC = g++

# Standard Arguments
HOSTNAME = $(shell hostname)
LINKS = -lsensors -lm -lstdc++fs
GPU_LINKS = -lcuda -lcudart -lnvidia-ml
C_FLAGS = -std=c++17 -O3 -march=native $(LINKS) -g
NVIDIA_FLAGS = $(C_FLAGS) -DGPU_ENABLED $(GPU_LINKS)

# GPU availability determines which flag set to use
ifeq ("$(wildcard /usr/loca/cuda/include/cuda.h)","")
    ifeq ("$(wildcard /usr/include/cuda.h)","")
        FLAGS := $(C_FLAGS)
    else
        FLAGS := $(NVIDIA_FLAGS)
    endif
else
    FLAGS := $(NVIDIA_FLAGS)
endif

SENSOR: sensor_tool.cpp safecuda.h
	$(CPUCC) sensor_tool.cpp -o $(HOSTNAME)_sensors $(FLAGS)


