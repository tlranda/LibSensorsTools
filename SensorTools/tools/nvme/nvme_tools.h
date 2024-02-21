// Headers and why they're included
// Document necessary compiler flags beside each header as needed in full-line comment below the header
#include <vector> // vector type and operations
#include <libnvme.h> // Read NVME device temperatures
// Must compile with: -lnvme
#include "io/argparse_libsensors.h" // Debug levels, arguments, Output class
// End Headers



// Class and Type declarations
typedef struct nvme_cache_t {
    // IDs
    int index;
    struct nvme_smart_log temp_log;
    std::vector<struct nvme_smart_log> smarts;
    std::vector<int> fds;
    // Cached data
    std::vector<int> temperature;
    std::vector<int> initial_temperature;
} nvme_cache;



// Function declarations
void cache_nvme(void);
int update_nvme(void);
// End Function declarations



// External variable declarations
//extern std::vector<std::unique_ptr<nvme_cache>> known_nvme;
extern std::vector<nvme_cache> known_nvme;
extern int nvme_to_satisfy;
// End External variable declarations

