#include "omreport_tools.h"

int omreport_cache::get_CLI_values(void) {
    // Make command execute and capture its output
    char buffer[128];
    std::string result = "", line;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        args.error_log << "OMReport Popen failed" << std::endl;
        return 1; // Popen Error
    }
    try {
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
            result += buffer;
    }
    catch (const std::exception& e) {
        args.error_log << "OMReport buffer read failed" << std::endl;
        fclose(pipe);
        return 2; // Parsing error
    }
    fclose(pipe);
    // Process line-by-line
    std::stringstream ss(result);
    bool has_watts = false, has_amps = false;
    int lineIdx = 0;
    while ((!has_watts || !has_amps) && getline(ss, line)) {
        if (!has_watts && line.find("Reading") != std::string::npos) {
            result = line.substr(line.find(":")+2);
            result = result.substr(0, result.find(" "));
            has_watts = true;
            this->wattReading = atoi(result.c_str());
        }
        if (!has_amps && line.find("PS1 Current 1") != std::string::npos) {
            result = line.substr(line.find(":")+2);
            result = result.substr(0, result.find(" "));
            has_amps = true;
            this->amperageReading = atof(result.c_str());
        }
        lineIdx++;
    }
    // Determine return status
    if (has_watts && has_amps) return 0; // OK
    else if (has_watts) {
        args.error_log << "Missed amp reading" << std::endl;
        return 3; // Missed amps ONLY
    }
    else if (has_amps) {
        args.error_log << "Missed watts reading" << std::endl;
        return 4; // Missed watts
    }
    args.error_log << "Missed watts and amps readings" << std::endl;
    return 5; // Missed both
}

void cache_omreports(void) {
    // No caching if we aren't going to query
    if (!args.omreport) return;

    if (args.debug >= DebugVerbose)
        args.error_log << "Begin caching OMReport" << std::endl;

    // OMReport over CLI isn't really cache-able, but we can do an initial hit to ensure everything is OK and have the object already initialized
    omreport_cache candidate;
    candidate.id = 0;
    int status = candidate.get_CLI_values();
    if (status != 0) {
        args.error_log << "No OMReport cached, disabling future polling" << std::endl;
        args.omreport = false;
        return;
    }
    else if (args.debug >= DebugMinimal)
        args.error_log << "Tracking OMReport" << std::endl;
    known_omreports.push_back(candidate);
    omreports_to_satisfy++;
}

int update_omreports(void) {
    if (args.debug >= DebugVerbose) args.error_log << "Update OMReport" << std::endl;
    int omreport_read = 0;
    for (std::vector<omreport_cache>::iterator i = known_omreports.begin();
         i != known_omreports.end();
         i++) {
        if ((i->get_CLI_values()) != 0) {
            args.error_log << "Failed to retrieve OMReport " << i->id
                           << ", no longer tracking" << std::endl;
            args.omreport = false;
            omreports_to_satisfy--;
            continue;
        }
        else omreport_read++;
        if (args.debug >= DebugVerbose || update) {
            switch (args.format) {
                case OutputCSV:
                    args.log << "," << i->wattReading
                             << "," << i->amperageReading << std::endl;
                    break;
                case OutputHuman:
                    args.log << "OMReport " << i->id << " Watts: " << i->wattReading << std::endl
                             << "OMReport " << i->id << " Amperage: " << i->amperageReading << std::endl;
                    break;
                case OutputJSON:
                    args.log << "\t\"omreport-" << i->id << "-watts\": " << i->wattReading << "," << std::endl
                             << "\t\"omreport-" << i->id << "-amps\": " << i->amperageReading << "," << std::endl;
                    break;
            }
        }
    }
    return omreport_read;
}

// Definition of external variables for OMReport tools
std::vector<omreport_cache> known_omreports;
int omreports_to_satisfy = 0;

