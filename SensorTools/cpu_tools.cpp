#include "cpu_tools.h"

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
        if (args.debug >= DebugVerbose)
            args.error_log << "Begin caching chip " << candidate.chip_name << std::endl;

        // Feature determination
        const sensors_feature* temp_feature = sensors_get_features(temp_name, &nr_feature);
        while(temp_feature) {
            if (args.debug >= DebugVerbose)
                args.error_log << "\tInspect feature " << nr_feature << " with type " << temp_feature->type << " (hit on type == " << SENSORS_FEATURE_TEMP << ")" << std::endl;
            // We only care about this type of feature
            if (temp_feature->type == SENSORS_FEATURE_TEMP) {
                candidate.features.push_back(temp_feature);
                // Skip directly to input subfeature value
                const sensors_subfeature* temp_subfeature = sensors_get_subfeature(temp_name, temp_feature, nr_subfeature);
                if (args.debug >= DebugVerbose)
                    args.error_log << "\t\tFeature hit. Acquiring temperature subfeature " << nr_subfeature << std::endl;
                candidate.subfeatures.push_back(temp_subfeature);
                sensors_get_value(temp_name, temp_subfeature->number, &value);
                if (args.debug >= DebugVerbose)
                    args.error_log << "\t\t\tTemperature value read: " << value << std::endl;
                candidate.temperature.push_back(value);
                candidate.initial_temperature.push_back(value);
                cpus_to_satisfy++;
            }
            temp_feature = sensors_get_features(temp_name, &nr_feature);
        }
        if (args.debug >= DebugVerbose)
            args.error_log << "Finished inspecting chip " << candidate.chip_name;
        if (!candidate.temperature.empty()) {
            known_cpus.push_back(candidate);
            if (args.debug >= DebugVerbose)
                args.error_log << " , added to known CPUs" << std::endl;
        }
        else if (args.debug >= DebugVerbose)
            args.error_log << " , but discarded due to empty temperature reads" << std::endl;
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
                else if (args.debug >= DebugMinimal)
                    args.error_log << "Unable to read CPU freq for core " << n_cpu << ", so it is not cached" << std::endl;
            }
            else if (args.debug >= DebugMinimal)
                args.error_log << "Unable to open CPU freq for core " << n_cpu << ", but its file should exist!" << std::endl;
        }
        else {
            if (args.debug >= DebugVerbose)
                args.error_log << "Could not locate file '" << fpath << "', terminating core frequency search" << std::endl;
            break;
        }
        n_cpu++;
    }
    if (args.debug >= DebugMinimal)
        args.error_log << "Tracking " << cpus_to_satisfy << " CPU temperature sensors" << std::endl;
}


int update_cpus(void) {
    if (args.debug >= DebugVerbose) args.error_log << "Update CPUs" << std::endl;
    // Temperature updates
    int at_below_initial_temperature = 0;
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
            if (i->temperature[j] <= i->initial_temperature[j]) at_below_initial_temperature++;
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
        if (nbytes <= 0 && args.debug >= DebugMinimal)
            args.error_log << "Unable to update frequency for CPU " << i->coreid << std::endl;
        else i->hz = std::stoi(buf);
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
    return at_below_initial_temperature;
}

// Definition of external variables for CPU tools
std::vector<cpu_cache> known_cpus;
int cpus_to_satisfy = 0;
std::vector<freq_cache> known_freqs;

