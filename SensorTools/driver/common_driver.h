#cmakedefine BUILD_CPU
#cmakedefine BUILD_GPU
#cmakedefine BUILD_SUBMER
#cmakedefine BUILD_NVME
#cmakedefine SERVER_MAIN
// Headers and why they're included
// Document necessary compiler flags as needed in full-line comment below the header
#ifdef SERVER_MAIN
#include "../io/argparse_server.h" // Common headers, IO control

#include <netinet/in.h> // Define sockaddr_in struct and options
#include <sys/select.h> // FD_[SET|ISSET|ZERO], time-based structs
#include <sys/time.h> // Other time-based structs, may import the key things from select.h
#else
#include "../io/argparse_libsensors.h" // Common headers, IO control
#endif

#include <iomanip> // setw and setprecision
#include <thread> // often implies <chrono>
#include <chrono> // given explicitly in case the above comment is untrue -- needed for timespec structs
#include <signal.h> // signal interrupts
#include <sys/types.h> // pid_t type
#include <sys/wait.h> // fork()
#include <arpa/inet.h> // Make network strings (IP addr, etc) for debug/logging
#include <sys/socket.h> // Socket datatypes, socket operations
#include <nlohmann/json.hpp> // JSON data type

#ifdef BUILD_CPU
#include "../tools/cpu/cpu_tools.h"
#endif
#ifdef BUILD_GPU
#include "../tools/gpu/gpu_tools.h"
#endif
#ifdef BUILD_SUBMER
#include "../tools/submer/submer_tools.h"
#endif
#ifdef BUILD_NVME
#include "../tools/nvme/nvme_tools.h"
#endif

// End Headers

// Class and Type declarations
// End Class and Type declarations

// Function declarations
int main(int argc, char** argv);
// CALLED FROM MAIN
void init_libsensorstools(int argc, char** argv);
// Sub-calls of init_libsensorstools
void print_csv_header();
void main_loop();
// Sub-calls of main_loop
void init_timing();
void set_initial_temperatures();
int poll_cycle(std::chrono::time_point<std::chrono::system_clock> t0);
void client_connect_loop();
void simple_poll_cycle_loop();
int get_n_to_satisfy();
void fork_join_loop();
void fork_join_parent_poll(pid_t pid, std::chrono::time_point<std::chrono::system_clock> t0, int satisfy);
// Helpers that may be called at other times
void shutdown(int signal);
// End Function declarations

// External variable declarations
extern std::chrono::time_point<std::chrono::system_clock> t_minus_one;
#ifdef SERVER_MAIN
extern int master_socket;
extern std::vector<int> client_sockets;
#endif
// End External variable declarations

