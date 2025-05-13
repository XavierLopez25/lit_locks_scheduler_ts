#pragma once
#include <string>

struct Process {
    std::string pid;
    int burst;
    int arrival;
    int priority;
};
