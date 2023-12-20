// Headers and why they're included
// Document necessary compiler flags beside each header as needed in full-line comment below the header
#include "control.h" // Debug levels, arguments, Output class
#ifdef BUILD_CPU
#include "cpu_tools.h"
#endif
#ifdef BUILD_GPU
#include "gpu_tools.h"
#endif
#ifdef BUILD_POD
#include "submer_tools.h"
#endif
#ifdef BUILD_NVME
#include "nvme_tools.h"
#endif

#include <iomanip> // setw and setprecision
#include <thread> // Often implies <chrono>
#include <chrono> // Given explicitly in case of above comment being untrue

#include <signal.h> // signal interrupts
#include <sys/types.h> // pid_t
#include <sys/wait.h> // fork()

