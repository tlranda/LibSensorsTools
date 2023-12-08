cmake_minimum_required(VERSION 3.25) # This may be overly-cautious, but is the tested version

project(LibSensorTools
    VERSION 0.1.0
    DESCRIPTION "Composite library of sensing tools"
    HOMEPAGE_URL "https://github.com/tlranda/LibSensorsTools"
    LANGUAGES "CXX" "CUDA")

# Some basic settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# The name of our executable in CMake is libsensors, but make sure this doesn't conflict with different builds on different systems
add_executable(libsensors)
cmake_host_system_information(RESULT hostname QUERY HOSTNAME)
set_target_properties(libsensors PROPERTIES OUTPUT_NAME "${hostname}_sensors")

target_compile_options(libsensors PUBLIC -march=native -O3)
# Should drop -O3 for -O0 -g3 if in debug mode
target_sources(libsensors PUBLIC "sensor_tool.cpp" "control.cpp")

# Always add these
target_link_libraries(libsensors PUBLIC m stdc++fs)
target_include_directories(libsensors PUBLIC ../submodules/nlohmann_json/single_include)


# Options define which tools get built in
option(BUILD_CPU "Build the lm-sensors tool" ON)
option(BUILD_GPU "Build the nvml tool" OFF)
option(BUILD_POD "Build the submer pod webapi tool" OFF)
option(BUILD_NVME "Build the libnvme tool" OFF)

# These instructions set up the various files for different tools
# and the relevant linker flags etc
if (BUILD_CPU)
    file(GLOB cpu_sources cpu_tools.cpp)
    target_link_libraries(libsensors PUBLIC sensors)
    target_sources(libsensors PUBLIC ${cpu_sources})
endif(BUILD_CPU)
if (BUILD_GPU)
    file(GLOB gpu_sources gpu_tools.cpp)
    target_link_libraries(libsensors PUBLIC cuda cudart nvidia-ml)
    add_compile_definitions(libsensors GPU_ENABLED)
    target_sources(libsensors PUBLIC ${gpu_sources})
endif(BUILD_GPU)
if (BUILD_POD)
    file(GLOB pod_sources submer_tools.cpp)
    target_link_libraries(libsensors PUBLIC curl)
    target_sources(libsensors PUBLIC ${pod_sources})
endif(BUILD_POD)
if (BUILD_NVME)
    file(GLOB nvme_sources nvme_tools.cpp)
    target_link_libraries(libsensors PUBLIC nvme)
    target_sources(libsensors PUBLIC ${nvme_sources})
endif(BUILD_NVME)

# Installation
install(TARGETS libsensors)

