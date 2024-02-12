# Sensor Tools

A C/C++ library for monitoring CPU and GPU hardware via other 1st and 3rd party tools.
Sensor Tools primarily serves to monitor temperature and power sensors across exposed hardware interfaces.

## CPU Sensing
CPU thermal sensors are detected via [lm-sensors](https://github.com/lm-sensors/lm-sensors/), using the libsensor-devel C API.
All sensors that can be probed are collected on each update.
CPU core frequencies are recorded using the linux cpufreq logs in `/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq`.
All detected files are read on each update.

## GPU Sensing
CUDA GPU properties are primarily read through [NVML](https://developer.nvidia.com/nvidia-management-library-nvml)'s C API and the [CUDA Runtime API](https://docs.nvidia.com/cuda/cuda-runtime-api/index.html).
These properties include:
* GPU temperature / memory junction temperature
* Power usage, state and limit
* Compute and Memory Utilization

## Submer Sensing
A submer liquid imemrsion cooling pod provides a web-api to read thermal sensor values.
The particular API link should be similar to "http://your_domain/api/realTime", and should be specified in [SensorTools/submer_api.h](SensorTools/submer_api.h).

## NVMe Sensing
NVMe devices can also include thermal sensors, read through [libnvme](https://github.com/linux-nvme/libnvme/tree/93aecc45b3453406e9b80e45012ae37a2ad1c5e4).
This dependency is added as a submodule as not all package managers provide direct support.

## Dependencies

This project is built with Nlohmann JSON, a submodule is provided to ensure compatibility.
Clone the repository by adding `--recurse-submodules` to your `git clone` command, or run `git submodule update --init --recursive` to check them out if you didn't fetch them during repository clonging.

For CPU sensing, lm-sensors and its development tools must be installed.
See their repository linked above for instructions.
The CMake build variable is `-DBUILD_CPU=ON`, which is on by default.

For GPU sensing, a CUDA runtime should provide NVML access.
The build tools will not link against GPU code if the runtime cannot be found.
The CMake build variable is `-DBUILD_GPU=ON`, which is OFF by default.

For Submer sensing, the webapi linked in the submer_api.h document must be accessible by your machine.
The CMake build variable is `-DBUILD_POD=ON`, which is OFF by default.

For NVMe sensing, libnvme must be installed through the provided LibNVMe submodule.
The CMake build variable is `-DBUILD_NVME=ON`, which is OFF by default.

## Build

After installing dependencies, you should be able to compile the program using the Makefile.

```
$ cd SensorTools;
$ mkdir build;
$ cd build;
$ cmake <-DBUILD_*=ON for each tool you want> ..;
$ make;
```

The executable target will be `${HOSTNAME}_sensors`.

## Usage

For an initial demo, run the program with no arguments: `$ ./${HOSTNAME}_sensors;`
This will output a CSV header with a timestamp and all detected sensors in order.

If more than one of a particular sensor are detected at runtime, all observations for each class of sensors are output as a contiguous block in order.
NOTE: In the current version, `CUDA_VISIBLE_DEVICES` does NOT limit which GPU devices are monitored.

For full options to customize program behaviors, you can use the `-h | --help` argument, ie: `$ ./${HOSTNAME}_sensors -h;` or `$ ./${HOSTNAME}_sensors --help`;

### Detailed Explanations of Some Runtime Options
* Polling
    + Polling intervals can be set to less than one second, however most measurements collected by this tool do not meaningfully change on millisecond time scales etc.
    + To determine the lowest reasonable bound of polling, run with verbose debug for several seconds. The verbose logs include timing for each update cycle, which can help to determine the fastest interval that updates can be generated.
        - Metric updates are serialized in a single thread to minimize timing discrepancies between recorded metrics while maintaining low overall resource utilization
* Wrapped sensing
    + After specifying any/all runtime arguments to sensors, use the `--` separator and add another command or executable and its arguments.
    + After initializing all tools, the sensor program will fork/exec your command and continue sensing until it terminates
    + Use `-i | --interval` to specify an interval to collect sensing data PRIOR to starting your command (ie: set sensor baselines prior to the command)
    + Use `-w | --post-wait` to specify an interval to collect sensing data AFTER your command completes (ie: observe behavior normalization after the command)
        - Specifying a negative value for this argument permits an early-exit from the post-wait when all thermal sensors read at/below the last recorded temperature prior to starting your command.
* File I/O
    + The `-l [FILE] | --log [FILE]` argument can be used to change the destination for standard output from the sensors program -- this comprises each update's sensing data
        - When outputting in the default format, this forms a proper CSV file that can easily be consumed by other data analysis tools you may have
    + The `-L [FILE] | --errorlog [FILE]` argument can be used to change the destination for standard errors from the sensors program
        - The contents of this output depend on your debug verbosity
        - At the lowest level, only the initialize and shutdown timestamps will be provided
        - At any higher level:
            1) The values of runtime arguments to the program are provided
            2) Errors detected during sensor caching are provided
            3) The number of identified thermal sensors for CPU and GPU are provided
            4) The elapsed time for each sensor update cycle is provided
            5) The shutdown signal for the program (0 == normal exit without signal) is provided
        - At the highest level:
            1) The version of all tools is provided
            2) Debug data during the sensors detection/caching process is provided
            3) Update functions log each function entry/exit
            4) Waiting period expectations are provided
    + When using a wrapped command, providing both above redirections can permit shell redirection to effectively isolate the outputs from your command to file
        - Some output will go to standard output (NMW == no matter what; D#+ == when debug argument is # or greater):
            1) [NMW] Help arguments when specified by `-h | --help`
        - Some output will go to standard error no matter what:
            1) [NMW] Errors recognized while parsing sensor tool arguments
            2) [D1+] The recognized command to wrap
* JSON output
    + The `-f 2 | --format 2` argument will output the normal data in JSON rather than CSV format. Due to the flexibility of data coherency in JSONs, this unifies some key timing information (such as when a wrapped command starts and stops) all into a single output file for your downstream parsing purposes.
        - The JSON format also automatically includes all arguments and versions in its records
        - The recorded data is exactly the same, however the field naming conventions are changed to prefer '-' separators over '\_' separators for CSVs.
        - Due to the JSON format, each polling cycle ends with a dummy field called "dummy-end" which is always true.
        - While the JSON format is larger on disk compared to the CSV format, we observe similar minimum update latency to the CSV and expect there is no significant difference in tool overhead/performance as the actual API access and data collation are more time-consuming than basic I/O.
* Server-Client Setup
    + Utilizing the separate build targets, multi-node processing is provided through a server-client infrastructure.
    + Generally, the server should be the only runtime that has a wrapped command.
      - All clients will monitor their own nodes, but do not transmit data to the server. Have each client write to a distinct output file.
      - The server process does not include any tools for sensing and only acts as a coordinator for its clients.
    + Server controls:
      - The server utilizes `-C [number] | --clients [number]` to know the exact number of clients that will connect (Only use one client per node).
      - The server also utilizes the `-t [timeout] | --timeout [timeout]` to early-terminate if the expected number of clients fail to arrive.
    + Client controls:
      - The client utilizes `-I [IP_ADDR] | --ip-address [IP_ADDR]` to know which IP address will connect it to a server (uses port 8080 unless redefined in [control.h.in](control.h.in))
      - The client has a limited number of attempts to reach the server (default: 10, redefined by `-C [number] | --connection-attempts [number]`, use a negative value for infinite attempts) and can also take a timeout via `-t [timeout] | --timeout [timeout]`.

## Contribute

If you've found a bug or discovered missing opportunities in the tool, please [open an Issue](https://github.com/tlranda/LibSensorsTools/issues/new)

Contributions are welcome, particularly:
* AMD GPU support
* Useful benchmarks to represent realistic/interesting workloads

Contributions are only accepted via pull requests.

