#include <stdio.h>
#include <iostream>
#include <iomanip>
// #include <sensors-c++/sensors.h>
#include <cstring>
#include <sensors/sensors.h>

// Must compile with -lsensors

int main(void) {
	std::cout << "The program lives" << std::endl;

	auto const error = sensors_init(NULL);
	if(error != 0) {
		std::cerr << "LibSensors library did not initialize properly! Aborting..." << std::endl;
		exit(1);
	}
	// Fetch library version
	std::cout << "Using libsensors v" << libsensors_version << std::endl;

	// Try to fetch chips?
	int nr = 0;
	auto name = sensors_get_detected_chips(nullptr, &nr);
	while (name) {
		char chip_name[200];
		sensors_snprintf_chip_name(chip_name, 200, name);
		if (strcmp(chip_name, "k10temp-pci-00cb") != 0) { // DEBUG ONLY: Limit output to single chip
			name = sensors_get_detected_chips(nullptr, &nr);
			continue;
		}
		std::cout << chip_name << std::endl;
		const char *adap = sensors_get_adapter_name(&name->bus);
		std::cout << "Adapter: " << adap << std::endl;
		int nr2 = 0;
		auto feat = sensors_get_features(name, &nr2);
		while (feat) {
			if (feat->type != 2) { // SENSORS_FEATURE_TEMP
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

	std::cout << "The program ends" << std::endl;
	return 0;
}
