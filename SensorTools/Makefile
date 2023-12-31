# Compilers
#GPUCC = nvcc # NOT USED
CPUCC = g++

# Standard Arguments
HOSTNAME = $(shell hostname)
INCLUDES = -I../submodules/nlohmann_json/single_include
BASIC_LINKS = -lm -lstdc++fs
LINKS = $(BASIC_LINKS) -lsensors -lcurl -lnvme
BASIC_FLAGS = -std=c++17 -march=native
C_FLAGS = $(BASIC_FLAGS) $(INCLUDES) $(LINKS)
STANDARD_OPT = -O3
DEBUG_OPT = -O0 -g3
GPU_LINKS = -lcuda -lcudart -lnvidia-ml
NVIDIA_FLAGS = $(C_FLAGS) -DGPU_ENABLED $(GPU_LINKS)

# GPU availability determines which flag set to use
ifeq ("$(wildcard /usr/local/cuda/include/cuda.h)","")
    ifeq ("$(wildcard /usr/include/cuda.h)","")
        FLAGS := $(C_FLAGS)
    else
        FLAGS := $(NVIDIA_FLAGS)
    endif
else
    FLAGS := $(NVIDIA_FLAGS)
endif

SENSOR: sensor_tool.cpp control.cpp cpu_tools.cpp gpu_tools.cpp submer_tools.cpp nvme_tools.cpp
	$(CPUCC) sensor_tool.cpp control.cpp cpu_tools.cpp gpu_tools.cpp submer_tools.cpp nvme_tools.cpp -o $(HOSTNAME)_sensors $(FLAGS) $(STANDARD_OPT)

SERVER: server.cp control.cpp
	$(CPUCC) sever.cpp control.cpp -o $(HOSTNAME)_server $(BASIC_FLAGS) $(BASIC_LINKS) $(STANDARD_OPT)

DEBUG: sensor_tool.cpp control.cpp cpu_tools.cpp gpu_tools.cpp submer_tools.cpp nvme_tools.cpp
	$(CPUCC) sensor_tool.cpp control.cpp cpu_tools.cpp gpu_tools.cpp submer_tools.cpp nvme_tools.cpp -o $(HOSTNAME)_sensors $(FLAGS) $(DEBUG_OPT)

SIMPLE: simple_nvme.cpp control.cpp nvme_tools.cpp
	$(CPUCC) simple_nvme.cpp control.cpp nvme_tools.cpp $(FLAGS) $(DEBUG_OPT)

