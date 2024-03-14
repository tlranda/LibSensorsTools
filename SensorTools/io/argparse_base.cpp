#cmakedefine SERVER_MAIN

#ifdef SERVER_MAIN
#include "argparse_server.h"
#else
#include "argparse_libsensors.h"
#endif

/*
   Read command line and parse arguments
   Pointer args used to store semantic settings for program execution
 */
void parse(int argc, char** argv) {
    char* PROGNAME = argv[0];
    int c, bad_args = 0;

    // Getopt option declarations
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        #ifndef SERVER_MAIN
            #ifdef BUILD_CPU
            {"cpu", no_argument, 0, 'c'},
            #endif
            #ifdef BUILD_GPU
            {"gpu", no_argument, 0, 'g'},
            #endif
            #ifdef BUILD_SUBMER
            {"submer", no_argument, 0, 's'},
            #endif
            #ifdef BUILD_NVME
            {"nvme", no_argument, 0, 'n'},
            #endif
            #ifdef BUILD_PDU
            {"pdu", no_argument, 0, 'P'},
            #endif
            {"ipaddr", required_argument, 0, 'I'},
            {"connections", required_argument, 0, 'C'},
        #else
            {"clients", required_argument, 0, 'C'},
        #endif
        {"format", required_argument, 0, 'f'},
        {"log", required_argument, 0, 'l'},
        {"errorlog", required_argument, 0, 'L'},
        {"poll", required_argument, 0, 'p'},
        {"initial-wait", required_argument, 0, 'i'},
        {"post-wait", required_argument, 0, 'w'},
        {"timeout", required_argument, 0, 't'},
        {"debug", required_argument, 0, 'd'},
        {"version", no_argument, 0, 'v'},
        {0,0,0,0}
    };
    const char* optionstr = "h"
    #ifndef SERVER_MAIN
        #ifdef BUILD_CPU
        "c"
        #endif
        #ifdef BUILD_GPU
        "g"
        #endif
        #ifdef BUILD_SUBMER
        "s"
        #endif
        #ifdef BUILD_NVME
        "n"
        #endif
        #ifdef BUILD_PDU
        "P"
        #endif
        "I:"
    #endif
    "C:f:l:L:p:i:w:t:d:v";
    // Disable getopt's automatic error message -- we'll catch it via the '?' return and shut down
    opterr = 0;

    // Parsing loop
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, optionstr, long_options, &option_index);
        if (c == -1) break;
        switch (c) {
            // This case may be deprecated -- blindly following a demo that includes it for some reason
            case 0:
                std::cerr << "Weird option " << long_options[option_index].name;
                if (optarg) std::cerr << " with arg " << optarg;
                std::cerr << std::endl;
                break;
            case 'h':
                std::cout << "SensorTools v" << SensorToolsVersion << std::endl;
                std::cout << "Usage: " << PROGNAME << " [options]" << std::endl;
                std::cout << "\t-h | --help\n\t\t" <<
                             "Print this help message and exit" << std::endl;
                #ifndef SERVER_MAIN
                    #ifdef BUILD_CPU
                    std::cout << "\t-c | --cpu\n\t\t" <<
                                 "Query CPU stats only (default: CPU and GPU)" << std::endl;
                    #endif
                    #ifdef BUILD_GPU
                    std::cout << "\t-g | --gpu\n\t\t" <<
                                 "Query GPU stats only (default: GPU and CPU)" << std::endl;
                    #endif
                    #ifdef BUILD_SUBMER
                    std::cout << "\t-s | --submer\n\t\t" <<
                                 "Query Submer Pod stats (default: Not queried)" << std::endl;
                    #endif
                    #ifdef BUILD_NVME
                    std::cout << "\t-n | --nvme\n\t\t" <<
                                 "Query NVMe device temperatures (default: Not queried)" << std::endl;
                    #endif
                    #ifdef BUILD_PDU
                    std::cout << "\t-P | --pdu\n\t\t" <<
                                 "Query PDU readings over SNMP (default: Not queried)" << std::endl;
                    #endif
                    std::cout << "\t-I | --ipaddr\n\t\t" <<
                                 "IP address of a server to coordinate with (server controls start/stop of measurements and any applications)" << std::endl;
                    std::cout << "\t-C [value] | --connections [value]\n\t\t" <<
                                 "Maximum number of attempts to connect to server (default: " << args.connection_attempts << "), use negative value for infinite" << std::endl;
                #else
                    std::cout << "\t-C | --clients\n\t\t" <<
                                 "Number of clients to connect to server" << std::endl;
                #endif
                std::cout << "\t-f [level] | --format [level]\n\t\t" <<
                             "Output format [0 = CSV == default | 1 = human-readable | 2 = JSON]" << std::endl;
                std::cout << "\t-l [file] | --log [file]\n\t\t" <<
                             "File to write output to" << std::endl;
                std::cout << "\t-L [file] | --errorlog [file]\n\t\t" <<
                             "File to write extra debug/errors to" << std::endl;
                std::cout << "\t-p [interval] | --poll [interval]\n\t\t" <<
                             "Floating point interval in seconds to poll stats (interval > 0)" << std::endl;
                std::cout << "\t-i [interval] | --initial-wait [interval]\n\t\t" <<
                             "Floating point interval in seconds to wait between collection initalization and starting subcommands (interval > 0)" << std::endl;
                std::cout << "\t-w [interval] | --post-wait [interval]\n\t\t" <<
                             "Floating point interval in seconds to wait after subcommand completion\n\t\t" <<
                             "A negative interval indicates to wait up to that many seconds or when temperatures return to initial levels" << std::endl;
                std::cout << "\t-t [interval] | --timeout [interval]\n\t\t" <<
                             "Floating point interval in seconds for servers and clients to cease connection attempts\n\t\t" <<
                #ifdef SERVER_MAIN
                             "Servers default to infinte waiting period until all clients arrive"
                #else
                             "Clients default to attempting to locate the server up to " << args.connection_attempts << " times (regardless of walltime duration)"
                #endif
                             << std::endl;
                std::cout << "\t-d [level] | --debug [level]\n\t\t" <<
                             "Debug verbosity (default: " << DebugOFF << ", maximum: " << DebugVerbose << ")" << std::endl;
                std::cout << "\t-v | --version\n\t\t" <<
                             "Ouput version of SensorTools and dependent libraries" << std::endl;
                std::cout << std::endl << "To automatically wrap another command with sensing for its duration, specify that command after the '--' argument" <<
                             std::endl << "ie: " << PROGNAME << " -- sleep 3" << std::endl;
                exit(EXIT_SUCCESS);
            #ifndef SERVER_MAIN
                #ifdef BUILD_CPU
                case 'c':
                    args.cpu = true;
                    break;
                #endif
                #ifdef BUILD_GPU
                case 'g':
                    args.gpu = true;
                    break;
                #endif
                #ifdef BUILD_SUBMER
                case 's':
                    args.submer = true;
                    break;
                #endif
                #ifdef BUILD_NVME
                case 'n':
                    args.nvme = true;
                    break;
                #endif
                #ifdef BUILD_PDU
                case 'P':
                    args.pdu = true;
                    break;
                #endif
                case 'I':
                    args.ip_addr = argv[optind-1];
                    break;
                case 'C':
                    args.connection_attempts = atoi(optarg);
                    break;
            #else
                case 'C':
                    args.clients = atoi(optarg);
                    break;
            #endif
            case 'f':
                args.format = atoi(optarg);
                // Validate the argument choice
                if (args.format < 0 || args.format >= count_OutputFormats) {
                    std::cerr << "Invalid setting for " << argv[optind-2] << ": " << optarg <<
                    "\n\tFormats are specified by integers [0-" << count_OutputFormats << ")" << std::endl;
                    bad_args += 1;
                }
                break;
            case 'l':
                args.log_path = std::filesystem::path(optarg);
                if (!args.log.redirect(args.log_path, false)) exit(EXIT_FAILURE);
                break;
            case 'L':
                args.error_log_path = std::filesystem::path(optarg);
                if (!args.error_log.redirect(args.error_log_path, false)) exit(EXIT_FAILURE);
                break;
            case 'p':
                args.poll = atof(optarg);
                if (args.poll <= 0) {
                    std::cerr << "Invalid setting for " << argv[optind-2] << ": " << optarg <<
                                 "\n\tPolling duration must be greater than 0" << std::endl;
                    bad_args += 1;
                }
                else args.poll_duration = static_cast<std::chrono::duration<double>>(args.poll); // Set sleep time for nanosleep
                break;
            case 'i':
                args.initial_wait = atof(optarg);
                if (args.initial_wait <= 0) {
                    std::cerr << "Invalid setting for " << argv[optind-2] << ": " << optarg <<
                                "\n\tInitial wait duration must be greater than 0" << std::endl;
                    bad_args += 1;
                }
                else args.initial_duration = static_cast<std::chrono::duration<double>>(args.initial_wait);
                break;
            case 'w':
                args.post_wait = atof(optarg);
                // Post wait can be less than zero to indicate a negative timeout
                args.post_duration = static_cast<std::chrono::duration<double>>(args.post_wait);
                break;
            case 't':
                args.timeout = atof(optarg);
                break;
            case 'd':
                args.debug = atoi(optarg);
                if (args.debug < 0 || args.debug >= count_DebugLevels) { // Invalid debug levels
                    std::cerr << "Invalid setting for " << argv[optind-2] << ": " << optarg <<
                                 "\n\tDebug levels are specified by integers [0-" << count_DebugLevels << ")" << std::endl;
                    bad_args += 1;
                }
                break;
            case 'v':
                args.version = true;
                break;
            case '?':
                std::cerr << "Unrecognized argument: " << argv[optind-1] << std::endl;
                bad_args += 1;
                break;
        }
    }
    // Post-reading logic
    if (!args.any_active()) {
        if (args.debug >= DebugVerbose) args.error_log << "Nothing active, enabling defaults" << std::endl;
        args.default_active(); // Ensure defaults always on
    }
    if (optind < argc) {
        args.wrapped = const_cast<char**>(&argv[optind]);
        if (args.debug >= DebugMinimal && args.error_log.is_cerr()) {
            std::cerr << "Treating additional arguments as a command to wrap:" << std::endl;
            for (int i = 0; i < argc-optind; i++) std::cerr << args.wrapped[i] << " ";
            std::cerr << std::endl;
        }
        if (args.poll == 0) {
            std::cerr << "Polling duration must be > 0 when wrapping a command" << std::endl;
            bad_args += 1;
        }
    }
    else args.wrapped = nullptr;
    #ifndef SERVER_MAIN
    if (args.wrapped != nullptr && args.ip_addr != nullptr) {
        std::cerr << "IP address for server given, but also a wrapped command!" << std::endl <<
                     "Server should run the wrapped command for proper synchronization" << std::endl;
        bad_args += 1;
    }
    #else
    if (args.wrapped == nullptr && args.poll != 0)
        std::cerr << "Warning! No wrapped command but polling indefinitely. You may wish to omit your polling argument for a demo or provide a wrapped command after the -- separator" << std::endl;
    #endif

    if (bad_args > 0) exit(EXIT_FAILURE);
}

// Definition of external variables for IO tools
arguments args;
// Flag that permits update logging upon collection (prevents first row from being logged prior to CSV headers)
bool update = false;

