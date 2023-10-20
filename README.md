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

## Dependencies

For CPU sensing, lm-sensors and its development tools must be installed.
See their repository linked above for instructions.

For GPU sensing, a CUDA runtime should provide NVML access.
The build tools will not link against GPU code if the runtime cannot be found.

## Build

After installing dependencies, you should be able to compile the program using the Makefile.

```
$ cd SensorTools;
$ make;
```

The executable target will be `${HOSTNAME}_senors`.

## Usage

For an initial demo, run the program with no arguments: `$ ./${HOSTNAME}_sensors;`
This will output a CSV header with all detected sensors in order:
* Timestamp
* All CPU thermal sensors
* All CPU core frequencies
* All GPU sensors # if GPUs detected at compilation time

If more than one GPU is detected at runtime, each GPUs sensors are output as a contiguous block in order.
In the current version, `CUDA_VISIBLE_DEVICES` does NOT limit which devices are monitored.

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

## Contribute

If you've found a bug or discovered missing opportunities in the tool, please [open an Issue](https://github.com/tlranda/LibSensorsTools/issues/new)

Contributions are welcome, particularly:
* AMD GPU support
* Transition from Make --> CMake
* Useful benchmarks to represent realistic/interesting workloads

Contributions are only accepted via pull requests.

