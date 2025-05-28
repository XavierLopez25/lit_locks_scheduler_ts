#pragma once

#include "Process.h"
#include "Resource.h"
#include "Action.h"
#include "SyncPrimitives/SyncPrimitives.h"
#include "common/SimMode.h"
#include <unordered_map>
#include <vector>
#include <deque>

class SimulationEngine {
public:
    SimulationEngine(const std::vector<Process>& procs,
                     const std::vector<Resource>& res,
                     const std::vector<Action>& acts,
                     SchedulingAlgo algo,
                     int rrQuantum = 1);

    void reset();
    void tick();
    bool isFinished() const;
    int  currentCycle() const;
    int  runningIndex() const;
    const std::vector<Process>&  procs() const;
    const std::deque<int>&       readyQueue() const;

    const std::vector<std::string>& getExecutionHistory() const {
        return executionHistory_;
    }
    void setAlgorithm(SchedulingAlgo algo) {
        algo_ = algo;
    }
    SchedulingAlgo getAlgorithm() const {
        return algo_;
    }

    float getAverageWaitingTime() const;

    int rrQuantum_   = 1;

    const std::vector<SyncEvent>& getSyncLog() const { return syncLog_; }

    void setMode(SimMode m) { mode_ = m; }
    SimMode getMode() const   { return mode_; }

    bool isMutex(const std::string& name) const;

    const std::unordered_map<std::string, Mutex>& getMutexes() const {
        return sync_.mutexes;
    }
    const std::unordered_map<std::string, Semaphore>& getSemaphores() const {
        return sync_.semaphores;
    }

private:

    SimMode mode_ = SimMode::SCHEDULING;

    // datos originales (para reset)
    std::vector<Process>  origProcs_;
    std::vector<Resource> origRes_;
    std::vector<Action>   origActs_;

    SyncPrimitives sync_;
    std::vector<SyncEvent> syncLog_; 
    std::unordered_map<std::string, Mutex>    mutexes_;
    std::unordered_map<std::string, Semaphore> semaphores_;

    // estado mutable
    std::vector<Process>  procs_;
    std::vector<Resource> res_;
    std::vector<Action>   acts_;

    std::vector<std::string> executionHistory_;

    int cycle_       = 0;
    SchedulingAlgo algo_;
    int rrCounter_   = 0;

    int maxSyncCycle_; 
    
    std::deque<int> readyQueue_;
    int runningIdx_  = -1;

    int findProcessIndex(const std::string& pid) const;

    void handleArrivals();
    void scheduleNext();
    void executeRunning();
    void handleSyncActions();
};