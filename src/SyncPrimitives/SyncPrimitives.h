#pragma once

#include <string>
#include <deque>
#include <unordered_map>
#include "Process.h"

// Resultado de un intento de sincronización
enum class SyncResult
{
    ACCESSED, // obtuvo el lock/semaforo
    WAITING   // quedó bloqueado
};

// Tipo de operación de sincronización
enum class SyncAction
{
    READ,
    WRITE,
    LOCK,
    UNLOCK,
    WAIT,
    SIGNAL,
    WAKE
};

struct SyncPrimitives
{
    std::unordered_map<std::string, Mutex> mutexes;
    std::unordered_map<std::string, Semaphore> semaphores;
};

struct SyncEvent
{
    int cycle;
    int pidIdx;
    std::string res;
    SyncResult result;
    SyncAction action;

    SyncEvent(int c, int p, const std::string &r,
              SyncResult rs, SyncAction a)
        : cycle(c), pidIdx(p), res(r), result(rs), action(a)
    {
    }
};