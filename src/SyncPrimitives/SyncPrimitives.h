#pragma once

#include <string>
#include <deque>
#include <unordered_map>
#include "Process.h"
#include "SyncEnums.h"

struct SyncPrimitives {
    std::unordered_map<std::string, Mutex>    mutexes;
    std::unordered_map<std::string, Semaphore> semaphores;
};

struct SyncEvent {
    int         cycle;    
    int         pidIdx;   
    std::string res;      
    SyncResult  result;   
    SyncAction  action;   

    SyncEvent(int c, int p, const std::string& r,
              SyncResult rs, SyncAction a)
      : cycle(c), pidIdx(p), res(r), result(rs), action(a)
    {}
};