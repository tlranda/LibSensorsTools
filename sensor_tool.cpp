#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <thread>
#include <sensors/sensors.h> // Must compile with -lsensors

#include <cuda.h> // Must compile with -lcuda
#include <cuda_runtime.h> // Must compile with -lcudart
#include <nvml.h> // Must compile with -lnvidia-ml

#include "safecuda.h" // Macros to do safe cuda calls

#define CHIP_NAME_BUFFER_SIZE 200

void collect_cpu(void) {
	// Try to fetch chips?
	int nr = 0;
	char chip_name[CHIP_NAME_BUFFER_SIZE];
	auto name = sensors_get_detected_chips(nullptr, &nr);
	while (name) {
		// Clear chip name buffer
        memset(chip_name, 0, CHIP_NAME_BUFFER_SIZE);
        sensors_snprintf_chip_name(chip_name, CHIP_NAME_BUFFER_SIZE, name);
		if (strcmp(chip_name, "k10temp-pci-00cb") != 0) { // DEBUG ONLY: Limit output to single chip
			name = sensors_get_detected_chips(nullptr, &nr);
			continue;
		}
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

int main(void) {
	std::cout << "The program lives" << std::endl;
	auto const error = sensors_init(NULL);
	if(error != 0) {
		std::cerr << "LibSensors library did not initialize properly! Aborting..." << std::endl;
		exit(1);
	}
    nvmlInit();

    // TODO: Command line argument parsing

    // TODO: Guard this output behind argument to check versions
	// Fetch library version
	//std::cout << "Using libsensors v" << libsensors_version << std::endl;
    int NVML_VERSION;
    nvmlSystemGetCudaDriverVersion(&NVML_VERSION);
    char NVML_DRIVER_VERSION[200];
    nvmlSystemGetDriverVersion(NVML_DRIVER_VERSION, 200);
    std::cout << "Using NVML v" << NVML_VERSION << std::endl;
    std::cout << "Using NVML v" << NVML_DRIVER_VERSION << std::endl;
    //collect_cpu();
    collect_gpu();

    // std::time_t timestamp = std::chrono::high_resolution-clock::now().time_since_epoch().count();
    // std::this_thread::sleep_for(std::chrono::milliseconds(1));

	std::cout << "The program ends" << std::endl;

    nvmlShutdown();
	return 0;
}

