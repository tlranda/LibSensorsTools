#include <iostream> // cout, cerr, etc
#include <iomanip> // setw and setprecision
#include <thread> // Often implies <chrono>
#include <chrono> // Given explicitly in case of above comment being untrue

#include <unistd.h> // Getopt
#include <getopt.h> // Getopt-long
#include <signal.h> // signal interrupts
#include <sensors/sensors.h> // Must compile with -lsensors
#include <vector> // Cache chips/gpus for repeated lookup

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

// Argument values stored here
typedef struct argstruct {
    short help = 0,
          cpu = 0,
          gpu = 0,
          debug = 0,
          format = 0;
    char* log = 0;
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
                args.log = optarg;
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


void print_header(void) {
}

typedef struct cpu_cache_t {
    char chip_name[NAME_BUFFER_SIZE] = {0};
    int nr = 0;
    const sensors_chip_name* name = nullptr;
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
            std::cerr << "Begin caching chip " << candidate.chip_name << std::endl;
        }

        // Feature determination
        const sensors_feature* temp_feature = sensors_get_features(temp_name, &nr_feature);
        while(temp_feature) {
            if (args.debug >= DebugVerbose) {
                std::cerr << "\tInspect feature " << nr_feature << " with type " << temp_feature->type << " (hit on type == " << SENSORS_FEATURE_TEMP << ")" << std::endl;
            }
            // We only care about this type of feature
            if (temp_feature->type == SENSORS_FEATURE_TEMP) {
                candidate.features.push_back(temp_feature);
                // Skip directly to input subfeature value
                const sensors_subfeature* temp_subfeature = sensors_get_subfeature(temp_name, temp_feature, nr_subfeature);
                if (args.debug >= DebugVerbose) {
                    std::cerr << "\t\tFeature hit. Acquiring temperature subfeature " << nr_subfeature << std::endl;
                }
                candidate.subfeatures.push_back(temp_subfeature);
                sensors_get_value(temp_name, temp_subfeature->number, &value);
                if (args.debug >= DebugVerbose) {
                    std::cerr << "\t\t\tTemperature value read: " << value << std::endl;
                }
                candidate.last_read.push_back(value);
            }
            temp_feature = sensors_get_features(temp_name, &nr_feature);
        }
        if (args.debug >= DebugVerbose) {
            std::cerr << "Finished inspecting chip " << candidate.chip_name;
        }
        if (!candidate.last_read.empty()) {
            known_cpus.push_back(candidate);
            if (args.debug >= DebugVerbose) {
                std::cerr << " , added to known CPUs" << std::endl;
            }
        }
        else if (args.debug >= DebugVerbose) {
            std::cerr << " , but discarded due to empty temperature reads" << std::endl;
        }
    }
}

void update_cpus(void) {
    for (std::vector<cpu_cache>::iterator i = known_cpus.begin(); i != known_cpus.end(); i++)
        for (int j = 0; j < i->last_read.size(); j++) {
            double prev = i->last_read[j];
            sensors_get_value(i->name, i->subfeatures[j]->number, &i->last_read[j]);
            if (args.debug >= DebugVerbose) {
                std::cout << "Chip " << i->chip_name << " temp BEFORE " << prev << " NOW " << i->last_read[j] << std::endl;
            }
        }
}


void collect_cpu(void) {
	// Try to fetch chips?
	int nr = 0;
	char chip_name[NAME_BUFFER_SIZE];
	auto name = sensors_get_detected_chips(nullptr, &nr);
	while (name) {
		// Clear chip name buffer
        memset(chip_name, 0, NAME_BUFFER_SIZE);
        sensors_snprintf_chip_name(chip_name, NAME_BUFFER_SIZE, name);
		/* if (strcmp(chip_name, "k10temp-pci-00cb") != 0) { // DEBUG ONLY: Limit output to single chip
			name = sensors_get_detected_chips(nullptr, &nr);
			continue;
		} */

		// TODO: Guard to output as lm-sensors format rather than CSV
        // TODO: Separate execution path to output as CSV only
        std::cout << chip_name << std::endl;
		const char *adap = sensors_get_adapter_name(&name->bus);
		std::cout << "Adapter: " << adap << std::endl;
		int nr2 = 0;
		auto feat = sensors_get_features(name, &nr2);
		while (feat) {
			if (feat->type != SENSORS_FEATURE_TEMP) { // SENSORS_FEATURE_TEMP features have the temperature data
				feat = sensors_get_features(name, &nr2);
				continue;
			}
			std::cout << sensors_get_label(name, feat) << ":\t";
			// std::cout << "\tFound feature " << sensors_get_label(name, feat) << std::endl;
			// EXPLICITLY FETCH TEMPS LIKE SENSORS ITSELF
			auto nr3 = SENSORS_SUBFEATURE_TEMP_INPUT;
			auto sub = sensors_get_subfeature(name, feat, SENSORS_SUBFEATURE_TEMP_INPUT);
			double value;
			int errno2;
			errno2 = sensors_get_value(name, sub->number, &value);
			//std::cout << "+" << value << "°C (" << nr3 << ")" << std::endl;
			const auto default_precision{std::cout.precision()};
			std::cout << "+" << std::setw(1) << std::setprecision(3) << value << "°C (" << nr3 << ")" << std::endl;
			std::cout << std::setprecision(default_precision);
			/*
			int nr3 = 0;
			auto sub = sensors_get_all_subfeatures(name, feat, &nr3);
			while (sub) {
				double value;
				int errno2;
				errno2 = sensors_get_value(name, nr3, &value);
				std::cout << "+" << value << "° (" << nr3 << ")" << std::endl;
				/ *
				if (errno2 < 0) {
					std::cerr << "\t\tCould not read value of " << sub->name << " (" << sub->type << ")" << std::endl;
					std::cerr << "\t\t\tThe error code is " << errno2 << std::endl;
					std::cerr << "\t\t\tBut if I could it'd be " << value << std::endl;
				}
				else {
					std::cout << "\t\tValue of " << sub->name << " (" << sub->type << "): " << value << std::endl;
				}
				* /
				sub = sensors_get_all_subfeatures(name, feat, &nr3);
			}
			*/
			feat = sensors_get_features(name, &nr2);
		}
		name = sensors_get_detected_chips(nullptr, &nr);
	}
}


typedef struct stats_ {
    std::time_t timestamp;
    uint temperature;
    uint powerUsage;
    uint powerLimit;
    nvmlUtilization_t utilization; // ui gpu | ui memory (both %)
    nvmlMemory_t memory; // ull free | ull total | ull used (all bytes)
    nvmlPstates_t performanceState;
} stats;



void collect_gpu(void) {
    int n_devices;
    CHECK_CUDA_ERROR(cudaGetDeviceCount(&n_devices));
    std::cout << "Found " << n_devices+1 << " GPUs" << std::endl;
    for (int i = 0; i <= n_devices; i++) {
        uint temperature, powerUsage, powerLimit;
        nvmlUtilization_t utilization; // ui gpu | ui memory (both as %)
        nvmlMemory_t memory; // ull free | ull total | ull used (all as bytes)
        nvmlPstates_t performanceState;
        nvmlDevice_t device;
        nvmlDeviceGetHandleByIndex_v2(static_cast<unsigned int>(i), &device);
        nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);
        std::cout << "Device " << i << " Temperature: " << temperature << std::endl;

        // NVML Unit?
        // BEN: Units only supported by S-class cards; we're not using those so any nvmlUnit* calls don't work
        nvmlUnit_t unit;
        nvmlUnitGetHandleByIndex(static_cast<unsigned int>(i), &unit);
        nvmlUnitGetTemperature(unit, 0, &temperature);
        std::cout << "\tDevice " << i << " Intake Temperature: " << temperature << std::endl;
        nvmlUnitGetTemperature(unit, 1, &temperature);
        std::cout << "\tDevice " << i << " Exhaust Temperature: " << temperature << std::endl;
        nvmlUnitGetTemperature(unit, 2, &temperature);
        std::cout << "\tDevice " << i << " Board Temperature: " << temperature << std::endl;
        nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_COUNT, &temperature);
        std::cout << "\t\tDevice " << i << " NVML Temp count " << NVML_TEMPERATURE_COUNT << std::endl;

        nvmlDeviceGetPowerUsage(device, &powerUsage);
        std::cout << "Device " << i << " Power Usage: " << powerUsage << std::endl;
        nvmlDeviceGetEnforcedPowerLimit(device, &powerLimit);
        std::cout << "Device " << i << " Power Limit: " << powerLimit << std::endl;
        nvmlDeviceGetUtilizationRates(device, &utilization);
        std::cout << "Device " << i << " Utilization: [" << utilization.gpu << "\% GPU] [" << utilization.memory << "\% MEMORY]" << std::endl;
        nvmlDeviceGetMemoryInfo(device, &memory);
        std::cout << "Device " << i << " Memory: [" << memory.used << " / " << memory.total << "] " << static_cast<double>(memory.used) / static_cast<double>(memory.total) << "%" << std::endl;
        nvmlDeviceGetPerformanceState(device, &performanceState);
        std::cout << "Device " << i << " Performance State: " << performanceState << std::endl;
    }
    /*
    auto device_;
    NVML_RT_CALL(nvmlDeviceGetTemperature(device_, NVML_TEMPERATURE_GPU, &device_stats.temperature));
    NVML_RT_CALL(nvmlDeviceGetPowerUsage(device, &device_stats.powerUsage));
    NVML_RT_CALL(nvmlDeviceGetEnforcedPowerLimit(device, &device_stats.powerLimit));
    NVML_RT_CALL(nvmlDeviceGetutilizationRates(device, &device_stats.utilization));
    NVML_RT_CALL(nvmlDeviceGetmemoryInfo(device, &device_stats.memory));
    NVML_RT_CALL(nvmlDeviceGetPerformanceState(device, &device_stats.performance));
    */
}


// Cleanup calls should be based on globally available information; process-killing interrupts will go through this function
void shutdown(int s = 0) {
    if (args.debug >= DebugMinimal) std::cerr << "Run shutdown with signal " << s << std::endl;
    nvmlShutdown();
    sensors_cleanup();
    if (args.debug >= DebugMinimal) std::cerr << "Shutdown clean. Exiting..." << std::endl;
    exit(EXIT_SUCCESS);
}


int main(int argc, char** argv) {
    // Library initializations
	auto const error = sensors_init(NULL);
	if(error != 0) {
		std::cerr << "LibSensors library did not initialize properly! Aborting..." << std::endl;
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
        std::cout << "The program lives" << std::endl;
    }
    if (args.debug >= DebugMinimal) {
        std::cout << "Arguments evaluate to" << std::endl <<
        "Help: " << args.help << std::endl <<
        "CPU: " << args.cpu << std::endl <<
        "GPU: " << args.gpu << std::endl;
        if (args.log == 0) std::cout << "Log: --" << std::endl;
        else std::cout << "Log: " << args.log << std::endl;
        std::cout << "Poll: " << args.poll << std::endl <<
        "Debug: " << args.debug << std::endl;
    }

    // Denote library versions
    if (args.debug >= DebugVerbose) {
        int NVML_VERSION;
        char NVML_DRIVER_VERSION[NAME_BUFFER_SIZE];
        nvmlSystemGetCudaDriverVersion(&NVML_VERSION);
        nvmlSystemGetDriverVersion(NVML_DRIVER_VERSION, NAME_BUFFER_SIZE);
        std::cout << "Using libsensors v" << libsensors_version << std::endl;
        std::cout << "Using NVML v" << NVML_VERSION << std::endl;
        std::cout << "Using NVML Driver v" << NVML_DRIVER_VERSION << std::endl;
    }

    // Hardware Detection / caching for faster updates
    cache_cpus();

    // Main Loop
    while (1) {
        // Collection
        if (args.cpu) update_cpus(); // collect_cpu();
        if (args.gpu) collect_gpu();
        // Sleeping between polls
        if (args.poll == 0) break;
        else std::this_thread::sleep_for(args.duration);
    }

    if (args.debug >= DebugVerbose) {
        std::cout << "The program ends" << std::endl;
    }

    // Shutdown
    shutdown();
    return 0;
}

