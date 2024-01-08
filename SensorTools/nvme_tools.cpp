#include "nvme_tools.h"

void cache_nvme(void) {
    // No caching if we aren't going to query NVME
    if (!args.nvme) return;

    // Cache temperature values through libnvme
	nvme_root_t r;
	nvme_host_t h;
	nvme_subsystem_t s;
	nvme_ctrl_t c;
    r = nvme_scan(NULL);
    if (!r) {
        args.error_log << "Failed to scan NVME, no longer tracking" << std::endl;
        args.nvme = false;
        nvme_to_satisfy = 0;
    }
    int nvme_index = 0;
    nvme_for_each_host(r, h) {
        nvme_for_each_subsystem(h, s) {
            nvme_subsystem_for_each_ctrl(s, c) {
                nvme_cache candidate;
                candidate.index = nvme_index;
                nvme_index++;
                if (args.debug >= DebugVerbose) args.error_log << "Identified an NVMe controller to potentially log" << std::endl;
                int fd = nvme_ctrl_get_fd(c);
                // Duplicate the file descriptor so that WE manage it, not lib-nvme which will close underneath us when scope changes
                candidate.fds.push_back(dup(fd));
                struct nvme_smart_log log;
                int ret = nvme_get_log_smart(fd, NVME_NSID_ALL, true, &log);
                if (ret) {
                    args.error_log << "Failed to retrieve NVME smart log for " << nvme_ctrl_get_name(c) << ", no longer tracking" << std::endl;
                    args.nvme = false;
                    nvme_to_satisfy = 0;
                }
                candidate.smarts.push_back(log);
                // Temperature is reported in degrees Kelvin rather than Celsius
                int temp = ((log.temperature[1] << 8) | log.temperature[0]) - 273;
                candidate.temperature.push_back(temp);
                candidate.initial_temperature.push_back(temp);
                if (args.debug >= DebugVerbose) args.error_log << "Tracking NVMe controller with " << candidate.temperature.size() << " temperatures" << std::endl;
                known_nvme.push_back(candidate);
                nvme_to_satisfy += candidate.temperature.size();
            }
        }
    }
    nvme_free_tree(r);
}

int update_nvme(void) {
    if (args.debug >= DebugVerbose) args.error_log << "Update NVMe" << std::endl;
    int at_below_initial_temperature = 0;
    for (std::vector<nvme_cache>::iterator i = known_nvme.begin(); i != known_nvme.end(); i++) {
        for (int k = 0; k < i->fds.size(); k++) {
            int ret = nvme_get_log_smart(i->fds[k], NVME_NSID_ALL, true, &i->smarts[k]);
            if (ret) {
                args.error_log << "Failed to retrieve NVME smart log for nvme-" << k << ", no longer tracking" << std::endl;
                args.nvme = false;
                nvme_to_satisfy = 0;
            }
            int temp = ((i->smarts[k].temperature[1] << 8) | i->smarts[k].temperature[0]) - 273;
            i->temperature[k] = temp;
            if (temp <= i->initial_temperature[k]) at_below_initial_temperature++;
            if (args.debug >= DebugVerbose || update) {
                switch (args.format) {
                    case OutputCSV:
                        args.log << "," << i->temperature[k];
                        break;
                    case OutputHuman:
                        args.log << "NVMe " << i->index << "_" << k << " Temperature: " << i->temperature[k] << std::endl;
                        break;
                    case OutputJSON:
                        args.log << "\t\"nvme-" << i->index << "-" << k << "-temperature\": " << i->temperature[k] << "," << std::endl;
                        break;
                }
            }
        }
    }
    return at_below_initial_temperature;
}

// Definition of external variables for NVME tools
std::vector<nvme_cache> known_nvme;
int nvme_to_satisfy = 0;

