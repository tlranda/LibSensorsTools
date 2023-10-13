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
#include <string> // String manipulation
#include <cstring> // strncopy, memset

#ifdef GPU_ENABLED
#include <cuda.h> // Must compile with -lcuda
#include <cuda_runtime.h> // Must compile with -lcudart
#include <nvml.h> // Must compile with -lnvidia-ml
// safecuda includes <cuda.h>, <cuda_runtime.h>, <nvrtc.h>, <cublas_v2.h>, <stdlib.h>, <stdio.h>
#include "safecuda.h" // Macros to do safe cuda calls
#else
// Add standard libraries that would have been included by safecuda and don't require GPU support
#include <cstdlib>
#include <cstdio>
#endif

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
    Output(bool direction = true) : defaultToCout(direction), fileStream(), fname{0} {
        std::lock_guard<std::mutex> lock(fileMutex);
        if (defaultToCout) rdbuf(std::cout.rdbuf());
        else rdbuf(std::cerr.rdbuf());
    }
    ~Output() {
        std::lock_guard<std::mutex> lock(fileMutex);
        closeFile();
    }

    // Safely close any existing file (except std::cout/std::cerr) and redirect this reference to a given filename
    // Falls back to std descriptor if the file cannot be opened
    void redirect(const char* fname) {
        std::lock_guard<std::mutex> lock(fileMutex);
        if (openFile(fname)) rdbuf(fileStream.rdbuf());
        else {
            if (defaultToCout) rdbuf(std::cout.rdbuf());
            else rdbuf(std::cerr.rdbuf());
        }
    }
    void redirect(std::filesystem::path fpath) {
        redirect(fpath.string().c_str());
    }
    // Safely close any existing file (except std::cout/std::cerr) and return output to std descriptor
    void revert() {
        std::lock_guard<std::mutex> lock(fileMutex);
        closeFile();
        if (defaultToCout) rdbuf(std::cout.rdbuf());
        else rdbuf(std::cerr.rdbuf());
    }

    // Permit outputting this object to streams for debug
    friend std::ostream& operator<<(std::ostream& os, const Output& obj) {
        if (obj.fname[0]) os << "Output[" << obj.fname << "]";
        else if (obj.defaultToCout) os << "Output[std::cout]";
        else os << "Output[std::cerr]";
        return os;
    }

private:
    std::mutex fileMutex;
    std::ofstream fileStream;
    bool defaultToCout;
    char fname[NAME_BUFFER_SIZE];

    bool openFile(const char *fname_) {
        closeFile();
        fileStream.open(fname_, std::ios::out | std::ios::app);
        if (fileStream.is_open()) {
            // Track file name internally
            std::memset(fname, 0, NAME_BUFFER_SIZE);
            std::strncpy(fname, fname_, NAME_BUFFER_SIZE);
            return true;
        }
        else {
            std::cerr << "Failed to open file " << fname_ << std::endl;
            return false;
        }
    }

    void closeFile() {
        if (fileStream.is_open() && (
            (defaultToCout && &fileStream != &std::cout) ||
            (!defaultToCout && &fileStream != &std::cerr))
           )
            fileStream.close();
        std::memset(fname, 0, NAME_BUFFER_SIZE);
    }
};

// Argument values stored here
typedef struct argstruct {
    bool help = 0,
         cpu = 0,
         gpu = 0;
    short format = 0, debug = 0;
    std::filesystem::path log_path, error_log_path;
    Output log, error_log = Output(false);
    double poll = 0.;
    std::chrono::duration<double> duration;
} arguments;
// Arguments have global scope to permit reference during shutdown (debug values, file I/O, etc)
arguments args;

// Flag that permits update logging upon collection (prevents first row from being logged prior to CSV headers)
bool update = false;


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
        {"errorlog", required_argument, 0, 'L'},
        {"poll", required_argument, 0, 'p'},
        {"debug", required_argument, 0, 'd'},
        {0,0,0,0}
    };
    const char* optionstr = "hcgfl:L:p:d:";
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
                std::cout << "\t-L [file] | --errorlog [file]\n\t\t" <<
                             "File to write extra debug/errors to" << std::endl;
                std::cout << "\t-p [interval] | --poll [interval]\n\t\t" <<
                             "Floating point interval in seconds to poll stats (interval > 0)" << std::endl;
                std::cout << "\t-d [level] | --debug [level]\n\t\t" <<
                             "Debug verbosity (default: 0, maximum: 1)" << std::endl;
                exit(EXIT_SUCCESS);
            case 'c':
                args.cpu = true;
                break;
            case 'g':
                args.gpu = true;
                break;
            case 'f':
                args.format = 1; // Not-CSV
                break;
            case 'l':
                args.log_path = std::filesystem::path(optarg);
                args.log.redirect(args.log_path);
                break;
            case 'L':
                args.error_log_path = std::filesystem::path(optarg);
                args.error_log.redirect(args.error_log_path);
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
        args.cpu = true;
        args.gpu = true;
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
    std::vector<double> temperature;
} cpu_cache;
// Combination of cached file pointer and last-read frequency value
typedef struct cpu_freq_cache_t {
    FILE * fhandle;
    int coreid, hz;
} freq_cache;
std::vector<cpu_cache> known_cpus;
std::vector<freq_cache> known_freqs;


void cache_cpus(void) {
    // No caching if we aren't going to query the CPUs
    if (!args.cpu) return;

    // Cache temperature values through lm-sensors
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

        // No more chips to read -- exit loop
        if (!temp_name) break;

        // Clear and copy into candidate
        memset(candidate.chip_name, 0, NAME_BUFFER_SIZE);
        sensors_snprintf_chip_name(candidate.chip_name, NAME_BUFFER_SIZE, temp_name);
        candidate.name = temp_name;
        if (args.debug >= DebugVerbose) {
            args.error_log << "Begin caching chip " << candidate.chip_name << std::endl;
        }

        // Feature determination
        const sensors_feature* temp_feature = sensors_get_features(temp_name, &nr_feature);
        while(temp_feature) {
            if (args.debug >= DebugVerbose) {
                args.error_log << "\tInspect feature " << nr_feature << " with type " << temp_feature->type << " (hit on type == " << SENSORS_FEATURE_TEMP << ")" << std::endl;
            }
            // We only care about this type of feature
            if (temp_feature->type == SENSORS_FEATURE_TEMP) {
                candidate.features.push_back(temp_feature);
                // Skip directly to input subfeature value
                const sensors_subfeature* temp_subfeature = sensors_get_subfeature(temp_name, temp_feature, nr_subfeature);
                if (args.debug >= DebugVerbose) {
                    args.error_log << "\t\tFeature hit. Acquiring temperature subfeature " << nr_subfeature << std::endl;
                }
                candidate.subfeatures.push_back(temp_subfeature);
                sensors_get_value(temp_name, temp_subfeature->number, &value);
                if (args.debug >= DebugVerbose) {
                    args.error_log << "\t\t\tTemperature value read: " << value << std::endl;
                }
                candidate.temperature.push_back(value);
            }
            temp_feature = sensors_get_features(temp_name, &nr_feature);
        }
        if (args.debug >= DebugVerbose) {
            args.error_log << "Finished inspecting chip " << candidate.chip_name;
        }
        if (!candidate.temperature.empty()) {
            known_cpus.push_back(candidate);
            if (args.debug >= DebugVerbose) {
                args.error_log << " , added to known CPUs" << std::endl;
            }
        }
        else if (args.debug >= DebugVerbose) {
            args.error_log << " , but discarded due to empty temperature reads" << std::endl;
        }
    }

    // Cache CPU frequencies via file pointers
    const std::string prefix = "/sys/devices/system/cpu/cpu",
                      suffix = "/cpufreq/scaling_cur_freq";
    int n_cpu = 0;
    char buf[NAME_BUFFER_SIZE];
    // Collect until we cannot find a CPU core id to match
    while (1) {
        std::filesystem::path fpath(prefix + std::to_string(n_cpu) + suffix);
        if (std::filesystem::exists(fpath)) {
            freq_cache candidate;
            candidate.coreid = n_cpu;
            candidate.fhandle = fopen(fpath.c_str(), "r");
            if (candidate.fhandle) {
                // Initial read
                int nbytes = fread(buf, sizeof(char), NAME_BUFFER_SIZE, candidate.fhandle);
                if (nbytes > 0) {
                    candidate.hz = std::stoi(buf);
                    rewind(candidate.fhandle);
                    known_freqs.push_back(candidate);
                    if (args.debug >= DebugVerbose)
                        args.error_log << "Found CPU freq for core " << n_cpu << std::endl;
                }
                else if (args.debug >= DebugMinimal) {
                    args.error_log << "Unable to read CPU freq for core " << n_cpu << ", so it is not cached" << std::endl;
                }
            }
            else if (args.debug >= DebugMinimal) {
                args.error_log << "Unable to open CPU freq for core " << n_cpu << ", but its file should exist!" << std::endl;
            }
        }
        else {
            if (args.debug >= DebugVerbose)
                args.error_log << "Could not locate file '" << fpath << "', terminating core frequency search" << std::endl;
            break;
        }
        n_cpu++;
    }
}


void update_cpus(void) {
    if (args.debug >= DebugVerbose) args.error_log << "Update CPUs" << std::endl;
    // Temperature updates
    for (std::vector<cpu_cache>::iterator i = known_cpus.begin(); i != known_cpus.end(); i++) {
        for (int j = 0; j < i->temperature.size(); j++) {
            double prev = i->temperature[j];
            if (args.debug >= DebugVerbose) {
                switch (args.format) {
                    case 0:
                        break;
                    case 1:
                        args.log << "Chip " << i->chip_name << " temp BEFORE " << prev << std::endl;
                        break;
                }
            }
            sensors_get_value(i->name, i->subfeatures[j]->number, &i->temperature[j]);
            if (args.debug >= DebugVerbose || update) {
                switch (args.format) {
                    case 0:
                        args.log << "," << i->temperature[j];
                        break;
                    case 1:
                        args.log << " temp NOW " << i->temperature[j] << std::endl;
                        break;
                }
            }
        }
    }
    // Frequency updates
    char buf[NAME_BUFFER_SIZE];
    for (std::vector<freq_cache>::iterator i = known_freqs.begin(); i != known_freqs.end(); i++) {
        rewind(i->fhandle);
        int nbytes = fread(buf, sizeof(char), NAME_BUFFER_SIZE, i->fhandle);
        if (nbytes <= 0 && args.debug >= DebugMinimal) {
            args.error_log << "Unable to update frequency for CPU " << i->coreid << std::endl;
        }
        else {
            i->hz = std::stoi(buf);
        }
        if (args.debug >= DebugVerbose || update) {
            switch (args.format) {
                case 0:
                    args.log << "," << i->hz;
                    break;
                case 1:
                    args.log << "Core " << i->coreid << " Frequency: " << i->hz << std::endl;
                    break;
            }
        }
    }
}


typedef struct gpu_cache_t {
    // IDs
    int device_ID;
    char deviceName[NAME_BUFFER_SIZE] = {0};
    #ifdef GPU_ENABLED
    nvmlDevice_t device_Handle;
    // Cached data
    uint temperature, powerUsage, powerLimit; // { degrees Celsius, Watts, Watts }
    nvmlUtilization_t utilization; // { ui .gpu (%), .memory (%); }
    nvmlMemory_t memory; // { ull .free (bytes), .total (bytes), .used (bytes) }
    nvmlPstates_t pState; // { ui[0-12], 12 is lowest idle, 0 is highest intensity }
    #endif
} gpu_cache;

std::vector<gpu_cache> known_gpus;

#ifdef GPU_ENABLED
void cache_gpus(void) {
    // No caching if we aren't going to query the GPUs
    if (!args.gpu) return;

    int n_devices;
    CHECK_CUDA_ERROR(cudaGetDeviceCount(&n_devices));

    for (int i = 0; i <= n_devices; i++) {
        if (args.debug >= DebugVerbose) {
            args.error_log << "Begin caching GPU " << i << std::endl;
        }
        // Prepare candidate
        gpu_cache candidate;
        candidate.device_ID = i;
        nvmlDeviceGetHandleByIndex_v2(static_cast<unsigned int>(i), &candidate.device_Handle);

        // Initial value caching
        nvmlDeviceGetName(candidate.device_Handle, candidate.deviceName, NAME_BUFFER_SIZE);
        nvmlDeviceGetTemperature(candidate.device_Handle, NVML_TEMPERATURE_GPU, &candidate.temperature);
        nvmlDeviceGetPowerUsage(candidate.device_Handle, &candidate.powerUsage);
        nvmlDeviceGetEnforcedPowerLimit(candidate.device_Handle, &candidate.powerLimit);
        nvmlDeviceGetUtilizationRates(candidate.device_Handle, &candidate.utilization);
        nvmlDeviceGetMemoryInfo(candidate.device_Handle, &candidate.memory);
        nvmlDeviceGetPerformanceState(candidate.device_Handle, &candidate.pState);

        if (args.debug >= DebugVerbose) {
            args.error_log << "Finished caching GPU " << i << std::endl;
        }
        known_gpus.push_back(candidate);
    }
}

void update_gpus(void) {
    if (args.debug >= DebugVerbose) args.error_log << "Update GPUs" << std::endl;
    for (std::vector<gpu_cache>::iterator i = known_gpus.begin(); i != known_gpus.end(); i++) {
        if (args.debug >= DebugVerbose) {
            switch (args.format) {
                case 0:
                    break;
                case 1:
                    args.log << "GPU " << i->device_ID << " " << i->deviceName << " BEFORE" << std::endl;
                    args.log << "\tTemperature: " << i->temperature << std::endl;
                    args.log << "\tPower Usage/Limit: " << i->powerUsage << " / " << i->powerLimit << std::endl;
                    args.log << "\tUtilization: " << i->utilization.gpu << "\% GPU " << i->utilization.memory << "\% Memory " << std::endl;
                    args.log << "\tPerformance State: " << i->pState << std::endl;
                    break;
            }
        }
        nvmlDeviceGetTemperature(i->device_Handle, NVML_TEMPERATURE_GPU, &i->temperature);
        nvmlDeviceGetPowerUsage(i->device_Handle, &i->powerUsage);
        nvmlDeviceGetEnforcedPowerLimit(i->device_Handle, &i->powerLimit);
        nvmlDeviceGetUtilizationRates(i->device_Handle, &i->utilization);
        nvmlDeviceGetMemoryInfo(i->device_Handle, &i->memory);
        nvmlDeviceGetPerformanceState(i->device_Handle, &i->pState);
        if (args.debug >= DebugVerbose || update) {
            switch (args.format) {
                case 0:
                    args.log << "," << i->deviceName << "," << i->temperature << "," << i->powerUsage << "," <<
                             i->powerLimit << "," << i->utilization.gpu << "," << i->utilization.memory <<
                             "," << i->memory.used << "," << i->memory.total << "," << i->pState;
                    break;
                case 1:
                    args.log << "GPU " << i->device_ID << " " << i->deviceName << " AFTER" << std::endl;
                    args.log << "\tTemperature: " << i->temperature << std::endl;
                    args.log << "\tPower Usage/Limit: " << i->powerUsage << " / " << i->powerLimit << std::endl;
                    args.log << "\tUtilization: " << i->utilization.gpu << "\% GPU " << i->utilization.memory << "\% Memory " << std::endl;
                    args.log << "\tPerformance State: " << i->pState << std::endl;
                    break;
            }
        }
    }
}
#else
void cache_gpus(void) {
    if (args.debug >= DebugMinimal)
        args.error_log << "Not compiled with GPU_ENABLED definition -- no GPU support" << std::endl;
}
void update_gpus(void) {
    if (args.debug >= DebugVerbose)
        args.error_log << "No GPU updates -- Not compiled with GPU_ENABLED definition" << std::endl;
}
#endif

// Prints the CSV header columns and immediate cached values from first read
void print_header(void) {
    // Future calls to update_*() will log their results
    update = true;
    if (args.format > 0) {
        if (args.debug >= DebugVerbose)
            args.error_log << "Non-CSV format, no headers to set" << std::endl;
        return;
    }
    // Skip header if appending to a file that already exists
    if ((&args.log != &std::cout) && args.log.tellp() > 0) {
        if (args.debug >= DebugVerbose)
            args.error_log << "No headers -- appending to existing file from " << args.log.tellp() << std::endl;
        return;
    } else if (args.debug >= DebugVerbose)
        args.error_log << "Printing headers" << std::endl;

    // Set headers
    args.log << "timestamp";
    if (args.cpu) {
        // Temperature
        for (std::vector<cpu_cache>::iterator i = known_cpus.begin(); i != known_cpus.end(); i++) {
            for (int j = 0; j < i->temperature.size(); j++) {
                args.log << ",cpu_" << i->chip_name << "_temperature_" << j;
            }
        }
        // Frequency
        for (std::vector<freq_cache>::iterator i = known_freqs.begin(); i != known_freqs.end(); i++) {
            args.log << ",core_" << i->coreid << "_freq";
        }
    }
    if (args.gpu) {
        for (std::vector<gpu_cache>::iterator i = known_gpus.begin(); i != known_gpus.end(); i++) {
            args.log << ",gpu_" << i->device_ID << "_name,gpu_" << i->device_ID << "_temperature,gpu_" <<
                     i->device_ID << "_power_usage,gpu_" << i->device_ID << "_power_limit,gpu_" <<
                     i->device_ID << "_utilization_gpu,gpu_" << i->device_ID <<
                     "_utilization_memory,gpu_" << i->device_ID << "_memory_used,gpu_" <<
                     i->device_ID << "_memory_total,gpu_" << i->device_ID << "_pstate";
        }
    }
    args.log << std::endl;
}


// Cleanup calls should be based on globally available information; process-killing interrupts will go through this function
void shutdown(int s = 0) {
    if (args.debug >= DebugMinimal) args.error_log << "Run shutdown with signal " << s << std::endl;
    #ifdef GPU_ENABLED
    nvmlShutdown();
    #endif
    sensors_cleanup();
    if (args.debug >= DebugMinimal) args.error_log << "Shutdown clean. Exiting..." << std::endl;
    exit(EXIT_SUCCESS);
}


int main(int argc, char** argv) {
    std::chrono::time_point<std::chrono::system_clock> t_minus_one = std::chrono::system_clock::now();
    // Library initializations
	auto const error = sensors_init(NULL);
	if(error != 0) {
		args.error_log << "LibSensors library did not initialize properly! Aborting..." << std::endl;
		exit(EXIT_FAILURE);
	}
    #ifdef GPU_ENABLED
    nvmlInit();
    #endif

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
        args.error_log << "The program lives" << std::endl;
    }
    if (args.debug >= DebugMinimal) {
        args.error_log << "Arguments evaluate to" << std::endl <<
        "Help: " << args.help << std::endl <<
        "CPU: " << args.cpu << std::endl <<
        "GPU: " << args.gpu << std::endl <<
        "Log: " << args.log << std::endl <<
        "Error log: " << args.error_log << std::endl <<
        "Poll: " << args.poll << std::endl <<
        "Debug: " << args.debug << std::endl;
    }

    // Denote library versions
    if (args.debug >= DebugVerbose) {
        args.error_log << "Using libsensors v" << libsensors_version << std::endl;
        #ifdef GPU_ENABLED
        int NVML_VERSION;
        char NVML_DRIVER_VERSION[NAME_BUFFER_SIZE];
        nvmlSystemGetCudaDriverVersion(&NVML_VERSION);
        nvmlSystemGetDriverVersion(NVML_DRIVER_VERSION, NAME_BUFFER_SIZE);
        args.error_log << "Using NVML v" << NVML_VERSION << std::endl <<
                          "Using NVML Driver v" << NVML_DRIVER_VERSION << std::endl;
        #endif
    }

    // Hardware Detection / caching for faster updates
    cache_cpus();
    cache_gpus();

    // Prepare output
    print_header();

    // TODO: Start timing
    std::chrono::time_point<std::chrono::system_clock> t0 = std::chrono::system_clock::now();
    if (args.debug >= DebugMinimal)
        args.error_log << "Initialization took " << std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 << "s" << std::endl;

    // Main Loop
    while (1) {
        // Timestamp
        std::chrono::time_point<std::chrono::system_clock> t1 = std::chrono::system_clock::now();
        args.log << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9;

        // Collection
        if (args.cpu) update_cpus();
        if (args.gpu) update_gpus();
        args.log << std::endl;
        if (args.debug >= DebugMinimal) {
            std::chrono::time_point<std::chrono::system_clock> t2 = std::chrono::system_clock::now();
            args.error_log << "Updates completed in " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t1).count() / 1e9 << "s" << std::endl;
        }
        // Sleeping between polls
        if (args.poll == 0) break;
        else std::this_thread::sleep_for(args.duration);
    }

    if (args.debug >= DebugVerbose) {
        args.error_log << "The program ends" << std::endl;
    }

    // Shutdown
    shutdown();
    return 0;
}

