// Headers and why they're included
// Document necessary compiler flags
#include <string> // C++ strings
#include <cstring> // C strings
#include <vector> // Vector types
#include <sstream> // Stream for reading lines out of the command output
#include "io/argparse_libsensors.h" // DebugLevels, OutputFormats, argstruct/args, Output, update

// End Headers


// Class and Type declarations
typedef struct omreport_cache_t {
  // ID
  int id;
  // Cached data
  const char * cmd = "omreport chassis pwrmonitoring";
  int wattReading;
  float amperageReading;
  // Functions
  int get_CLI_values(void);
} omreport_cache;


// Function declarations
void cache_omreports(void);
int update_omreports(void);
// End function declarations


// External variable declarations
extern std::vector<omreport_cache> known_omreports;
extern int omreports_to_satisfy;
// End external variable declarations

