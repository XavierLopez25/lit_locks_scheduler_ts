#pragma once
#include <string>
#include <vector>
#include "Process.h"
#include "Resource.h"
#include "Action.h"

// Funciones para parsear líneas y cargar vectores
Process    parseProcessLine(const std::string& line);
Resource   parseResourceLine(const std::string& line);
Action     parseActionLine(const std::string& line);

std::vector<Process>  loadProcesses(const std::string& filepath);
std::vector<Resource> loadResources(const std::string& filepath);
std::vector<Action>   loadActions(const std::string& filepath);