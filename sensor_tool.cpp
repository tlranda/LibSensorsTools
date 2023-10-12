#include <iostream> // cout, cerr, etc
#include <iomanip> // setw and setprecision
#include <thread> // Often implies <chrono>
#include <chrono> // Given explicitly in case of above comment being untrue

#include <unistd.h> // Getopt
#include <getopt.h> // Getopt-long
#include <signal.h> // signal interrupts
#include <sensors/sensors.h> // Must compile with -lsensors
#include <vector> // Cache chips/gpus for repeated lookup
#include <filesystem> // Manage I/O for writing to files rather than standard file descriptors
#include <fstream> // for ofstreams
#include <mutex> // Let's just be safe for once

#include <cuda.h> // Must compile with -lcuda
#include <cuda_runtime.h> // Must compile with -lcudart
#include <nvml.h> // Must compile with -lnvidia-ml

// safecuda includes <cuda.h>, <cuda_runtime.h>, <nvrtc.h>, <stdlib.h>, <stdio.h>, <cublas_v2.h>
#include "safecuda.h" // Macros to do safe cuda calls

// LibSensors and NVML demos typically allocate this much space for chip/driver names
#define NAME_BUFFER_SIZE 200

// Debug levels as enum to automatically count the range of argument values
enum DebugLevels {
DebugOFF,
DebugMinimal,
DebugVerbose
};

// Class permits output to std::cout by default but can be flexibly redirected as needed
// Based on code generated by ChatGPT because I'm too dumb to fix C++ I/O myself
class Output : public std::ostream {
public:
    Output() : std::ostream(std::cout.rdbuf()), fileStream() {
        std::lock_guard<std::mutex> lock(fileMutex);
    }
    ~Output() {
        std::lock_guard<std::mutex> lock(fileMutex);
        closeFile();
    }

    // Safely close any existing file (except std::cout) and redirect this reference to a given filename
    // Falls back to std::cout if the file cannot be opened
    void redirect(const char* fname) {
        std::lock_guard<std::mutex> lock(fileMutex);
        if (openFile(fname)) rdbuf(fileStream.rdbuf());
        else rdbuf(std::cout.rdbuf());
    }
    void redirect(std::filesystem::path fpath) {
        redirect(fpath.string().c_str());
    }
    // Safely close any existing file (except std::cout) and return output to std::cout
    void revert() {
        std::lock_guard<std::mutex> lock(fileMutex);
        closeFile();
        rdbuf(std::cout.rdbuf());
    }

private:
    std::mutex fileMutex;
    std::ofstream fileStream;

    bool openFile(const char *fname) {
        closeFile();
        fileStream.open(fname, std::ios::out | std::ios::app);
        if (fileStream.is_open()) return true;
        else {
            std::cerr << "Failed to open file " << fname << std::endl;
            return false;
        }
    }

    void closeFile() {
        if (fileStream.is_open() && &fileStream != &std::cout)
            fileStream.close();
    }
};

// Argument values stored here
typedef struct argstruct {
    short help = 0,
          cpu = 0,
          gpu = 0,
          debug = 0,
          format = 0;
    std::filesystem::path log_path;
    Output log;
    double poll = 0.;
    std::chrono::duration<double> duration;
} arguments;
// Arguments have global scope to permit reference during shutdown (debug values, file I/O, etc)
arguments args;



/* Read command line and parse arguments
   Pointer args used to store semantic settings for program execution
 */
void parse(int argc, char** argv) {
    char* PROGNAME = argv[0];
    int c, bad_args = 0;

    // Getopt option declarations
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"cpu", no_argument, 0, 'c'},
        {"gpu", no_argument, 0, 'g'},
        {"full-format", no_argument, 0, 'f'},
        {"log", required_argument, 0, 'l'},
        {"poll", required_argument, 0, 'p'},
        {"debug", required_argument, 0, 'd'},
        {0,0,0,0}
    };
    const char* optionstr = "hcgfl:p:d:";
    // Disable getopt's automatic error message -- we'll catch it via the '?' return and shut down
    opterr = 0;

    // Parsing loop
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, optionstr, long_options, &option_index);
        if (c == -1) break;
        switch (c) {
            // This case may be deprecated -- blindly following a demo that includes it for some reason
            case 0:
                std::cerr << "Weird option " << long_options[option_index].name;
                if (optarg) std::cerr << " with arg " << optarg;
                std::cerr << std::endl;
                break;
            case 'h':
                std::cout << "Usage: " << PROGNAME << " [options]" << std::endl;
                std::cout << "\t-h | --help\n\t\t" <<
                             "Print this help message and exit" << std::endl;
                std::cout << "\t-c | --cpu\n\t\t" <<
                             "Query CPU stats only (default: CPU and GPU)" << std::endl;
                std::cout << "\t-g | --gpu\n\t\t" <<
                             "Query GPU stats only (default: GPU and CPU)" << std::endl;
                std::cout << "\t-f | --full-format\n\t\t" <<
                             "Output in full text rather than CSV format" << std::endl;
                std::cout << "\t-l [file] | --log [file]\n\t\t" <<
                             "File to write output to" << std::endl;
                std::cout << "\t-p [interval] | --poll [interval]\n\t\t" <<
                             "Floating point interval in seconds to poll stats (interval > 0)" << std::endl;
                std::cout << "\t-d [level] | --debug [level]\n\t\t" <<
                             "Debug verbosity (default: 0, maximum: 1)" << std::endl;
                exit(EXIT_SUCCESS);
            case 'c':
                args.cpu = 1;
                break;
            case 'g':
                args.gpu = 1;
                break;
            case 'f':
                args.format = 1;
                break;
            case 'l':
                args.log_path = std::filesystem::path(optarg);
                args.log.redirect(args.log_path);
                break;
            case 'p':
                args.poll = atof(optarg);
                if (args.poll < 0) {
                    std::cerr << "Invalid setting for " << argv[optind-2] << ": " << optarg <<
                                 "\n\tPolling duration must be greater than 0" << std::endl;
                    bad_args += 1;
                }
                else {
                    // Set sleep time for nanosleep
                    args.duration = static_cast<std::chrono::duration<double>>(args.poll);
                }
                break;
            case 'd':
                args.debug = atoi(optarg);
                if (args.debug < DebugOFF || args.debug > DebugVerbose) { // Invalid debug levels
                    std::cerr << "Invalid setting for " << argv[optind-2] << ": " << optarg <<
                                 "\n\tValid levels are integers between " << DebugOFF <<
                                 " and " << DebugVerbose << std::endl;
                    bad_args += 1;
                }
                break;
            case '?':
                std::cerr << "Unrecognized argument: " << argv[optind-1] << std::endl;
                bad_args += 1;
                break;
        }
    }
    if (optind < argc) {
        std::cerr << "Unrecognized additional arguments!" << std::endl << "\t";
        for (int i = optind; i < argc; i++) {
            std::cerr << argv[i] << " ";
        }
        std::cerr << std::endl;
        bad_args += 1;
    }

    // Post-reading logic
    if (!args.cpu && !args.gpu) { // Default: Handle both CPU and GPU
        args.cpu = 1;
        args.gpu = 1;
    }
    if (bad_args > 0) {
        exit(EXIT_FAILURE);
    }
}


typedef struct cpu_cache_t {
    // IDs
    char chip_name[NAME_BUFFER_SIZE] = {0};
    int nr = 0;
    const sensors_chip_name* name = nullptr;
    // Cached data
    std::vector<const sensors_feature*> features;
    std::vector<const sensors_subfeature*> subfeatures;
    std::vector<double> last_read;
} cpu_cache;

std::vector<cpu_cache> known_cpus;


void cache_cpus(void) {
    // No caching if we aren't going to query the CPUs
    if (!args.cpu) return;

    int nr_name = 0, nr_feature;
    sensors_subfeature_type nr_subfeature = SENSORS_SUBFEATURE_TEMP_INPUT;
    double value;

    // Exits when no additional chips can be read from sensors library
    while (1) {
        // Reset sub-iterators
        nr_feature = 0;
        nr_subfeature = SENSORS_SUBFEATURE_TEMP_INPUT;

        // Prepare candidate
        cpu_cache candidate;
        candidate.nr = nr_name;
        const sensors_chip_name* temp_name = sensors_get_detected_chips(nullptr, &nr_name);

        // No more chips to read -- exit function
        if (!temp_name) return;

        // Clear and copy into candidate
        memset(candidate.chip_name, 0, NAME_BUFFER_SIZE);
        sensors_snprintf_chip_name(candidate.chip_name, NAME_BUFFER_SIZE, temp_name);
        candidate.name = temp_name;
        if (args.debug >= DebugVerbose) {
            args.log << "Begin caching chip " << candidate.chip_name << std::endl;
        }

        // Feature determination
        const sensors_feature* temp_feature = sensors_get_features(temp_name, &nr_feature);
        while(temp_feature) {
            if (args.debug >= DebugVerbose) {
                args.log << "\tInspect feature " << nr_feature << " with type " << temp_feature->type << " (hit on type == " << SENSORS_FEATURE_TEMP << ")" << std::endl;
            }
            // We only care about this type of feature
            if (temp_feature->type == SENSORS_FEATURE_TEMP) {
                candidate.features.push_back(temp_feature);
                // Skip directly to input subfeature value
                const sensors_subfeature* temp_subfeature = sensors_get_subfeature(temp_name, temp_feature, nr_subfeature);
                if (args.debug >= DebugVerbose) {
                    args.log << "\t\tFeature hit. Acquiring temperature subfeature " << nr_subfeature << std::endl;
                }
                candidate.subfeatures.push_back(temp_subfeature);
                sensors_get_value(temp_name, temp_subfeature->number, &value);
                if (args.debug >= DebugVerbose) {
                    args.log << "\t\t\tTemperature value read: " << value << std::endl;
                }
                candidate.last_read.push_back(value);
            }
            temp_feature = sensors_get_features(temp_name, &nr_feature);
        }
        if (args.debug >= DebugVerbose) {
            args.log << "Finished inspecting chip " << candidate.chip_name;
        }
        if (!candidate.last_read.empty()) {
            known_cpus.push_back(candidate);
            if (args.debug >= DebugVerbose) {
                args.log << " , added to known CPUs" << std::endl;
            }
        }
        else if (args.debug >= DebugVerbose) {
            args.log << " , but discarded due to empty temperature reads" << std::endl;
        }
    }
}


void update_cpus(void) {
    for (std::vector<cpu_cache>::iterator i = known_cpus.begin(); i != known_cpus.end(); i++)
        for (int j = 0; j < i->last_read.size(); j++) {
            double prev = i->last_read[j];
            sensors_get_value(i->name, i->subfeatures[j]->number, &i->last_read[j]);
            if (args.debug >= DebugVerbose) {
                args.log << "Chip " << i->chip_name << " temp BEFORE " << prev << " NOW " << i->last_read[j] << std::endl;
            }
        }
}


typedef struct gpu_cache_t {
    // IDs
    int device_ID;
    nvmlDevice_t device_Handle;
    // Cached data
    uint temperature, powerUsage, powerLimit; // { degrees Celsius, Watts, Watts }
    nvmlUtilization_t utilization; // { ui .gpu (%), .memory (%); }
    nvmlMemory_t memory; // { ull .free (bytes), .total (bytes), .used (bytes) }
    nvmlPstates_t pState; // { ui[0-12], 12 is lowest idle, 0 is highest intensity }
} gpu_cache;

std::vector<gpu_cache> known_gpus;


void cache_gpus(void) {
    // No caching if we aren't going to query the GPUs
    if (!args.gpu) return;

    int n_devices;
    CHECK_CUDA_ERROR(cudaGetDeviceCount(&n_devices));

    for (int i = 0; i <= n_devices; i++) {
        if (args.debug >= DebugVerbose) {
            args.log << "Begin caching GPU " << i << std::endl;
        }
        // Prepare candidate
        gpu_cache candidate;
        candidate.device_ID = i;
        nvmlDeviceGetHandleByIndex_v2(static_cast<unsigned int>(i), &candidate.device_Handle);

        // Initial value caching
        nvmlDeviceGetTemperature(candidate.device_Handle, NVML_TEMPERATURE_GPU, &candidate.temperature);
        nvmlDeviceGetPowerUsage(candidate.device_Handle, &candidate.powerUsage);
        nvmlDeviceGetEnforcedPowerLimit(candidate.device_Handle, &candidate.powerLimit);
        nvmlDeviceGetUtilizationRates(candidate.device_Handle, &candidate.utilization);
        nvmlDeviceGetMemoryInfo(candidate.device_Handle, &candidate.memory);
        nvmlDeviceGetPerformanceState(candidate.device_Handle, &candidate.pState);

        if (args.debug >= DebugVerbose) {
            args.log << "Finished caching GPU " << i << std::endl;
        }
        known_gpus.push_back(candidate);
    }
}

void update_gpus(void) {
    for (std::vector<gpu_cache>::iterator i = known_gpus.begin(); i != known_gpus.end(); i++) {
        if (args.debug >= DebugVerbose) {
            args.log << "GPU " << i->device_ID << " BEFORE" << std::endl;
            args.log << "\tTemperature: " << i->temperature << std::endl;
            args.log << "\tPower Usage/Limit: " << i->powerUsage << " / " << i->powerLimit << std::endl;
            args.log << "\tUtilization: " << i->utilization.gpu << "\% GPU " << i->utilization.memory << "\% Memory " << std::endl;
            args.log << "\tPerformance State: " << i->pState << std::endl;
        }
        nvmlDeviceGetTemperature(i->device_Handle, NVML_TEMPERATURE_GPU, &i->temperature);
        nvmlDeviceGetPowerUsage(i->device_Handle, &i->powerUsage);
        nvmlDeviceGetEnforcedPowerLimit(i->device_Handle, &i->powerLimit);
        nvmlDeviceGetUtilizationRates(i->device_Handle, &i->utilization);
        nvmlDeviceGetMemoryInfo(i->device_Handle, &i->memory);
        nvmlDeviceGetPerformanceState(i->device_Handle, &i->pState);
        if (args.debug >= DebugVerbose) {
            args.log << "GPU " << i->device_ID << " AFTER" << std::endl;
            args.log << "\tTemperature: " << i->temperature << std::endl;
            args.log << "\tPower Usage/Limit: " << i->powerUsage << " / " << i->powerLimit << std::endl;
            args.log << "\tUtilization: " << i->utilization.gpu << "\% GPU " << i->utilization.memory << "\% Memory " << std::endl;
            args.log << "\tPerformance State: " << i->pState << std::endl;
        }
    }
}


void print_header(void) {
}


// Cleanup calls should be based on globally available information; process-killing interrupts will go through this function
void shutdown(int s = 0) {
    if (args.debug >= DebugMinimal) args.log << "Run shutdown with signal " << s << std::endl;
    nvmlShutdown();
    sensors_cleanup();
    if (args.debug >= DebugMinimal) args.log << "Shutdown clean. Exiting..." << std::endl;
    exit(EXIT_SUCCESS);
}


int main(int argc, char** argv) {
    // Library initializations
	auto const error = sensors_init(NULL);
	if(error != 0) {
		args.log << "LibSensors library did not initialize properly! Aborting..." << std::endl;
		exit(EXIT_FAILURE);
	}
    nvmlInit();

    // Prepare shutdown via CTRL+C or other signals
    struct sigaction sigHandler;
    sigHandler.sa_handler = shutdown;
    sigemptyset(&sigHandler.sa_mask);
    sigHandler.sa_flags = 0;
    sigaction(SIGINT, &sigHandler, NULL);
    sigaction(SIGABRT, &sigHandler, NULL);
    sigaction(SIGTERM, &sigHandler, NULL);
    // SIGKILL and SIGSTOP cannot be caught, blocked, or ignored

    // Command line argument parsing
    parse(argc, argv);
    if (args.debug >= DebugVerbose) {
        args.log << "The program lives" << std::endl;
    }
    if (args.debug >= DebugMinimal) {
        args.log << "Arguments evaluate to" << std::endl <<
        "Help: " << args.help << std::endl <<
        "CPU: " << args.cpu << std::endl <<
        "GPU: " << args.gpu << std::endl;
        if (args.log_path.empty()) args.log << "Log: --" << std::endl;
        else args.log << "Log: " << args.log_path << std::endl;
        args.log << "Poll: " << args.poll << std::endl <<
        "Debug: " << args.debug << std::endl;
    }

    // Denote library versions
    if (args.debug >= DebugVerbose) {
        int NVML_VERSION;
        char NVML_DRIVER_VERSION[NAME_BUFFER_SIZE];
        nvmlSystemGetCudaDriverVersion(&NVML_VERSION);
        nvmlSystemGetDriverVersion(NVML_DRIVER_VERSION, NAME_BUFFER_SIZE);
        args.log << "Using libsensors v" << libsensors_version << std::endl;
        args.log << "Using NVML v" << NVML_VERSION << std::endl;
        args.log << "Using NVML Driver v" << NVML_DRIVER_VERSION << std::endl;
    }

    // Hardware Detection / caching for faster updates
    cache_cpus();
    cache_gpus();

    // Prepare output
    print_header();

    // Main Loop
    while (1) {
        // Collection
        if (args.cpu) update_cpus();
        if (args.gpu) update_gpus();
        // Sleeping between polls
        if (args.poll == 0) break;
        else std::this_thread::sleep_for(args.duration);
    }

    if (args.debug >= DebugVerbose) {
        args.log << "The program ends" << std::endl;
    }

    // Shutdown
    shutdown();
    return 0;
}

