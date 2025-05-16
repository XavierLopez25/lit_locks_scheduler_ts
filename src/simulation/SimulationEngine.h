#pragma once

#include <vector>
#include <deque>
#include "Process.h"
#include "Resource.h"
#include "Action.h"

enum class SchedulingAlgo { FIFO, SJF, SRT, RR, PRIORITY };

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

private:
    // datos originales (para reset)
    std::vector<Process>  origProcs_;
    std::vector<Resource> origRes_;
    std::vector<Action>   origActs_;

    // estado mutable
    std::vector<Process>  procs_;
    std::vector<Resource> res_;
    std::vector<Action>   acts_;

    std::vector<std::string> executionHistory_;

    int cycle_       = 0;
    SchedulingAlgo algo_;
    int rrQuantum_   = 1;
    int rrCounter_   = 0;

    std::deque<int> readyQueue_;
    int runningIdx_  = -1;

    void handleArrivals();
    void scheduleNext();
    void executeRunning();
   
};
