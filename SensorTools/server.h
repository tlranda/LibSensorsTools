// Headers and why they're included
// Document necessary compiler flags beside each header as needed in full-line comment below the header
#include <arpa/inet.h> // Make network strings (IP addr, etc) for debug/logging
//#include <sys/types.h> // POSIX types (may not be needed)
#include <sys/socket.h> // Socket datatypes, socket operations
#include <netinet/in.h> // Define sockaddr_in struct and options
#include <sys/select.h> // FD_[SET|ISSET|ZERO], time-based structs
#include <sys/time.h> // Other time-based structs, may import the key things from select.h

#define SERVER_MAIN // Compiler flag to indicate this source operates as server main (changes how control.h operates)
#include "control_server.h" // Common headers, IO control

// Similar includes as things in sensor_tool main
#include <iomanip> // setw and setprecision
#include <thread> // Multithreading connections, often timplies <chrono>
#include <signal.h> // signal interrupts
#include <sys/types.h> // pid_t
#include <sys/wait.h> // fork()
// End Headers

