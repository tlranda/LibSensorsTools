#cmakedefine BUILD_CPU
#cmakedefine BUILD_GPU
#cmakedefine BUILD_POD
#cmakedefine BUILD_NVME
#cmakedefine SERVER_MAIN
#ifdef SERVER_MAIN
#include "common_driver_server.h"
#else
#include "common_driver_libsensors.h"
#endif

// Variable declarations
std::chrono::time_point<std::chrono::system_clock> t_minus_one;
#ifdef SERVER_MAIN
int master_socket = -1;
std::vector<int> client_sockets;
#endif

int main(int argc, char** argv) {
    init_libsensorstools(argc, argv);
    main_loop();
    return 0;
}

void init_libsensorstools(int argc, char** argv) {
    t_minus_one = std::chrono::system_clock::now();

    // Library initializations
    #ifdef BUILD_CPU
    int error = sensors_init(NULL);
    if (error != 0) {
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

    // Prepare for graceful shutdown via CTRL+C and other common signals
    struct sigaction sigHandler;
    sigHandler.sa_handler = shutdown;
    sigemptyset(&sigHandler.sa_mask);
    sigHandler.sa_flags = 0;
    sigaction(SIGINT, &sigHandler, NULL);
    sigaction(SIGABRT, &sigHandler, NULL);
    sigaction(SIGTERM, &sigHandler, NULL);
    // SIGKILL and SIGSTOP cannot be caught, blocked or ignored -- nor should they

    // Command line argument parsing
    parse(argc, argv);
    if (args.debug >= DebugVerbose) args.error_log << "The program lives" << std::endl;
    if (args.format == OutputJSON) {
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
                    #ifdef SERVER_MAIN
                    "\t\"clients\": " << args.clients << "," << std::endl <<
                    #else
                    "\t\"ip-address\": \"" << ((args.ip_addr == nullptr) ? "N/A" : args.ip_addr) << "\"," << std::endl <<
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
        if (args.wrapped != nullptr) {
            int argidx = 0;
            args.log << "\"";
            while(args.wrapped[argidx] != nullptr) args.log << args.wrapped[argidx++] << " ";
            args.log << "\"" << std::endl;
        }
        else args.log << "null" << std::endl;
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
        #ifdef SERVER_MAIN
        "Clients: " << args.clients << std::endl <<
        #else
        "IP Address: " << ((args.ip_addr == nullptr) ? "N/A" : args.ip_addr) << std::endl <<
        #endif
        "Format: ";
        switch(args.format) {
            case OutputCSV:
                args.error_log << "csv";
                break;
            case OutputHuman:
                args.error_log << "human-readable";
                break;
            // case OutputJSON would not be reached
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
    if (args.format == OutputJSON) {
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
        args.error_log << "LibNVMe: " << nvme_get_version(NVME_VERSION_PROJECT) << std::endl;
        #endif
        args.error_log << "Nlohmann_Json: " <<
                          NLOHMANN_JSON_VERSION_MAJOR << "." <<
                          NLOHMANN_JSON_VERSION_MINOR << "." <<
                          NLOHMANN_JSON_VERSION_PATCH << std::endl;
    }
    // When version argument is supplied, OK to exit immediately after supplying version information
    if (args.version) exit(EXIT_SUCCESS);

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

    #ifndef SERVER_MAIN
    // Prepare output
    update = true;
    if (args.format == OutputCSV) print_csv_header();
    else if (args.debug >= DebugVerbose) args.error_log << "Non-CSV format, no headers to set" << std::endl;
    #else
    // Initialize server sockets and get clients connected
    int activity, addrlen, new_socket, opt = 1, sd, max_sd, valread;
    struct sockaddr_in address;
    char buffer [NAME_BUFFER_SIZE] = {0};
    fd_set readfds;

    // Create master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        args.error_log << "Socket creation failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    // Allow multiple same-host connections
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        args.error_log << "setsockopt failed to permit SO_REUSEADDR" << std::endl;
        exit(EXIT_FAILURE);
    }
    // Type of socket we want created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SensorToolsPort); // See control.h.in --> control_server.h
    // Bind socket to localhost port
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) {
        args.error_log << "Socket binding failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    // Prepare to listen
    if (listen(master_socket, args.clients) < 0) {
        args.error_log << "Listen failed for requested " << args.clients << " clients" << std::endl;
        exit(EXIT_FAILURE);
    }
    // Wait for all clients to connect
    addrlen = sizeof(address);
    while (client_sockets.size() < args.clients) {
        // Clear the socket set, always include master socket
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;
        // Add any clients already connected
        for (int i = 0; i < client_sockets.size(); i++) {
            sd = client_sockets[i];
            if (sd > max_sd) max_sd = sd;
        }
        // Wait for activity on one of the sockets -- nonblocking

        // We redefine the timeout each iteration as its value CAN become
        // undefined after some socket API calls
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);
        if ((activity < 0) && (errno != EINTR)) args.error_log << "Select() error" << std::endl;
        // Activity on master socket == new connection
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                args.error_log << "Failed to accept client connection" << std::endl;
                // Are there any side effects to worry about here?
            }
            else {
                if (args.debug >= DebugVerbose)
                    args.error_log << "New connection at socket " << new_socket <<
                                   " from IP addr " << inet_ntoa(address.sin_addr) <<
                                   " on port " << ntohs(address.sin_port) << std::endl;
                client_sockets.push_back(new_socket);
            }
        }
        else {
            // Activity on non-master sockets while awaiting client connections
            for (std::vector<int>::iterator it = client_sockets.begin();
                 it != client_sockets.end(); ) {
                sd = *it;
                if (FD_ISSET(sd, &readfds)) {
                    // The client disconnected
                    if ((valread = read(sd, buffer, NAME_BUFFER_SIZE)) == 0) {
                        getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                        args.error_log << "Client disconnected from IP address " << inet_ntoa(address.sin_addr) << " on port " << ntohs(address.sin_port) << std::endl;
                        close(sd);
                        client_sockets.erase(it);
                    }
                    else ++it; // Client sent some data (SHOULD NOT HAVE), but ignore it
                }
                else ++it; // This socket did not receive activity
            }
        }
    }
    #endif
}

void print_csv_header() {
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

void shutdown(int signal = 0) {
    std::chrono::time_point<std::chrono::system_clock> t0 = std::chrono::system_clock::now();
    if (args.format == OutputJSON) {
        args.log << "{ \"event\": \"shutdown\", \"timestamp\": " <<
                 std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 <<
                 " }" << std::endl << "]" << std::endl;
    }
    else
        args.error_log << "@@Shutdown at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 << "s" << std::endl;
    if (args.debug >= DebugMinimal) args.error_log << "Run shutdown with signal " << signal << std::endl;
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
    #ifdef SERVER_MAIN
    // Terminate and free client sockets
    for (int i = 0; i < client_sockets.size(); i++) close(client_sockets[i]);
    if (master_socket != -1) close(master_socket);
    #endif
    if (args.debug >= DebugMinimal) args.error_log << "Shutdown clean. Exiting..." << std::endl;
    // Use the signal as the exit code, default of 0 == EXIT_SUCCESS
    exit(signal);
}

void init_timing() {
    std::chrono::time_point<std::chrono::system_clock> t0 = std::chrono::system_clock::now();
    if (args.format == OutputJSON) args.log << "{\"event\": \"initialization\", \"duration\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 << "}," << std::endl;
    else args.error_log << "@@Initialized at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 << "s" << std::endl;
}

int poll_cycle(std::chrono::time_point<std::chrono::system_clock> t0) {
    int satisfied = 0;
    // Initial timestamp
    std::chrono::time_point<std::chrono::system_clock> t1 = std::chrono::system_clock::now();
    switch (args.format) {
        case OutputHuman:
            args.log << "Poll update at ";
        case OutputCSV:
            args.log << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9;
            break;
        case OutputJSON:
            args.log << "{\"event\": \"poll-data\", \"timestamp\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9 << "," << std::endl;
            break;
    }

    // Collection
    #ifdef BUILD_CPU
    if (args.cpu) {
        int update = update_cpus();
        if (args.debug >= DebugVerbose) args.error_log << "CPUs have " << update << " / " << cpus_to_satisfy << " satisfied temperatures" << std::endl;
        satisfied += update;
        //satisfied += update_cpus();
    }
    #endif
    #ifdef BUILD_GPU
    if (args.gpu) {
        int update = update_gpus();
        if (args.debug >= DebugVerbose) args.error_log << "GPUs have " << update << " / " << gpus_to_satisfy << " satisfied temperatures" << std::endl;
        satisfied += update;
        //satisfied += update_gpus();
    }
    #endif
    #ifdef BUILD_POD
    if (args.submer) {
        int update = update_submers();
        if (args.debug >= DebugVerbose) args.error_log << "Pods have " << update << " / " << submers_to_satisfy << " satisfied temperatures" << std::endl;
        satisfied += update;
        //satisfied += update_submers();
    }
    #endif
    #ifdef BUILD_NVME
    if (args.nvme) {
        int update = update_nvme();
        if (args.debug >= DebugVerbose) args.error_log << "NVMe has " << update << " / " << nvme_to_satisfy << " satisfied temperatures" << std::endl;
        satisfied += update;
        //satisfied += update_nvme();
    }
    #endif
    #ifdef SERVER_MAIN
    // TODO: Collection only in post-wait phases to increment satisfied
    #endif

    // Final timestamp
    switch (args.format) {
        case OutputCSV:
        case OutputHuman:
            args.log << std::endl;
            if (args.debug >= DebugMinimal) {
                std::chrono::time_point<std::chrono::system_clock> t2 = std::chrono::system_clock::now();
                args.error_log << "Updates completed in " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t1).count() / 1e9 << "s" << std::endl;
            }
            break;
        case OutputJSON:
            std::chrono::time_point<std::chrono::system_clock> t2 = std::chrono::system_clock::now();
            args.log << "\"poll-update-duration\": " <<
                     std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t1).count() / 1e9 <<
                     std::endl;
            args.log << "}," << std::endl;
            break;
    }
    // Sleeping between polls
    if (args.poll != 0) std::this_thread::sleep_for(args.poll_duration);
    return satisfied;
}

void main_loop() {
    init_timing();

    #ifdef SERVER_MAIN
    // All clients connected, notify them to start polling
    if (args.debug >= DebugVerbose) args.error_log << "Server sends 'START' message to all clients" << std::endl;
    char clientMsgBuffer[NAME_BUFFER_SIZE] = "START";
    for (std::vector<int>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++) send(*it, clientMsgBuffer, NAME_BUFFER_SIZE, 0);
    #endif

    // Prepare count of sensors to re-satisfy for early-exit
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

    // Decide what kind of execution is necessary
    #ifndef SERVER_MAIN
    if (args.ip_addr != nullptr) client_connect_loop();
    else
    #endif
    if (args.wrapped == nullptr) simple_poll_cycle_loop();
    else fork_join_loop(n_to_satisfy);

    #ifdef SERVER_MAIN
    // Shut down all clients
    if (args.debug >= DebugVerbose) args.error_log << "Server sends 'STOP' message to all clients" << std::endl;
    strncpy(clientMsgBuffer, "STOP", 4);
    clientMsgBuffer[4] = '\0';
    for (std::vector<int>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++) send(*it, clientMsgBuffer, NAME_BUFFER_SIZE, 0);
    // Give clients a chance to disconnect
    // TODO: There should be a safer way to perform this wait
    sleep(1);
    #endif

    if (args.debug >= DebugVerbose) args.error_log << "The program ends" << std::endl;

    // Shutdown without signal, normally
    shutdown();
}

void client_connect_loop() {
    args.error_log << "Client process attempts to connect to server at " << args.ip_addr << std::endl;
    // We will connect to a server for coordination
    int clientSocket, attempt = 0;
    struct sockaddr_in serverAddr;
    while (1) {
        // Create client socket
        if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            args.error_log << "Socket creation failed";
            exit(EXIT_FAILURE);
        }
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(args.ip_addr);
        serverAddr.sin_port = htons(SensorToolsPort);
        // Connect to server
        if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            args.error_log << "Server Connection attempt #" << attempt+1 << " failed" << std::endl;
            // Final attempt failed
            if (MAX_CLIENT_CONNECT_ATTEMPTS > 0 && attempt+1 >= MAX_CLIENT_CONNECT_ATTEMPTS) {
                args.error_log << "Maximum server connection attempts exhausted. Exiting." << std::endl;
                exit(EXIT_FAILURE);
            }
            sleep(1);
            //std::this_thread::sleep_for(1); // Wait before retrying
        }
        else {
            args.error_log << "Server Connection SUCCESS on attempt #" << attempt+1 << std::endl;
            break;
        }
        attempt++;
    }
    int satisfied;
    char serverMsgBuffer[NAME_BUFFER_SIZE] = {0};
    // Receive server start message
    args.error_log << "Wait for server ready message" << std::endl;
    recv(clientSocket, serverMsgBuffer, NAME_BUFFER_SIZE, 0);
    args.error_log << "Received server ready message. Begin polling" << std::endl;
    while (1) {
        satisfied = poll_cycle(t_minus_one); // Monitor until server signals for termination
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(clientSocket, &readfds);
        int activity = pselect(clientSocket+1, &readfds, NULL, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) args.error_log << "Selection error" << std::endl;
        if (FD_ISSET(clientSocket, &readfds)) {
            recv(clientSocket, serverMsgBuffer, NAME_BUFFER_SIZE, 0);
            // TODO: Another message from the server may indicate to begin communicating number of satisfied values on this client
            if (strcmp(serverMsgBuffer, "STOP") == 0) break;
            else
                args.error_log << "Client received unexpected message from server: " << serverMsgBuffer << std::endl;
        }
    }
    args.error_log << "Closing connection to server" << std::endl;
    close(clientSocket);
}

void simple_poll_cycle_loop() {
    if (args.poll == 0) poll_cycle(t_minus_one); // Single event collection
    else while (1) poll_cycle(t_minus_one); // No wrapping, monitor until the process is signaled to terminate
}

void set_initial_temperatures() {
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

void fork_join_loop(int satisfy) {
    // Initial Wait
    std::chrono::time_point<std::chrono::system_clock> t0 = std::chrono::system_clock::now(), t1 = t0;
    if (args.format == OutputJSON)
        args.log << "{\"event\": \"initial-wait-start\", \"timestamp\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t_minus_one).count() / 1e9 << "}," << std::endl;
    else if (args.debug >= DebugVerbose)
        args.error_log << "Begin initial wait. Should last " << args.initial_wait << " seconds" << std::endl;
    double waiting = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9;
    int poll_result;
    while (waiting < args.initial_wait) {
        poll_result = poll_cycle(t_minus_one);
        t1 = std::chrono::system_clock::now();
        waiting = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() / 1e9;
    }
    // After initial wait expires, change initial temperatures
    set_initial_temperatures();
    if (args.format == OutputJSON) {
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
    else fork_join_parent_poll(pid, t0, satisfy);
}

void fork_join_parent_poll(pid_t pid, std::chrono::time_point<std::chrono::system_clock> t0, int satisfy) {
    // Parent process will continue to poll devices and wait for child
    int status, poll_result;
    pid_t result;
    double waiting;

    // Non-blocking wait using waitpid with WNOHANG
    do {
        // Briefly check in on child process, then go back to collecting results
        result = waitpid(pid, &status, WNOHANG);
        poll_result = poll_cycle(t0);
    } while (result == 0);
    std::chrono::time_point<std::chrono::system_clock> t1 = std::chrono::system_clock::now(), t2 = t1;
    if (args.format == OutputJSON) {
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
    if (args.format == OutputJSON) args.log << "{\"event\": \"post-wait-start\", \"timestamp\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t_minus_one).count() / 1e9 << "}," << std::endl;
    else args.error_log << "@@Post wait begins at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t_minus_one).count() / 1e9 << "s" << std::endl;
    if (args.debug >= DebugVerbose) args.error_log << "Post wait can last up to " << args.post_wait << " seconds" << std::endl;
    t1 = std::chrono::system_clock::now();
    t2 = t1;
    waiting = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t2).count() / 1e9;
    if (args.post_wait < 0) {
        // Maximal wait enforced here
        while (waiting < -args.post_wait) {
            if (args.debug >= DebugVerbose)
                args.error_log << poll_result << " / " << satisfy << " temperatures reached initial thresholds" << std::endl;
            // Check for initial temperature match
            if (poll_result == satisfy) break;
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
    if (args.format == OutputJSON) {
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

