// Headers and why they're included
// Document necessary compiler flags beside each header as needed in full-line comment below the header
#include <sensors/sensors.h> // sensor types and API
// Must compile with: -lsensors
// May require installing libsensors-devel or equivalent
#include <vector> // vector type and operations
#include <cstdio> // FILE type, fopen(), fread(), rewind(), fclose()
#include <cstring> // memset()
#include <string> // string data type
#include <filesystem> // filesystem types
// May require on some systems: -lstdc++fs
#include "io/argparse_libsensors.h" // Debug levels, arguments, Output class
// End Headers


// Class and Type declarations
typedef struct cpu_cache_t {
    // IDs
    char chip_name[NAME_BUFFER_SIZE] = {0};
    int nr = 0;
    const sensors_chip_name* name = nullptr;
    // Cached data
    std::vector<const sensors_feature*> features;
    std::vector<const sensors_subfeature*> subfeatures;
    std::vector<double> temperature;
    std::vector<double> initial_temperature;
} cpu_cache;


// Combination of cached file pointer and last-read frequency value
typedef struct cpu_freq_cache_t {
    FILE * fhandle;
    int coreid, hz;
} freq_cache;
// End Class and Type declarations


// Function declarations
void cache_cpus(void);
int update_cpus(void);
// End Function declarations



// External variable declarations
extern std::vector<cpu_cache> known_cpus;
extern int cpus_to_satisfy;
extern std::vector<freq_cache> known_freqs;
// End External variable declarations

