#include <stdio.h>
#include <iostream>
#include <iomanip>
// #include <sensors-c++/sensors.h>
#include <cstring>
#include <sensors/sensors.h>

// Must compile with -lsensors

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

int main(void) {
	std::cout << "The program lives" << std::endl;
	auto const error = sensors_init(NULL);
	if(error != 0) {
		std::cerr << "LibSensors library did not initialize properly! Aborting..." << std::endl;
		exit(1);
	}

    // TODO: Command line argument parsing

    // TODO: Guard this output behind argument to check versions
	// Fetch library version
	std::cout << "Using libsensors v" << libsensors_version << std::endl;

    collect_cpu();


	std::cout << "The program ends" << std::endl;
	return 0;
}

