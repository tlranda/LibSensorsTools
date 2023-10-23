#include "sensor_tool.h"

// Set to program start time
std::chrono::time_point<std::chrono::system_clock> t_minus_one;

// Changes initial temperature threshold values to most recently updated value (does not create an update itself)
void set_initial_temperatures(void) {
    if (args.cpu)
        for (std::vector<cpu_cache>::iterator i = known_cpus.begin(); i != known_cpus.end(); i++)
            for (int j = 0; j < i->temperature.size(); j++)
                i->initial_temperature[j] = i->temperature[j];
    if (args.gpu)
        for (std::vector<gpu_cache>::iterator i = known_gpus.begin(); i != known_gpus.end(); i++)
            i->gpu_initialTemperature = i->gpu_temperature;
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
    if (args.cpu) {
        // Temperature
        for (std::vector<cpu_cache>::iterator i = known_cpus.begin(); i != known_cpus.end(); i++)
            for (int j = 0; j < i->temperature.size(); j++)
                args.log << ",cpu_" << i->chip_name << "_temperature_" << j;
        // Frequency
        for (std::vector<freq_cache>::iterator i = known_freqs.begin(); i != known_freqs.end(); i++)
            args.log << ",core_" << i->coreid << "_freq";
    }
    if (args.gpu) {
        for (std::vector<gpu_cache>::iterator i = known_gpus.begin(); i != known_gpus.end(); i++) {
            args.log << ",gpu_" << i->device_ID << "_name,gpu_" << i->device_ID << "_gpu_temperature,gpu_" <<
                     i->device_ID << "_mem_temperature,gpu_" << i->device_ID << "_power_usage,gpu_" <<
                     i->device_ID << "_power_limit,gpu_" << i->device_ID << "_utilization_gpu,gpu_" <<
                     i->device_ID << "_utilization_memory,gpu_" << i->device_ID << "_memory_used,gpu_" <<
                     i->device_ID << "_memory_total,gpu_" << i->device_ID << "_pstate";
        }
    }
    args.log << std::endl;
}

// Returns the number of sensors that are at or below their initial value reading
int poll_cycle(std::chrono::time_point<std::chrono::system_clock> t0) {
    int at_or_below_initial_temperature = 0;
    // Timestamp
    std::chrono::time_point<std::chrono::system_clock> t1 = std::chrono::system_clock::now();
    args.log << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9;

    // Collection
    if (args.cpu) {
        int update = update_cpus();
        if (args.debug >= DebugVerbose) args.error_log << "CPUs have " << update << " / " << cpus_to_satisfy << " satisfied temperatures" << std::endl;
        at_or_below_initial_temperature += update;
        //at_or_below_initial_temperature += update_cpus();
    }
    if (args.gpu) {
        int update = update_gpus();
        if (args.debug >= DebugVerbose) args.error_log << "GPUs have " << update << " / " << gpus_to_satisfy << " satisfied temperatures" << std::endl;
        at_or_below_initial_temperature += update;
        //at_or_below_initial_temperature += update_gpus();
    }
    args.log << std::endl;
    // JSON format is different
    if (args.debug >= DebugMinimal) {
        std::chrono::time_point<std::chrono::system_clock> t2 = std::chrono::system_clock::now();
        args.error_log << "Updates completed in " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t1).count() / 1e9 << "s" << std::endl;
    }
    // Sleeping between polls
    if (args.poll != 0) std::this_thread::sleep_for(args.poll_duration);
    return at_or_below_initial_temperature;
}


// Cleanup calls should be based on globally available information; process-killing interrupts will go through this function
void shutdown(int s = 0) {
    std::chrono::time_point<std::chrono::system_clock> t0 = std::chrono::system_clock::now();
    // JSON format is different
    args.error_log << "@@Shutdown at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 << "s" << std::endl;
    if (args.debug >= DebugMinimal) args.error_log << "Run shutdown with signal " << s << std::endl;
    #ifdef GPU_ENABLED
    nvmlShutdown();
    #endif
    sensors_cleanup();
    if (args.debug >= DebugMinimal) args.error_log << "Shutdown clean. Exiting..." << std::endl;
    exit(EXIT_SUCCESS);
}


int main(int argc, char** argv) {
    t_minus_one = std::chrono::system_clock::now();
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
    if (args.debug >= DebugVerbose) args.error_log << "The program lives" << std::endl;
    // JSON should be different
    if (args.debug >= DebugMinimal) {
        args.error_log << "Arguments evaluate to" << std::endl <<
        "Help: " << args.help << std::endl <<
        "CPU: " << args.cpu << std::endl <<
        "GPU: " << args.gpu << std::endl <<
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

    // JSON should be different
    // Denote library versions
    if (args.debug >= DebugVerbose || args.version) {
        args.error_log << "SensorTools v" << SensorToolsVersion << std::endl;
        args.error_log << "Using libsensors v" << libsensors_version << std::endl;
        #ifdef GPU_ENABLED
        int NVML_VERSION;
        char NVML_DRIVER_VERSION[NAME_BUFFER_SIZE];
        nvmlSystemGetCudaDriverVersion(&NVML_VERSION);
        nvmlSystemGetDriverVersion(NVML_DRIVER_VERSION, NAME_BUFFER_SIZE);
        args.error_log << "Using NVML v" << NVML_VERSION << std::endl <<
                          "Using NVML Driver v" << NVML_DRIVER_VERSION << std::endl;
        #endif
        if (args.version) exit(EXIT_SUCCESS);
    }

    // Hardware Detection / caching for faster updates
    cache_cpus();
    cache_gpus();

    // Prepare output
    print_header();

    // Start timing
    std::chrono::time_point<std::chrono::system_clock> t0 = std::chrono::system_clock::now();
    // JSON should be different
    args.error_log << "@@Initialized at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 << "s" << std::endl;

    // Main Loop
    int poll_result, n_to_satisfy = cpus_to_satisfy + gpus_to_satisfy;
    if (args.wrapped == nullptr) {
        if (args.poll == 0) poll_cycle(t_minus_one); // Single event collection
        else while (1) poll_cycle(t_minus_one); // No wrapping, monitor until the process is signaled to terminate
    }
    else { // There's a call to fork
        // Initial Wait
        if (args.debug >= DebugVerbose)
            args.error_log << "Begin initial wait. Should last " << args.initial_wait << " seconds" << std::endl;
        std::chrono::time_point<std::chrono::system_clock> t1 = std::chrono::system_clock::now();
        double waiting = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9;
        while (waiting < args.initial_wait) {
            poll_result = poll_cycle(t_minus_one);
            t1 = std::chrono::system_clock::now();
            waiting = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9;
        }
        // After initial wait expires, change initial temperatures
        set_initial_temperatures();
        // JSON should be different
        args.error_log << "@@Initial wait concludes at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t_minus_one).count() / 1e9 << "s" << std::endl
                       << "@@Launching wrapped command: ";
        int argidx = 0;
        while(args.wrapped[argidx] != nullptr) args.error_log << args.wrapped[argidx++] << " ";
        args.error_log << std::endl;
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
            // JSON should be different
            args.error_log << "@@Wrapped command concludes at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t_minus_one).count() / 1e9 << "s" << std::endl;

            // Waiting is over
            if (result == -1) {
                args.error_log << "Wait on child process failed" << std::endl;
                exit(EXIT_FAILURE);
            }
            if (WIFEXITED(status)) args.error_log << "Child process exits with status " << WEXITSTATUS(status) << std::endl;
            else if (WIFSIGNALED(status)) args.error_log << "Child process caught/terminated by signal " << WTERMSIG(status) << std::endl;

            // Post Wait
            t2 = std::chrono::system_clock::now();
            // JSON should be different
            args.error_log << "@@Post wait begins at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t_minus_one).count() / 1e9 << "s" << std::endl;
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
            // JSON should be different
            t2 = std::chrono::system_clock::now();
            args.error_log << "@@Post wait ends at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t_minus_one).count() / 1e9 << "s" << std::endl;
        }
    }

    if (args.debug >= DebugVerbose)
        args.error_log << "The program ends" << std::endl;

    // Shutdown
    shutdown();
    return 0;
}

