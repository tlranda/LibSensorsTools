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

    for (std::vector<std::unique_ptr<submer_cache>>::iterator i = known_submers.begin(); i != known_submers.end(); i++) {
        submer_cache* j = i->get();
        curl_easy_setopt(j->curl_handle, CURLOPT_WRITEDATA, &j->response);
        j->res_code = curl_easy_perform(j->curl_handle);
        if (j->res_code != CURLE_OK) {
            args.error_log << "curl_easy_perform() failed: " << curl_easy_strerror(j->res_code) << std::endl;
            args.error_log << "Submer information will not be logged from hence forth" << std::endl;
            args.submer = 0;
            submers_to_satisfy = 0;
            return 0;
        }
        j->json_data = nlohmann::json::parse(j->response)["data"];
        free(j->response);
        j->response = NULL;
        if (j->json_data["temperature"] <= j->initialSubmerTemperature)
            at_below_initial_temperature++;
        if (args.debug >= DebugVerbose || update) {
            switch (args.format) {
                case 0:
                    args.log << "," << j->json_data["temperature"];
                    break;
                case 1:
                    args.log << "Submer Temperature: " << j->json_data["temperature"] << std::endl;
                    break;
                case 2:
                    args.log << "\t\"submer-" << j->index << "-temperature\": " << j->json_data["temperature"] << "," << std::endl;
                    break;
            }
        }
    }
    return at_below_initial_temperature;
}

// Definition of external variables for Submer tools
std::vector<std::unique_ptr<submer_cache>> known_submers;
int submers_to_satisfy = 0;

