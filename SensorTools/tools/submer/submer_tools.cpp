#include "submer_tools.h"

size_t curlJSONCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t real_size = size * nmemb;

    char** response = (char **)userp;
    // Realloc pointer on reuse
    *response = (char*)realloc(*response, real_size+1);
    if (*response) {
        memcpy(*response, contents, real_size);
        (*response)[real_size] = 0; // Null-terminate the string
    }
    return real_size;
}

void cache_submers(void) {
    // No caching if we aren't going to query the pods
    if (!args.submer) return;

    if (args.debug >= DebugVerbose)
        args.error_log << "Begin caching submer" << std::endl;

    // Prepare candidate
    std::unique_ptr<submer_cache> candidate = std::make_unique<submer_cache>();
    candidate->index = 0;

    // Set up Curl Handle
    candidate->curl_handle = curl_easy_init();
    if (candidate->curl_handle) {
        curl_easy_setopt(candidate->curl_handle, CURLOPT_URL, SUBMER_URL);
        curl_easy_setopt(candidate->curl_handle, CURLOPT_WRITEFUNCTION, curlJSONCallback);
        curl_easy_setopt(candidate->curl_handle, CURLOPT_WRITEDATA, &candidate->response);

        // Make first call to set up information
        candidate->res_code = curl_easy_perform(candidate->curl_handle);
        if (candidate->res_code != CURLE_OK) {
            args.error_log << "curl_easy_perform() failed: " << curl_easy_strerror(candidate->res_code) << std::endl;
            args.error_log << "Submer information will not be logged from hence forth" << std::endl;
            args.error_log << "Finished inspecting submer" << std::endl;
            args.submer = 0;
            submers_to_satisfy = 0;
        }
        else {
            // We only need the data portion
            candidate->json_data = nlohmann::json::parse(candidate->response)["data"];
            // No longer need this memory
            free(candidate->response);
            candidate->response = NULL;
            candidate->initialSubmerTemperature = candidate->json_data["temperature"];
            if (args.debug >= DebugVerbose)
                args.error_log << "Finished inspecting submer" << std::endl;
            known_submers.emplace_back(std::move(candidate));
            submers_to_satisfy++;
        }
    }
    else {
        args.error_log << "curl_easy_init() failed" << std::endl;
        args.error_log << "Submer information will not be logged from hence forth" << std::endl;
        args.error_log << "Finished inspecting submer" << std::endl;
        args.submer = 0;
        submers_to_satisfy = 0;
    }
    if (args.debug >= DebugMinimal)
        args.error_log << "Tracking " << submers_to_satisfy << " Sumber pods" << std::endl;
}

int update_submers(void) {
    if (args.debug >= DebugVerbose) args.error_log << "Update Submers" << std::endl;
    int at_below_initial_temperature = 0;
    const long timeout = 200L;

    for (std::vector<std::unique_ptr<submer_cache>>::iterator i = known_submers.begin(); i != known_submers.end(); i++) {
        submer_cache* j = i->get();
        curl_easy_setopt(j->curl_handle, CURLOPT_WRITEDATA, &j->response);
        // Ensure timeout is enforced somewhat reasonably
        curl_easy_setopt(j->curl_handle, CURLOPT_TIMEOUT_MS, timeout);
        j->res_code = curl_easy_perform(j->curl_handle);
        if (j->res_code != CURLE_OK) {
            if (j->res_code == CURLE_OPERATION_TIMEDOUT) {
                args.error_log << "curl_easy_perform() timedout (" << timeout << " milliseconds). Using stale data!!" << std::endl;
            }
            else {
                args.error_log << "curl_easy_perform() failed: " << curl_easy_strerror(j->res_code) << " (code: " << j->res_code << ")" << std::endl;
                args.error_log << "Submer information will not be logged from hence forth" << std::endl;
                args.submer = 0;
                submers_to_satisfy = 0;
                return 0;
            }
        }
        else {
            // Parse response
            j->json_data = nlohmann::json::parse(j->response)["data"];
            free(j->response);
            j->response = NULL;
        }
        if (j->json_data["temperature"] <= j->initialSubmerTemperature)
            at_below_initial_temperature++;
        if (args.debug >= DebugVerbose || update) {
            switch (args.format) {
                case OutputCSV:
                    args.log << "," << j->json_data["temperature"]
                    << "," << j->json_data["consumption"]
                    << "," << j->json_data["dissipation"]
                    << "," << j->json_data["dissipationC"]
                    << "," << j->json_data["dissipationW"]
                    << "," << j->json_data["mpue"]
                    << "," << j->json_data["pump1rpm"]
                    << "," << j->json_data["pump2rpm"]
                    << "," << j->json_data["cti"]
                    << "," << j->json_data["cto"]
                    << "," << j->json_data["cf"]
                    << "," << j->json_data["wti"]
                    << "," << j->json_data["wto"]
                    << "," << j->json_data["wf"];
                    break;
                case OutputHuman:
                    args.log << "Submer Temperature: " << j->json_data["temperature"] << std::endl
                    << "Submer Consumption: " << j->json_data["consumption"] << std::endl
                    << "Submer Dissipation: " << j->json_data["dissipation"] << std::endl
                    << "Submer DissipationC: " << j->json_data["dissipationC"] << std::endl
                    << "Submer DissipationW: " << j->json_data["dissipationW"] << std::endl
                    << "Submer mPUE: " << j->json_data["mpue"] << std::endl
                    << "Submer Pump 1 RPM: " << j->json_data["pump1rpm"] << std::endl
                    << "Submer Pump 2 RPM: " << j->json_data["pump2rpm"] << std::endl
                    << "Submer CTI: " << j->json_data["cti"] << std::endl
                    << "Submer CTO: " << j->json_data["cto"] << std::endl
                    << "Submer CF: " << j->json_data["cf"] << std::endl
                    << "Submer WTI: " << j->json_data["wti"] << std::endl
                    << "Submer WTO: " << j->json_data["wto"] << std::endl
                    << "Submer WF: " << j->json_data["wf"] << std::endl;
                    break;
                case OutputJSON:
                    args.log << "\t\"submer-" << j->index << "-temperature\": " << j->json_data["temperature"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-consumption\": " << j->json_data["consumption"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-dissipation\": " << j->json_data["dissipation"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-dissipationC\": " << j->json_data["dissipationC"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-dissipationW\": " << j->json_data["dissipationW"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-mPUE\": " << j->json_data["mpue"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-pump1rpm\": " << j->json_data["pump1rpm"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-pump2rpm\": " << j->json_data["pump2rpm"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-cti\": " << j->json_data["cti"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-cto\": " << j->json_data["cto"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-cf\": " << j->json_data["cf"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-wti\": " << j->json_data["wti"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-wto\": " << j->json_data["wto"] << "," << std::endl
                    << "\t\"submer-" << j->index << "-wf\": " << j->json_data["wf"] << "," << std::endl;
                    break;
            }
        }
    }
    return at_below_initial_temperature;
}

// Definition of external variables for Submer tools
std::vector<std::unique_ptr<submer_cache>> known_submers;
int submers_to_satisfy = 0;

