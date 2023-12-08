#include "sensor_tool.h"

// Set to program start time
std::chrono::time_point<std::chrono::system_clock> t_minus_one;

// Changes initial temperature threshold values to most recently updated value (does not create an update itself)
void set_initial_temperatures(void) {
    #ifdef BUILD_CPU
    if (args.cpu)
        for (std::vector<cpu_cache>::iterator i = known_cpus.begin(); i != known_cpus.end(); i++)
            for (int j = 0; j < i->temperature.size(); j++)
                i->initial_temperature[j] = i->temperature[j];
    #endif
    #ifdef BUILD_GPU
    if (args.gpu)
        for (std::vector<gpu_cache>::iterator i = known_gpus.begin(); i != known_gpus.end(); i++)
            i->gpu_initialTemperature = i->gpu_temperature;
    #endif
    #ifdef BUILD_POD
    if (args.submer)
        for (std::vector<std::unique_ptr<submer_cache>>::iterator i = known_submers.begin(); i != known_submers.end(); i++) {
            submer_cache* j = i->get();
            j->initialSubmerTemperature = j->json_data["temperature"];
        }
    #endif
    #ifdef BUILD_NVME
    if (args.nvme)
        for (std::vector<nvme_cache>::iterator i = known_nvme.begin(); i != known_nvme.end(); i++)
            for (int j = 0; j < i->temperature.size(); j++)
                i->initial_temperature[j] = i->temperature[j];
    #endif
}

// Prints the CSV header columns and immediate cached values from first read
void print_header(void) {
    // Future calls to update_*() will log their results
    update = true;
    if (args.format != 0) {
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
    #ifdef BUILD_CPU
    if (args.cpu) {
        // Temperature
        for (std::vector<cpu_cache>::iterator i = known_cpus.begin(); i != known_cpus.end(); i++)
            for (int j = 0; j < i->temperature.size(); j++)
                args.log << ",cpu_" << i->chip_name << "_temperature_" << j;
        // Frequency
        for (std::vector<freq_cache>::iterator i = known_freqs.begin(); i != known_freqs.end(); i++)
            args.log << ",core_" << i->coreid << "_freq";
    }
    #endif
    #ifdef BUILD_GPU
    if (args.gpu) {
        for (std::vector<gpu_cache>::iterator i = known_gpus.begin(); i != known_gpus.end(); i++) {
            args.log << ",gpu_" << i->device_ID << "_name,gpu_" << i->device_ID << "_gpu_temperature,gpu_" <<
                     i->device_ID << "_mem_temperature,gpu_" << i->device_ID << "_power_usage,gpu_" <<
                     i->device_ID << "_power_limit,gpu_" << i->device_ID << "_utilization_gpu,gpu_" <<
                     i->device_ID << "_utilization_memory,gpu_" << i->device_ID << "_memory_used,gpu_" <<
                     i->device_ID << "_memory_total,gpu_" << i->device_ID << "_pstate";
        }
    }
    #endif
    #ifdef BUILD_POD
    if (args.submer) {
        for (std::vector<std::unique_ptr<submer_cache>>::iterator i = known_submers.begin(); i != known_submers.end(); i++) {
            submer_cache* j = i->get();
            args.log << ",submer_" << j->index, "_temperature";
        }
    }
    #endif
    #ifdef BUILD_NVME
    if (args.nvme) {
        for (std::vector<nvme_cache>::iterator i = known_nvme.begin(); i != known_nvme.end(); i++)
            for (int j = 0; j < i->temperature.size(); j++)
                args.log << ",nvme_" << i->index << "_" << j << "_temperature";
    }
    #endif
    args.log << std::endl;
}

// Returns the number of sensors that are at or below their initial value reading
int poll_cycle(std::chrono::time_point<std::chrono::system_clock> t0) {
    int at_or_below_initial_temperature = 0;
    // Timestamp
    std::chrono::time_point<std::chrono::system_clock> t1 = std::chrono::system_clock::now();
    switch (args.format) {
        case 1:
            args.log << "Poll update at ";
        case 0:
            args.log << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9;
            break;
        case 2:
            args.log << "{\"event\": \"poll-data\", \"timestamp\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9 << "," << std::endl;
            break;
    }

    // Collection
    #ifdef BUILD_CPU
    if (args.cpu) {
        int update = update_cpus();
        if (args.debug >= DebugVerbose) args.error_log << "CPUs have " << update << " / " << cpus_to_satisfy << " satisfied temperatures" << std::endl;
        at_or_below_initial_temperature += update;
        //at_or_below_initial_temperature += update_cpus();
    }
    #endif
    #ifdef BUILD_GPU
    if (args.gpu) {
        int update = update_gpus();
        if (args.debug >= DebugVerbose) args.error_log << "GPUs have " << update << " / " << gpus_to_satisfy << " satisfied temperatures" << std::endl;
        at_or_below_initial_temperature += update;
        //at_or_below_initial_temperature += update_gpus();
    }
    #endif
    #ifdef BUILD_POD
    if (args.submer) {
        int update = update_submers();
        if (args.debug >= DebugVerbose) args.error_log << "Pods have " << update << " / " << submers_to_satisfy << " satisfied temperatures" << std::endl;
        at_or_below_initial_temperature += update;
        //at_or_below_initial_temperature += update_submers();
    }
    #endif
    #ifdef BUILD_NVME
    if (args.nvme) {
        int update = update_nvme();
        if (args.debug >= DebugVerbose) args.error_log << "NVMe has " << update << " / " << nvme_to_satisfy << " satisfied temperatures" << std::endl;
        at_or_below_initial_temperature += update;
        //at_or_below_initial_temperature += update_nvme();
    }
    #endif
    switch (args.format) {
        case 0:
        case 1:
            args.log << std::endl;
            if (args.debug >= DebugMinimal) {
                std::chrono::time_point<std::chrono::system_clock> t2 = std::chrono::system_clock::now();
                args.error_log << "Updates completed in " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t1).count() / 1e9 << "s" << std::endl;
            }
            break;
        case 2:
            // Have to add a dummy end for JSON record to be compliant
            args.log << "\"dummy-end\": true" << std::endl;
            args.log << "}," << std::endl;
            std::chrono::time_point<std::chrono::system_clock> t2 = std::chrono::system_clock::now();
            args.log << "{\"event\": \"poll-update\", \"duration\": " <<
                     std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t1).count() / 1e9 <<
                     "}," << std::endl;
            break;
    }
    // Sleeping between polls
    if (args.poll != 0) std::this_thread::sleep_for(args.poll_duration);
    return at_or_below_initial_temperature;
}


// Cleanup calls should be based on globally available information; process-killing interrupts will go through this function
void shutdown(int s = 0) {
    std::chrono::time_point<std::chrono::system_clock> t0 = std::chrono::system_clock::now();
    if (args.format == 2) {
        args.log << "{ \"event\": \"shutdown\", \"timestamp\": " <<
                 std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 <<
                 " }" << std::endl <<
                 "]" << std::endl;
    }
    else
        args.error_log << "@@Shutdown at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 << "s" << std::endl;
    if (args.debug >= DebugMinimal) args.error_log << "Run shutdown with signal " << s << std::endl;
    #ifdef BUILD_CPU
    sensors_cleanup();
    #endif
    #ifdef BUILD_GPU
    #ifdef GPU_ENABLED
    nvmlShutdown();
    #endif
    #endif
    #ifdef BUILD_POD
    curl_global_cleanup();
    #endif
    if (args.debug >= DebugMinimal) args.error_log << "Shutdown clean. Exiting..." << std::endl;
    exit(EXIT_SUCCESS);
}


int main(int argc, char** argv) {
    t_minus_one = std::chrono::system_clock::now();
    // Library initializations
    #ifdef BUILD_CPU
    auto const error = sensors_init(NULL);
    if(error != 0) {
        args.error_log << "LibSensors library did not initialize properly! Aborting..." << std::endl;
        exit(EXIT_FAILURE);
    }
    #endif
    #ifdef BUILD_GPU
    #ifdef GPU_ENABLED
    nvmlInit();
    #endif
    #endif
    #ifdef BUILD_POD
    curl_global_init(CURL_GLOBAL_ALL);
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
    if (args.debug >= DebugVerbose) args.error_log << "The program lives" << std::endl;
    if (args.format == 2) {
        args.log << "[" << std::endl;
        args.log << "{\"arguments\": { " << std::endl <<
                    "\t\"help\": " << args.help << "," << std::endl <<
                    #ifdef BUILD_CPU
                    "\t\"cpu\": " << args.cpu << "," << std::endl <<
                    #endif
                    #ifdef BUILD_GPU
                    "\t\"gpu\": " << args.gpu << "," << std::endl <<
                    #endif
                    #ifdef BUILD_POD
                    "\t\"submer\": " << args.submer << "," << std::endl <<
                    #endif
                    #ifdef BUILD_NVME
                    "\t\"nvme\": " << args.nvme << "," << std::endl <<
                    #endif
                    "\t\"format\": \"json\"," << std::endl <<
                    "\t\"log\": \"" << args.log << "\"," << std::endl <<
                    "\t\"error-log\": \"" << args.error_log << "\"," << std::endl <<
                    "\t\"poll\": " << args.poll << "," << std::endl <<
                    "\t\"initial-wait\": " << args.initial_wait << "," << std::endl <<
                    "\t\"post-wait\": " << args.post_wait << "," << std::endl <<
                    "\t\"debug\": " << args.debug << "," << std::endl <<
                    "\t\"version\": \"" << args.version << "\"," << std::endl <<
                    "\t\"wrapped-call\": ";
        if (args.wrapped == nullptr) args.log << "null" << std::endl;
        else {
            int argidx = 0;
            args.log << "\"";
            while(args.wrapped[argidx] != nullptr) args.log << args.wrapped[argidx++] << " ";
            args.log << "\"" << std::endl;
        }
        args.log << "\t}" << std::endl << "}," << std::endl;
    }
    else if (args.debug >= DebugMinimal) {
        args.error_log << "Arguments evaluate to" << std::endl <<
        "Help: " << args.help << std::endl <<
        #ifdef BUILD_CPU
        "CPU: " << args.cpu << std::endl <<
        #endif
        #ifdef BUILD_GPU
        "GPU: " << args.gpu << std::endl <<
        #endif
        #ifdef BUILD_POD
        "Submer: " << args.submer << std::endl <<
        #endif
        #ifdef BUILD_NVME
        "NVMe: " << args.nvme << std::endl <<
        #endif
        "Format: ";
        switch(args.format) {
            case 0:
                args.error_log << "csv";
                break;
            case 1:
                args.error_log << "human-readable";
                break;
        }
        args.error_log << std::endl << "Log: " << args.log << std::endl <<
        "Error log: " << args.error_log << std::endl <<
        "Poll: " << args.poll << std::endl <<
        "Initial Wait: " << args.initial_wait << std::endl <<
        "Post Wait: " << args.post_wait << std::endl <<
        "Debug: " << args.debug << std::endl <<
        "Version: " << args.version << std::endl <<
        "Wrapped call: ";
        if (args.wrapped != nullptr) {
            int argidx = 0;
            while(args.wrapped[argidx] != nullptr) args.error_log << args.wrapped[argidx++] << " ";
        }
        else args.error_log << "N/A";
        args.error_log << std::endl;
    }

    // Denote library versions
    if (args.format == 2) {
        args.log << "{\"versions\": {" << std::endl <<
                    "\t\"SensorTools\": \"" << SensorToolsVersion << "\"," << std::endl;
        #ifdef BUILD_CPU
        args.log << "\t\"LibSensors\": \"" << libsensors_version << "\"," << std::endl;
        #endif
        #ifdef BUILD_GPU
        #ifdef GPU_ENABLED
        int NVML_VERSION;
        char NVML_DRIVER_VERSION[NAME_BUFFER_SIZE];
        nvmlSystemGetCudaDriverVersion(&NVML_VERSION);
        nvmlSystemGetDriverVersion(NVML_DRIVER_VERSION, NAME_BUFFER_SIZE);
        args.log << "\t\"NVML\": \"" << NVML_VERSION << "\"," << std::endl <<
                    "\t\"NVIDIA Driver\": \"" << NVML_DRIVER_VERSION << "\"," << std::endl;
        #endif
        #endif
        #ifdef BUILD_POD
        args.log << "\t\"LibCurl\": \"" << curl_version() << "\"," << std::endl;
        #endif
        #ifdef BUILD_NVME
        args.log << "\t\"LibNVMe\": \"" << nvme_get_version(NVME_VERSION_PROJECT) << "\"," << std::endl;
        #endif
        args.log << "\t\"Nlohmann_Json\": \"" <<
                        NLOHMANN_JSON_VERSION_MAJOR << "." <<
                        NLOHMANN_JSON_VERSION_MINOR << "." <<
                        NLOHMANN_JSON_VERSION_PATCH << "\"\t}" << std::endl << "}," << std::endl;
    }
    else if (args.debug >= DebugVerbose || args.version) {
        args.error_log << "SensorTools v" << SensorToolsVersion << std::endl;
        #ifdef BUILD_CPU
        args.error_log << "Using libsensors v" << libsensors_version << std::endl;
        #endif
        #ifdef BUILD_GPU
        #ifdef GPU_ENABLED
        int NVML_VERSION;
        char NVML_DRIVER_VERSION[NAME_BUFFER_SIZE];
        nvmlSystemGetCudaDriverVersion(&NVML_VERSION);
        nvmlSystemGetDriverVersion(NVML_DRIVER_VERSION, NAME_BUFFER_SIZE);
        args.error_log << "Using NVML v" << NVML_VERSION << std::endl <<
                          "Using NVML Driver v" << NVML_DRIVER_VERSION << std::endl;
        #endif
        #endif
        #ifdef BUILD_POD
        args.error_log << "LibCurl v" << curl_version() << std::endl;
        #endif
        #ifdef BUILD_NVME
        args.log << "LibNVMe: " << nvme_get_version(NVME_VERSION_PROJECT) << std::endl;
        #endif
        args.log << "Nlohmann_Json: " <<
                    NLOHMANN_JSON_VERSION_MAJOR << "." <<
                    NLOHMANN_JSON_VERSION_MINOR << "." <<
                    NLOHMANN_JSON_VERSION_PATCH << std::endl;
        if (args.version) exit(EXIT_SUCCESS);
    }

    // Hardware Detection / caching for faster updates
    #ifdef BUILD_CPU
    cache_cpus();
    #endif
    #ifdef BUILD_GPU
    cache_gpus();
    #endif
    #ifdef BUILD_POD
    cache_submers();
    #endif
    #ifdef BUILD_NVME
    cache_nvme();
    #endif

    // Prepare output
    print_header();

    // Start timing
    std::chrono::time_point<std::chrono::system_clock> t0 = std::chrono::system_clock::now();
    if (args.format == 2) args.log << "{\"event\": \"initialization\", \"duration\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 << "}," << std::endl;
    else args.error_log << "@@Initialized at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 << "s" << std::endl;

    // Main Loop
    int poll_result, n_to_satisfy = 0;
    #ifdef BUILD_CPU
    n_to_satisfy += cpus_to_satisfy;
    #endif
    #ifdef BUILD_GPU
    n_to_satisfy += gpus_to_satisfy;
    #endif
    #ifdef BUILD_POD
    n_to_satisfy += submers_to_satisfy;
    #endif
    #ifdef BUILD_NVME
    n_to_satisfy += nvme_to_satisfy;
    #endif
    if (args.wrapped == nullptr) {
        if (args.poll == 0) poll_cycle(t_minus_one); // Single event collection
        else while (1) poll_cycle(t_minus_one); // No wrapping, monitor until the process is signaled to terminate
    }
    else { // There's a call to fork
        // Initial Wait
        std::chrono::time_point<std::chrono::system_clock> t1 = std::chrono::system_clock::now();
        if (args.format == 2)
            args.log << "{\"event\": \"initial-wait-start\", \"timestamp\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t_minus_one).count() / 1e9 << "}," << std::endl;
        else if (args.debug >= DebugVerbose)
            args.error_log << "Begin initial wait. Should last " << args.initial_wait << " seconds" << std::endl;
        double waiting = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9;
        while (waiting < args.initial_wait) {
            poll_result = poll_cycle(t_minus_one);
            t1 = std::chrono::system_clock::now();
            waiting = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9;
        }
        // After initial wait expires, change initial temperatures
        set_initial_temperatures();
        if (args.format == 2) {
            args.log << "{\"event\": \"initial-wait-end\", \"timestamp\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t_minus_one).count() / 1e9 << ", \"wrapped-command\": \"";
            int argidx = 0;
            // NOTE: Not escaped for JSON format, so this value could break the file
            while (args.wrapped[argidx] != nullptr) args.log << args.wrapped[argidx++] << " ";
            args.log << "\"}," << std::endl;
        }
        else {
            args.error_log << "@@Initial wait concludes at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t_minus_one).count() / 1e9 << "s" << std::endl
                           << "@@Launching wrapped command: ";
            int argidx = 0;
            while(args.wrapped[argidx] != nullptr) args.error_log << args.wrapped[argidx++] << " ";
            args.error_log << std::endl;
        }
        // Fork call
        pid_t pid = fork();
        if (pid == -1) {
            args.error_log << "Fork failed." << std::endl;
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {
            // Child process should execute the indicated command
            if (execvp(args.wrapped[0], args.wrapped) == -1) {
                args.error_log << "Exec child process failed" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        else {
            // Parent process will continue to poll devices and wait for child
            int status;
            pid_t result;

            // Non-blocking wait using waitpid with WNOHANG
            do {
                // Briefly check in on child process, then go back to collecting results
                result = waitpid(pid, &status, WNOHANG);
                poll_result = poll_cycle(t0);
            }
            while (result == 0);
            std::chrono::time_point<std::chrono::system_clock> t2 = std::chrono::system_clock::now();
            if (args.format == 2) {
                args.log << "{\"event\": \"wrapped-command-end\", \"timestamp\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t_minus_one).count() / 1e9 << "}," << std::endl;
            }
            else args.error_log << "@@Wrapped command concludes at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t_minus_one).count() / 1e9 << "s" << std::endl;

            // Waiting is over
            if (result == -1) {
                args.error_log << "Wait on child process failed" << std::endl;
                exit(EXIT_FAILURE);
            }
            if (WIFEXITED(status)) args.error_log << "Child process exits with status " << WEXITSTATUS(status) << std::endl;
            else if (WIFSIGNALED(status)) args.error_log << "Child process caught/terminated by signal " << WTERMSIG(status) << std::endl;

            // Post Wait
            t2 = std::chrono::system_clock::now();
            if (args.format == 2) args.log << "{\"event\": \"post-wait-start\", \"timestamp\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t_minus_one).count() / 1e9 << "}," << std::endl;
            else args.error_log << "@@Post wait begins at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t_minus_one).count() / 1e9 << "s" << std::endl;
            if (args.debug >= DebugVerbose) args.error_log << "Post wait can last up to " << args.post_wait << " seconds" << std::endl;
            t2 = std::chrono::system_clock::now();
            t1 = std::chrono::system_clock::now();
            waiting = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t2).count() / 1e9;
            if (args.post_wait < 0) {
                // Maximal wait enforced here
                while (waiting < -args.post_wait) {
                    if (args.debug >= DebugVerbose)
                        args.error_log << poll_result << " / " << n_to_satisfy << " temperatures reached initial thresholds" << std::endl;
                    // Check for initial temperature match
                    if (poll_result == n_to_satisfy) break;
                    else if (args.debug >= DebugVerbose)
                        args.error_log << "Waited " << waiting << " seconds; will wait for temperature normalization or until " << -args.post_wait << " seconds elapse" << std::endl;
                    poll_result = poll_cycle(t0);
                    t1 = std::chrono::system_clock::now();
                    waiting = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t2).count() / 1e9;
                }
            }
            else {
                // Normal wait for a simple duration to expire
                while (waiting < args.post_wait) {
                    poll_result = poll_cycle(t0);
                    t1 = std::chrono::system_clock::now();
                    waiting = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t2).count() / 1e9;
                }
            }
            t2 = std::chrono::system_clock::now();
            if (args.format == 2) {
                args.log << "{\"event\": \"post-wait-end\", \"timestamp\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t_minus_one).count() / 1e9 << ", \"max-wait\": ";
                if (args.post_wait < 0) {
                    args.log << -args.post_wait << ", \"reason\": ";
                    if (waiting < -args.post_wait) args.log << "\"temperature-early-exit\"";
                    else args.log << "\"timeout\"";
                }
                else args.log << args.post_wait << ", \"reason\": \"timeout\"";
                args.log << "}," << std::endl;
            }
            else {
                args.error_log << "@@Post wait ends at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t_minus_one).count() / 1e9 << "s" << std::endl;
                if (args.post_wait < 0 && waiting < -args.post_wait)
                    args.error_log << "@@Post wait terminates due to temperature early exit" << std::endl;
                else args.error_log << "@@Post wait terminates due to timeout" << std::endl;
            }
        }
    }

    if (args.debug >= DebugVerbose)
        args.error_log << "The program ends" << std::endl;

    // Shutdown
    shutdown();
    return 0;
}

