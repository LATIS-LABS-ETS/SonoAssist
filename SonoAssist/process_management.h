#pragma once

#include <windows.h>
#include <string>

/**
* Creates a process to start up the specified .exe
* https://stackoverflow.com/questions/15435994/how-do-i-open-an-exe-from-another-c-exe
*
* \param application_path path to the .exe file.
* \param pi reference to the PROCESS_INFORMATION structure to be filled with the process info.
*/
bool process_startup(std::string application_path, PROCESS_INFORMATION& pi);