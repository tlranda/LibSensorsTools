#include "timestamp_buf.h"

// Default Constructor
TimestampStringBuf::TimestampStringBuf(void) :
                    timestamped(false),
                    output(&std::cout) {}
// Parameterized Constructor
TimestampStringBuf::TimestampStringBuf(std::ostream& stream, bool timestamped = false) :
                    timestamped(timestamped),
                    output(&stream) {}
// Destructor cannot call virtual methods, ensure final flush always occurs
TimestampStringBuf::~TimestampStringBuf(void) {
    if (pbase() != pptr()) putOutput();
}
// Ensure buffer properly clears on synchronization
int TimestampStringBuf::sync(void) {
    putOutput();
    return 0;
}
void TimestampStringBuf::putOutput() {
    // Output the timestamp
    if (timestamped) {
        std::chrono::time_point<std::chrono::system_clock> currentTimePoint =
                std::chrono::system_clock::now();
        std::chrono::duration<long int, std::ratio<1,1'000'000'000> > ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(currentTimePoint.time_since_epoch()) % 1'000'000'000;
        std::time_t current_time =
                std::chrono::system_clock::to_time_t(currentTimePoint);
        std::tm* localTime = std::localtime(&current_time);
        (*output) << std::put_time(localTime, "[%F %T.")
                  << std::setfill('0') << std::setw(9) << ns.count() << "] ";
    }
    // Output the buffer and reset it
    (*output) << str();
    str("");
    // Flush the output stream
    output->flush();
}
// Determine where the output is currently going
std::ostream* TimestampStringBuf::getOutput() const {
    return output;
}
// Change where the output should flush
void TimestampStringBuf::changeStream(std::ostream& changedStream) {
    output = &changedStream;
}

