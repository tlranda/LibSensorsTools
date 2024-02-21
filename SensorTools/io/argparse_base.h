/*
    May be pulled in multiple times in multi-file linking
    only define once
*/

#ifndef LibSensorTools_Arguments
#define LibSensorTools_Arguments

#cmakedefine BUILD_CPU
#cmakedefine BUILD_GPU
#cmakedefine BUILD_SUBMER
#cmakedefine BUILD_NVME
#cmakedefine SERVER_MAIN

#include "output.h" // Output class definition
#include "../enums.h" // Enums for output formats, debug levels
#include "../definitions.h" // Debug levels, versioning, etc

#include <ctime> // localtime(), tm* data
#include <unistd.h> // needed for basic getopt functionality
#include <getopt.h> // provides getopt-long() definition
#include <iostream> // std file descriptors
#include <chrono> // durations and duration_casts
#include <filesystem> // filesystem path
// !! May require linker flag: -lstdc++fs

// Argument values stored here
typedef struct argstruct {
    bool help = 0,
         #ifndef SERVER_MAIN
             #ifdef BUILD_CPU
             cpu = 0,
             #endif
             #ifdef BUILD_GPU
             gpu = 0,
             #endif
             #ifdef BUILD_SUBMER
             submer = 0,
             #endif
             #ifdef BUILD_NVME
             nvme = 0,
             #endif
         #endif
         version = 0,
         shutdown = 0;
    #ifdef SERVER_MAIN
    int clients = 0;
    #else
    int connection_attempts = 10;
    #endif
    short format = 0, debug = 0;
    std::filesystem::path log_path, error_log_path;
    Output log, error_log = Output(false, true);
    double poll = 0., initial_wait = 0., post_wait = 0., timeout = -1.;
    std::chrono::duration<double> poll_duration, initial_duration, post_duration;
    char **wrapped, *ip_addr = nullptr;
    bool any_active(void) {
        bool ret = 0;
        #ifndef SERVER_MAIN
            #ifdef BUILD_CPU
            ret = ret | cpu;
            #endif
            #ifdef BUILD_GPU
            ret = ret | gpu;
            #endif
            #ifdef BUILD_SUBMER
            ret = ret | submer;
            #endif
            #ifdef BUILD_NVME
            ret = ret | nvme;
            #endif
        #endif
        return ret;
    }
    void default_active(void) {
        #ifndef SERVER_MAIN
            #ifdef BUILD_CPU
            cpu = 1;
            #endif
            #ifdef BUILD_GPU
            gpu = 1;
            #endif
            // Other tools only default when not built with CPU and not built with GPU
            #ifndef BUILD_CPU
            #ifndef BUILD_GPU
                #ifdef BUILD_SUBMER
                submer = 1;
                #endif
                #ifdef BUILD_NVME
                nvme = 1;
                #endif
            #endif
            #endif
        #endif
    }
} arguments;

void parse(int argc, char** argv);

extern arguments args;
extern bool update;
#endif

