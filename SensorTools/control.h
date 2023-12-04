/*
    May be pulled in multiple times in multi-file linking
    only define once
*/
#ifndef SensorIOtools
#define SensorIOtools
// Headers and why they're included
// Document necessary compiler flags beside each header as needed in full-line comment below the header
#include <iostream> // std file descriptors
#include <unistd.h> // needed for basic getopt functionality
#include <getopt.h> // provides getopt-long() definition
#include <filesystem> // Manage I/O for writing to files rather than standard file descriptors
// May require: -lstdc++fs
#include <fstream> // for ofstream classes/types
#include <mutex> // Currently single-threaded, but permit thread safety for IO
#include <string> // String class and manipulation
#include <cstring> // strncopy(), memset()

// LibSensors and NVML demos typically allocate this much space for chip/driver names
#define NAME_BUFFER_SIZE 200
#define SensorToolsVersion "0.1.0"
// End Headers



// Class and Type declarations
// Class permits output to std::cout by default but can be flexibly redirected as needed
// Based on code generated by ChatGPT because I'm too dumb to fix C++ I/O myself
// Debug levels as enum to automatically count the range of argument values
enum DebugLevels {
DebugOFF,
DebugMinimal,
DebugVerbose
};


class Output : public std::ostream {
public:
    Output(bool direction);
    ~Output(void);
    // Safely close any existing file (except std::cout/std::cerr) and redirect this reference to a given filename
    // Falls back to std descriptor if the file cannot be opened
    void redirect(const char* fname);
    void redirect(std::filesystem::path fpath);
    // Safely close any existing file (except std::cout/std::cerr) and return output to std descriptor
    void revert();
    // Permit outputting this object to streams for debug
    friend std::ostream& operator<<(std::ostream& os, const Output& obj);

private:
    std::mutex fileMutex;
    std::ofstream fileStream;
    FILE * fileDescriptor;
    bool defaultToCout, detectedAsSudo;
    char fname[NAME_BUFFER_SIZE];
    uid_t sudo_uid;
    gid_t sudo_gid;

    bool openFile(const char *fname_);
    void closeFile(void);
};


// Argument values stored here
typedef struct argstruct {
    bool help = 0,
         cpu = 0,
         gpu = 0,
         submer = 0,
         nvme = 0,
         version = 0,
         shutdown = 0;
    short format = 0, debug = 0;
    std::filesystem::path log_path, error_log_path;
    Output log, error_log = Output(false);
    double poll = 0., initial_wait = 0., post_wait = 0.;
    std::chrono::duration<double> poll_duration, initial_duration, post_duration;
    char** wrapped;
} arguments;
// End Class and Type declarations



// Function declarations
void parse(int argc, char** argv);
// End Function declarations



// External variable declarations
extern arguments args;
extern bool update;
// End External variable declarations
#endif

