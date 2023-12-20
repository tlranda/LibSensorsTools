cmake_minimum_required(VERSION 3.20) # This may be overly-cautious, but is the tested version

project(LibSensorTools
    VERSION 0.1.0
    DESCRIPTION "Composite library of sensing tools"
    HOMEPAGE_URL "https://github.com/tlranda/LibSensorsTools"
    LANGUAGES CXX)

# Some basic settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# The name of our executable in CMake is libsensors, but make sure this doesn't conflict with different builds on different systems
add_executable(libsensors)
cmake_host_system_information(RESULT hostname QUERY HOSTNAME)
set_target_properties(libsensors PROPERTIES OUTPUT_NAME "${hostname}_sensors")
# Separate compilation target for the server
add_executable(libsensors_server)
set_target_properties(libsensors_server PROPERTIES OUTPUT_NAME "${hostname}_sensors_server")

target_compile_options(libsensors PUBLIC -march=native -O3)
target_compile_options(libsensors_sever PUBLIC -march=native -O3)
# Should drop -O3 for -O0 -g3 if in debug mode
target_sources(libsensors PUBLIC "sensor_tool.cpp" "control.cpp")
target_sources(libsensors_server PUBLIC "server.cpp" "control.cpp")

# Always add these
target_link_libraries(libsensors PUBLIC m stdc++fs)
target_link_libraries(libsensors_server PUBLIC m stdc++fs)
target_include_directories(libsensors PUBLIC ../submodules/nlohmann_json/single_include)
target_include_directories(libsensors_server PUBLIC ../submodules/nlohmann_json/single_include)


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
configure_file(control.h.in ../control.h)

# Installation of libsensors
install(TARGETS libsensors)

# Now re-configure for the server
add_compile_definitions(SERVER_MAIN)
configure_file(control.h.in ../control_server.h)
remove_compile_definitions(SERVER_MAIN)
# Installation of libsensors_server
install(TARGETS libsensors_server)

