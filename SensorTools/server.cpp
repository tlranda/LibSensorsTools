#include "server.h"

// Set to program start time
std::chrono::time_point<std::chrono::system_clock> t_minus_one;

int master_socket = -1;
std::vector<int> client_sockets;

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
    // Terminate and free client sockets
    for (int i = 0; i < client_sockets.size(); i++)
        close(client_sockets[i]);
    if (master_socket != -1) close(master_socket);
    if (args.debug >= DebugMinimal) args.error_log << "Shutdown clean. Exiting..." << std::endl;
    exit(EXIT_SUCCESS);
}

int poll_cycle(std::chrono::time_point<std::chrono::system_clock> t0) {
    int satisfied = 0;
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
    // TODO: Collection only in post-wait phase, increment satisfied
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
    return satisfied;
}

int main(int argc, char** argv) {
    t_minus_one = std::chrono::system_clock::now();
    // Library initializations
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
                    "\t\"clients\": " << args.clients << "," << std::endl <<
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
        "Clients: " << args.clients << std::endl <<
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
                    "\t\"SensorTools\": \"" << SensorToolsVersion << "\"," << std::endl <<
                    "\t\"Nlohmann_Json\": \"" <<
                    NLOHMANN_JSON_VERSION_MAJOR << "." <<
                    NLOHMANN_JSON_VERSION_MINOR << "." <<
                    NLOHMANN_JSON_VERSION_PATCH << "\"\t}" << std::endl << "}," << std::endl;
    }
    else if (args.debug >= DebugVerbose || args.version) {
        args.error_log << "SensorTools v" << SensorToolsVersion << std::endl;
        args.log << "Nlohmann_Json: " <<
                    NLOHMANN_JSON_VERSION_MAJOR << "." <<
                    NLOHMANN_JSON_VERSION_MINOR << "." <<
                    NLOHMANN_JSON_VERSION_PATCH << std::endl;
        if (args.version) exit(EXIT_SUCCESS);
    }

    // No caching, no headers for the server process

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

    // Start timing
    std::chrono::time_point<std::chrono::system_clock> t0 = std::chrono::system_clock::now();
    if (args.format == 2) args.log << "{\"event\": \"initialization\", \"duration\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 << "}," << std::endl;
    else args.error_log << "@@Initialized at " << std::chrono::duration_cast<std::chrono::nanoseconds>(t0-t_minus_one).count() / 1e9 << "s" << std::endl;

    // All clients connected, notify them to start polling
    if (args.debug >= DebugVerbose) args.error_log << "Server sends 'START' message to all clients" << std::endl;
    char clientMsgBuffer[NAME_BUFFER_SIZE] = "START";
    for (std::vector<int>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++) send(*it, clientMsgBuffer, NAME_BUFFER_SIZE, 0);

    // Main Loop
    int poll_result, n_to_satisfy = 0;
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
                // TODO: Indicate to clients to check/send # satisfied
                if (args.debug >= DebugVerbose) args.error_log << "Server sends 'POLL' message to all clients" << std::endl;
                strncpy(clientMsgBuffer, "POLL", 4);
                clientMsgBuffer[4] = '\0';
                for (std::vector<int>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++) send(*it, clientMsgBuffer, NAME_BUFFER_SIZE, 0);
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
    // Shut down all clients
    if (args.debug >= DebugVerbose) args.error_log << "Server sends 'STOP' message to all clients" << std::endl;
    strncpy(clientMsgBuffer, "STOP", 4);
    clientMsgBuffer[4] = '\0';
    for (std::vector<int>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++) send(*it, clientMsgBuffer, NAME_BUFFER_SIZE, 0);
    // Give clients a chance to disconnect
    sleep(1);

    if (args.debug >= DebugVerbose)
        args.error_log << "The program ends" << std::endl;

    // Shutdown
    shutdown();
    return 0;
}

