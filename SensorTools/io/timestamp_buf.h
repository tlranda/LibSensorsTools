
/*
    May be pulled in multiple times in multi-file linking
    only define once
*/

#ifndef LibSensorTools_TimestampBufClass
#define LibSensorTools_TimestampBufClass

#include <iostream> // std file descriptors
#include <fstream> // for ostream classes/types
#include <sstream> // stringbuf
#include <chrono> // chrono namespace, time_point, etc
#include <iomanip> // setw, setfill manipulators

// Buffering for injecting timestamps with flush
// Implemented based on SO answer: https://stackoverflow.com/a/2212940/13189459
class TimestampStringBuf : public std::stringbuf {
    private:
        std::ostream* output;
        bool timestamped;
    public:
        TimestampStringBuf(void);
        TimestampStringBuf(std::ostream& stream, bool timestamped);
        ~TimestampStringBuf(void);
        virtual int sync(void);
        void putOutput(void);
        std::ostream* getOutput(void) const;
        void changeStream(std::ostream& changedStream);
};
#endif

