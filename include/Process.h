#pragma once
#include <string>
#include <deque>
#include <unordered_set>

enum class SchedulingAlgo { FIFO, SJF, SRT, RR, PRIORITY };
enum class ProcState { READY, RUNNING, BLOCKED };

struct Process {
    std::string pid;
    int burst;
    int arrival;
    int priority;
    bool justGrantedMutex = false;
    ProcState state = ProcState::READY;
    int remaining = 0;
    int completionTime = -1;

    std::unordered_set<std::string> acquiredSemaphores;
};

struct Mutex {
    bool locked = false;
    int ownerIdx = -1;              // índice en el vector de procesos
    std::deque<int> waitQueue;      // índices de procesos bloqueados
};

struct Semaphore {
    int count;
    std::deque<int> waitQueue;
    Semaphore(int c = 0): count(c) {}
};