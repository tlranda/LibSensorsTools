/*
    May be pulled in multiple times in multi-file linking
    only define once
*/

#ifndef LibSensorTools_OutputClass
#define LibSensorTools_OutputClass

#include "timestamp_buf.h" // TimestampStringBuf class definition
#include "../definitions.h" // NAME_BUFFER_SIZE and other definitions

#include <iostream> // std file descriptors
#include <fstream> // for ostream classes/types
#include <unistd.h> // for chown() and the uid_t, gid_t types
#include <mutex> // Currently single-threaded, but permit thread safety for IO
#include <string> // String class and manipulation
#include <cstring> // strncopy(), memset()
#include <filesystem> // Manage I/O for writing to files rather than standard file descriptors
// !! May require linker flag: -lstdc++fs

// Class permits output to std::cout by default but can be flexibly redirected as needed
// Based on code generated by ChatGPT because I'm too dumb to fix C++ I/O myself
class Output : public std::ostream {
private:
    TimestampStringBuf buffer;
    bool defaultToCout, detectedAsSudo;
    uid_t sudo_uid;
    gid_t sudo_gid;
    std::mutex fileMutex;
    std::ofstream fileStream;
    char fname[NAME_BUFFER_SIZE];

    bool detectSudo(void);
    bool openFile(const char* openName, bool exists_ok);
    void closeFile(void);
public:
    Output(void);
    Output(std::ostream& str, bool timestamped);
    Output(bool direction, bool timestamped);
    ~Output(void);
    // Permit outputting this object to streams for debug and control how output functions
    // Right-side overload
    friend std::ostream& operator<<(std::ostream& os, Output& obj);
    // Quick checks for convenience
    bool is_cout();
    bool is_cerr();
    bool is_custom();
    // Safely close any existing file (except std::cout/std::cerr) and redirect this reference to a given filename
    // Falls back to std descriptor if the file cannot be opened
    bool redirect(const char* fname, bool exists_ok);
    bool redirect(std::filesystem::path fpath, bool exists_ok);
    // Safely close any existing file (except std::cout/std::cerr) and return output to std descriptor
    void revert();
};
#endif
