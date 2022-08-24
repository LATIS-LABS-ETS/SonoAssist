#ifndef PROCESSMANAGEMENT_H
#define PROCESSMANAGEMENT_H

#include <windows.h>
#include <string>

bool process_startup(std::string application_path, PROCESS_INFORMATION& pi);

#endif