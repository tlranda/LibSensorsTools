/*
  Some hosts do not provide NVIDIA / AMD headers etc
  Use preprocessor macros to define a safe version of
  all calls when compiler definitions (or lack thereof)
  indicates the GPU tool is unsupported.

  Current compiler definitions:
  * GPU_ENABLED - permit GPU tools to be implemented (currently
                  targets NVIDIA architecture/tools only)
*/


// Headers and why they're included
// Document necessary compiler flags beside each header as needed in full-line comment below the header
#ifdef GPU_ENABLED
#include <cuda.h> // IDK yet, but it's probably something
// Must compile with: -lcuda
#include <cuda_runtime.h> // Device management API
// Must compile with: -lcudart
#include <nvml.h> // NVML API
// Must compile with: -lnvidia-ml
#endif

#include <vector> // Vector type
#include "control.h" // Debug levels, arguments, Output class
// End Headers



// Class and Type declarations
typedef struct gpu_cache_t {
    // IDs
    int device_ID;
    char deviceName[NAME_BUFFER_SIZE] = {-1};
    #ifdef GPU_ENABLED
    nvmlDevice_t device_Handle;
    nvmlFieldValue_t temperature_field;
    // Cached data
    uint gpu_temperature, gpu_initialTemperature, mem_temperature, mem_initialTemperature, // { all degrees Celsius }
         powerUsage, powerLimit; // { both Watts }
    nvmlUtilization_t utilization; // { ui .gpu (%), .memory (%); }
    nvmlMemory_t memory; // { ull .free (bytes), .total (bytes), .used (bytes) }
    nvmlPstates_t pState; // { ui[-1-12], 12 is lowest idle, 0 is highest intensity }
    #endif
} gpu_cache;
// End Class and Type declarations



// Function declarations
void cache_gpus(void);
int update_gpus(void);
// End Function declarations



// External variable declarations
extern std::vector<gpu_cache> known_gpus;
extern int gpus_to_satisfy;
// End External variable declarations

