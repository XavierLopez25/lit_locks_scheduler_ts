#include "SimulationEngine.h"
#include <algorithm>
#include <stdexcept>

SimulationEngine::SimulationEngine(
    const std::vector<Process>& procs,
    const std::vector<Resource>& res,
    const std::vector<Action>& acts,
    SchedulingAlgo algo,
    int rrQuantum
) : origProcs_(procs)
  , origRes_(res)
  , origActs_(acts)
  , algo_(algo)
  , rrQuantum_(rrQuantum)
{
    reset();
}

void SimulationEngine::reset() {
    cycle_      = -1;
    rrCounter_  = 0;
    runningIdx_ = -1;
    procs_      = origProcs_;
    res_        = origRes_;
    acts_       = origActs_;
    readyQueue_.clear();
    executionHistory_.clear();
}

bool SimulationEngine::isFinished() const {
    bool allDone = std::all_of(procs_.begin(), procs_.end(),
        [](auto& p){ return p.burst == 0; });
    return allDone && runningIdx_ < 0 && readyQueue_.empty();
}

int SimulationEngine::currentCycle() const { return cycle_; }
int SimulationEngine::runningIndex() const  { return runningIdx_; }
const std::vector<Process>& SimulationEngine::procs() const { return procs_; }
const std::deque<int>&       SimulationEngine::readyQueue() const { return readyQueue_; }

void SimulationEngine::tick() {
    cycle_++;
    handleArrivals();
    scheduleNext();
    executionHistory_.push_back(
        runningIdx_ >= 0 ? procs_[runningIdx_].pid : "idle"
    );
    executeRunning();
}

void SimulationEngine::handleArrivals() {
    for (int i = 0; i < (int)procs_.size(); ++i) {
        if (procs_[i].arrival == cycle_) {
            readyQueue_.push_back(i);
        }
    }
}

void SimulationEngine::scheduleNext() {
    // FIFO
    if (runningIdx_ < 0 && !readyQueue_.empty()) {
        runningIdx_ = readyQueue_.front();
        readyQueue_.pop_front();
    }
}

void SimulationEngine::executeRunning() {
    if (runningIdx_ < 0) return;
    procs_[runningIdx_].burst--;
    rrCounter_++;
    if (procs_[runningIdx_].burst <= 0) {
        runningIdx_ = -1;
    }
}
