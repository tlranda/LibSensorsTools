// Headers and why they're included
// Document necessary compiler flags beside each header as needed in full-line comment below the header
#include "control.h" // Debug levels, arguments, Output class
#include "cpu_tools.h" // Defined API and variables for monitoring CPUs
#include "gpu_tools.h" // Defined API and variables for monitoring GPUs
#include "submer_tools.h" // Defined API and variables for monitoring Submer Pods

#include <iostream> // cout, cerr, etc
#include <iomanip> // setw and setprecision
#include <thread> // Often implies <chrono>
#include <chrono> // Given explicitly in case of above comment being untrue

#include <signal.h> // signal interrupts
#include <sys/types.h> // pid_t
#include <sys/wait.h> // fork()

