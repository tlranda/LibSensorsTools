# Compilers
GPUCC = nvcc
CPUCC = g++

# Standard Arguments
HOSTNAME = $(shell hostname)
LINKS = -lsensors -lcuda -lcudart -lnvidia-ml
C_FLAGS = -std=c++17 -O3 -lm -march=native $(LINKS) -g
NVIDIA_FLAGS = -Xcompiler "$(C_FLAGS)"

SENSOR: sensor_tool.cpp \
        safecuda.h
	$(CPUCC) sensor_tool.cpp -o $(HOSTNAME)_sensors $(C_FLAGS)

