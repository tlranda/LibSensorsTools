/*
  Some hosts do not provide NVIDIA / AMD headers etc
  Use preprocessor macros to define a safe version of
  all calls when compiler definitions (or lack thereof)
  indicates the GPU tool is unsupported.

  Current compiler definitions are documented in gpu_tools.h

  It is preferred to keep all preprocessor variants at minimal scope (ie: relevant block or at most a single function definition)
*/
#include "gpu_tools.h"

#ifdef GPU_ENABLED
void cache_gpus(void) {
    // No caching if we aren't going to query the GPUs
    if (!args.gpu) return;

    int n_devices;
    cudaGetDeviceCount(&n_devices);

    for (int i = 0; i <= n_devices; i++) {
        if (args.debug >= DebugVerbose) args.error_log << "Begin caching GPU " << i << std::endl;
        // Prepare candidate
        gpu_cache candidate;
        candidate.device_ID = i;
        nvmlDeviceGetHandleByIndex_v2(static_cast<unsigned int>(i), &candidate.device_Handle);
        candidate.temperature_field.fieldId = NVML_FI_DEV_MEMORY_TEMP; // Memory temperature

        // Initial value caching
        nvmlDeviceGetName(candidate.device_Handle, candidate.deviceName, NAME_BUFFER_SIZE);
        nvmlDeviceGetTemperature(candidate.device_Handle, NVML_TEMPERATURE_GPU, &candidate.gpu_temperature);
        candidate.gpu_initialTemperature = candidate.gpu_temperature;
        nvmlDeviceGetFieldValues(candidate.device_Handle, 1, &candidate.temperature_field);
        candidate.mem_initialTemperature = candidate.mem_temperature = candidate.temperature_field.value.uiVal;
        nvmlDeviceGetPowerUsage(candidate.device_Handle, &candidate.powerUsage);
        nvmlDeviceGetEnforcedPowerLimit(candidate.device_Handle, &candidate.powerLimit);
        nvmlDeviceGetUtilizationRates(candidate.device_Handle, &candidate.utilization);
        nvmlDeviceGetMemoryInfo(candidate.device_Handle, &candidate.memory);
        nvmlDeviceGetPerformanceState(candidate.device_Handle, &candidate.pState);

        if (args.debug >= DebugVerbose) args.error_log << "Finished caching GPU " << i << std::endl;
        known_gpus.push_back(candidate);
        gpus_to_satisfy += 2;
    }
    if (args.debug >= DebugMinimal) args.error_log << "Tracking " << gpus_to_satisfy << " GPU temperature sensors" << std::endl;
}
#else
void cache_gpus(void) {
    if (args.debug >= DebugMinimal) args.error_log << "Not compiled with GPU_ENABLED definition -- no GPU support" << std::endl;
}
#endif

#ifdef GPU_ENABLED
int update_gpus(void) {
    if (args.debug >= DebugVerbose) args.error_log << "Update GPUs" << std::endl;
    int at_below_initial_temperature = 0;
    for (std::vector<gpu_cache>::iterator i = known_gpus.begin(); i != known_gpus.end(); i++) {
        if (args.debug >= DebugVerbose) {
            switch (args.format) {
                case OutputHuman:
                    args.log << "GPU " << i->device_ID << " " << i->deviceName << " BEFORE" << std::endl;
                    args.log << "\tGPU Temperature: " << i->gpu_temperature << std::endl;
                    args.log << "\tMemory Temperature: " << i->mem_temperature << std::endl;
                    args.log << "\tPower Usage/Limit: " << i->powerUsage << " / " << i->powerLimit << std::endl;
                    args.log << "\tUtilization: " << i->utilization.gpu << "\% GPU " << i->utilization.memory << "\% Memory " << std::endl;
                    args.log << "\tPerformance State: " << i->pState << std::endl;
                case OutputCSV:
                case OutputJSON:
                    break;
            }
        }
        nvmlDeviceGetTemperature(i->device_Handle, NVML_TEMPERATURE_GPU, &i->gpu_temperature);
        if (i->gpu_temperature <= i->gpu_initialTemperature) at_below_initial_temperature++;
        nvmlDeviceGetFieldValues(i->device_Handle, 1, &i->temperature_field);
        i->mem_temperature = i->temperature_field.value.uiVal;
        if (i->mem_temperature <= i->mem_initialTemperature) at_below_initial_temperature++;
        nvmlDeviceGetPowerUsage(i->device_Handle, &i->powerUsage);
        nvmlDeviceGetEnforcedPowerLimit(i->device_Handle, &i->powerLimit);
        nvmlDeviceGetUtilizationRates(i->device_Handle, &i->utilization);
        nvmlDeviceGetMemoryInfo(i->device_Handle, &i->memory);
        nvmlDeviceGetPerformanceState(i->device_Handle, &i->pState);
        if (args.debug >= DebugVerbose || update) {
            switch (args.format) {
                case OutputCSV:
                    args.log << "," << i->deviceName << "," << i->gpu_temperature << "," << i->mem_temperature << "," <<
                             i->powerUsage << "," << i->powerLimit << "," << i->utilization.gpu << "," <<
                             i->utilization.memory << "," << i->memory.used << "," << i->memory.total << "," << i->pState;
                    break;
                case OutputHuman:
                    args.log << "GPU " << i->device_ID << " " << i->deviceName << " AFTER" << std::endl;
                    args.log << "\tGPU Temperature: " << i->gpu_temperature << std::endl;
                    args.log << "\tMemory Temperature: " << i->mem_temperature << std::endl;
                    args.log << "\tPower Usage/Limit: " << i->powerUsage << " / " << i->powerLimit << std::endl;
                    args.log << "\tUtilization: " << i->utilization.gpu << "\% GPU " << i->utilization.memory << "\% Memory " << std::endl;
                    args.log << "\tPerformance State: " << i->pState << std::endl;
                    break;
                case OutputJSON:
                    args.log << "\t\"gpu-" << i->device_ID << "-name\": \"" << i->deviceName << "\"," << std::endl <<
                                "\t\"gpu-" << i->device_ID << "-gpu-temperature\": " << i->gpu_temperature << "," << std::endl <<
                                "\t\"gpu-" << i->device_ID << "-memory-temperature\": " << i->mem_temperature << "," << std::endl <<
                                "\t\"gpu-" << i->device_ID << "-power-usage\": " << i->powerUsage << "," << std::endl <<
                                "\t\"gpu-" << i->device_ID << "-power-limit\": " << i->powerLimit << "," << std::endl <<
                                "\t\"gpu-" << i->device_ID << "-utilization-gpu\": " << i->utilization.gpu << "," << std::endl <<
                                "\t\"gpu-" << i->device_ID << "-utilization-memory\": " << i->utilization.memory << "," << std::endl <<
                                "\t\"gpu-" << i->device_ID << "-memory-used\": " << i->memory.used << "," << std::endl <<
                                "\t\"gpu-" << i->device_ID << "-memory-total\": " << i->memory.total << "," << std::endl <<
                                "\t\"gpu-" << i->device_ID << "-pstate\": " << i->pState << "," << std::endl;
                    break;
            }
        }
    }
    return at_below_initial_temperature;
}
#else
int update_gpus(void) {
    if (args.debug >= DebugVerbose) args.error_log << "No GPU updates -- Not compiled with GPU_ENABLED definition" << std::endl;
    return 0;
}
#endif



// Definition of external variables for GPU tools
std::vector<gpu_cache> known_gpus;
int gpus_to_satisfy = 0;

