#include <iostream>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// Helper function to execute a command and capture the output
string cmdline(const char* cmd) {
    char buffer[128];
    string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw runtime_error("popen failed!");
    try {
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
    } catch (...) {
        fclose(pipe);
        throw;
    }
    fclose(pipe);
    return result;
}

// Function to parse the output from omreport
void parsePowerMonitoringData(const string& data) {
    stringstream ss(data);
    string line;

    // Variables to store parsed data
    bool has_read_Reading = false, has_read_PS1 = false;
    string Reading, PS1_Current;
    int rVal;
    float pVal;
    int lineIdx = 0;

    // Example of parsing specific details, adjust based on actual output format
    while ((!has_read_Reading || !has_read_PS1) && getline(ss, line)) { //
        if (!has_read_Reading && line.find("Reading") != string::npos) {
            Reading = line.substr(line.find(":") + 2);
            Reading = Reading.substr(0, Reading.find(" "));
            has_read_Reading = true;
            rVal = atoi(Reading.c_str());
        }
        if (!has_read_PS1 && line.find("PS1 Current 1") != string::npos) {
            PS1_Current = line.substr(line.find(":") + 2);
            PS1_Current = PS1_Current.substr(0, PS1_Current.find(" "));
            has_read_PS1 = true;
            pVal = atof(PS1_Current.c_str());
        }
        lineIdx++;
    }

    // Output parsed values (you can process them further)
    cout << "Read " << lineIdx << " lines" << endl;
    cout << "Reading: " << Reading << " (As int: " << rVal << ")" << endl;
    cout << "PS1 Current: " << PS1_Current << " (As float: " << pVal << ")" << endl;
}

int main() {
    try {
        string command = "omreport chassis pwrmonitoring";
        string result = cmdline(command.c_str());

        // Parse the result
        parsePowerMonitoringData(result);
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
    return 0;
}

