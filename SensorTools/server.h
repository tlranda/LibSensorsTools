// Headers and why they're included
// Document necessary compiler flags beside each header as needed in full-line comment below the header
#include <arpa/inet.h> // Make network strings (IP addr, etc) for debug/logging
//#include <sys/types.h> // POSIX types (may not be needed)
#include <sys/socket.h> // Socket datatypes, socket operations
#include <netinet/in.h> // Define sockaddr_in struct and options
#include <sys/select.h> // FD_[SET|ISSET|ZERO], time-based structs
#include <sys/time.h> // Other time-based structs, may import the key things from select.h

#include "control_server.h" // Common headers, IO control

// Similar includes as things in sensor_tool main
#include <iomanip> // setw and setprecision
#include <thread> // Multithreading connections, often timplies <chrono>
#include <signal.h> // signal interrupts
#include <sys/types.h> // pid_t
#include <sys/wait.h> // fork()
//#include <condition_variable> // Synchronization
// End Headers

// C++20's barriers would work but my compiler doesn't have C++20 support :(
/*
class Barrier {
private:
    std::mutex guard;
    std::condition_variable cond_var;
    int desired_count, current_count, pending;
public:
    Barrier(int c) : desired_count(c), current_count(c), pending(0) {}
    void wait() {
        std::unique_lock<std::mutex> lock(guard);
        int my_pending = pending;
        if (--current_count == 0) {
            pending++;
            current_count = desired_count;
            cond_var.notify_all();
        }
        else cond_var.wait(lock, [this, my_pending] { return my_pending != pending; });
    }
}
*/

