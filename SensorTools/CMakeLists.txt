cmake_minimum_required(VERSION 3.20) # This may be overly-cautious, but is the tested version

project(LibSensorTools
    VERSION 0.1.0
    DESCRIPTION "Composite library of sensing tools"
    HOMEPAGE_URL "https://github.com/tlranda/LibSensorsTools"
    LANGUAGES CXX)

# Some basic settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Sources for each exectuable
# Server::
set(SERVER_SOURCES server.cpp control_server.cpp)
set(SERVER_LIBRARIES)
# ::Server
# Libsensors::
set(LIBSENSORS_SOURCES sensor_tool.cpp control_sensors.cpp)
set(LIBSENSORS_LIBRARIES)
# Options define which tools get built into libsensors
option(BUILD_CPU "Build the lm-sensors tool" ON)
option(BUILD_GPU "Build the nvml tool" OFF)
option(BUILD_POD "Build the submer pod webapi tool" OFF)
option(BUILD_NVME "Build the libnvme tool" OFF)
# These instructions set up the various files for different tools
# and the relevant linker flags etc
if (BUILD_CPU)
    file(GLOB cpu_sources cpu_tools.cpp)
    set(LIBSENSORS_SOURCES ${LIBSENSORS_SOURCES} ${cpu_sources})
    set(LIBSENSORS_LIBRARIES ${LIBSENSORS_LIBRARIES} sensors)
endif(BUILD_CPU)
if (BUILD_GPU)
    file(GLOB gpu_sources gpu_tools.cpp)
    set(LIBSENSORS_SOURCES ${LIBSENSORS_SOURCES} ${gpu_sources})
    set(LIBSENSORS_LIBRARIES ${LIBSENSORS_LIBRARIES} cuda cudart nvidia-ml)
    add_compile_definitions(libsensors GPU_ENABLED)
endif(BUILD_GPU)
if (BUILD_POD)
    file(GLOB pod_sources submer_tools.cpp)
    set(LIBSENSORS_SOURCES ${LIBSENSORS_SOURCES} ${pod_sources})
    set(LIBSENSORS_LIBRARIES ${LIBSENSORS_LIBRARIES} curl)
endif(BUILD_POD)
if (BUILD_NVME)
    file(GLOB nvme_sources nvme_tools.cpp)
    set(LIBSENSORS_SOURCES ${LIBSENSORS_SOURCES} ${nvme_sources})
    set(LIBSENSORS_LIBRARIES ${LIBSENSORS_LIBRARIES} nvme)
endif(BUILD_NVME)
# ::Libsensors

# Common compile options and linked libraries
set(COMMON_OPTIONS -march=native -O3)
set(COMMON_LIBRARIES m stdc++fs)
set(COMMON_INCLUDE_DIRS ../submodules/nlohmann_json/single_include)
add_library(CommonSettings INTERFACE)
target_compile_options(CommonSettings INTERFACE ${COMMON_OPTIONS})
target_link_libraries(CommonSettings INTERFACE ${COMMON_LIBRARIES})
target_include_directories(CommonSettings INTERFACE ${COMMON_INCLUDE_DIRS})

# Put it all together
add_executable(libsensors ${LIBSENSORS_SOURCES})
target_link_libraries(libsensors PRIVATE CommonSettings ${LIBSENSORS_LIBRARIES})
add_executable(libsensors_server ${SERVER_SOURCES})
target_link_libraries(libsensors_server PRIVATE CommonSettings ${SERVER_LIBRARIES})

# The name of our executable in CMake is libsensors, but make sure this doesn't conflict with different builds on different systems
# Customize output binary names
cmake_host_system_information(RESULT hostname QUERY HOSTNAME)
set_target_properties(libsensors PROPERTIES OUTPUT_NAME "${hostname}_sensors")
set_target_properties(libsensors_server PROPERTIES OUTPUT_NAME "${hostname}_sensors_server")

# Special header configurations
configure_file(control.h.in control.h)
configure_file(control.cpp.in control_sensors.cpp)
target_include_directories(libsensors PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
# Now re-configure for the server
set(SERVER_MAIN ON)
configure_file(control.h.in control_server.h)
configure_file(control.cpp.in control_server.cpp)
target_include_directories(libsensors_server PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
# Installation of libsensors_server
install(TARGETS libsensors libsensors_server)

