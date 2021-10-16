#include "process_management.h"

/*
* Creates a process to start up the specified .exe 
* https://stackoverflow.com/questions/15435994/how-do-i-open-an-exe-from-another-c-exe
*
* @param [in] lpApplicationName path to the .exe file
* @param [in] pi reference to the PROCESS_INFORMATION structure to be filled with the process info
*/
bool process_startup(std::string application_path, PROCESS_INFORMATION& pi) {
    
    STARTUPINFOA si;

    // preparing the input structures
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    // launching the process
    bool success = CreateProcessA(application_path.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

    // closing handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return success;

}