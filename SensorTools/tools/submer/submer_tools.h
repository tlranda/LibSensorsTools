// Headers and why they're included
// Document necessary compiler flags as needed in full-line comment below the header
#include <curl/curl.h> // curl library
// Must compile with: -lcurl
#include "submer_api.h"
// Defines symbol SUBMER_URL as the http API URL for the monitor's JSON
#include "io/argparse_libsensors.h" // Debug levels, arguments, Output class
#include <nlohmann/json.hpp> // JSON parsing
//End Headers

// Class and Type declarations
typedef struct submer_cache_t {
    // IDs
    int index;
    CURL* curl_handle;
    CURLcode res_code;
    // Cached data
    char *response = NULL;
    nlohmann::json json_data;
    double initialSubmerTemperature;
    // Constructor
    submer_cache_t() {}
    // Destructor
    ~submer_cache_t() {
        if (curl_handle) curl_easy_cleanup(curl_handle);
        if (response) free(response);
    }
} submer_cache;
// End Class and Type declarations

// Function declarations
void cache_submers(void);
int update_submers(void);
// End Function declarations


// External variable declarations
extern std::vector<std::unique_ptr<submer_cache>> known_submers;
extern int submers_to_satisfy;
// End External variable declarations

