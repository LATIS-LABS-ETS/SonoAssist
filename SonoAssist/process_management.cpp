#include "process_management.h"

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