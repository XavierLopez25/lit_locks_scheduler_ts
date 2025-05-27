#pragma once

#include <string>
#include <deque>
#include <unordered_map>
#include "Process.h"   

enum class SyncResult { ACCESSED, WAITING };

// Todas las primitivas de sincronización que guarda el sistema
struct SyncPrimitives {
    std::unordered_map<std::string, Mutex>    mutexes;
    std::unordered_map<std::string, Semaphore> semaphores;
};

struct SyncEvent {
    int cycle;           // ciclo en el que ocurre
    int pidIdx;          // índice de proceso en procs_
    std::string res;     // nombre del recurso
    SyncResult result;   // ACCESSED si obtuvo el lock/semaforo, WAITING si quedó bloqueado
};