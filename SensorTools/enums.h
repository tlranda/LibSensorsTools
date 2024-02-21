/*
    May be pulled in multiple times in multi-file linking
    only define once
*/

#ifndef LibSensorTools_Enums
#define LibSensorTools_Enums
// Debug levels as enum to automatically count the range of argument values
enum DebugLevels {
DebugOFF,
DebugMinimal,
DebugVerbose,
count_DebugLevels
};

// Output formats that can be selected
enum OutputFormats {
OutputCSV,
OutputHuman,
OutputJSON,
count_OutputFormats
};

#endif

