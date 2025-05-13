#pragma once
#include <string>
#include <vector>
#include "Process.h"
#include "Resource.h"
#include "Action.h"

// Declaraciones
std::vector<Process>  loadProcesses(const std::string& path);
std::vector<Resource> loadResources(const std::string& path);
std::vector<Action>   loadActions(const std::string& path);
