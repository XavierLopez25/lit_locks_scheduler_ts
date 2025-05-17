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
    cycle_            = -1;
    rrCounter_        = 0;
    runningIdx_       = -1;
    procs_            = origProcs_;
    res_              = origRes_;
    acts_             = origActs_;
    readyQueue_.clear();
    executionHistory_.clear();

    if (algo_ == SchedulingAlgo::SJF) {
        // Carga todos los procesos en la cola de listos de golpe
        for (int i = 0; i < (int)procs_.size(); ++i) {
            readyQueue_.push_back(i);
        }
    }
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

    // Sólo para los algoritmos basados en arrival:
    if (algo_ == SchedulingAlgo::FIFO ||
        algo_ == SchedulingAlgo::SRT ||
        algo_ == SchedulingAlgo::RR ||
        algo_ == SchedulingAlgo::PRIORITY) {
        handleArrivals();
    }


    // scheduling según tipo
    bool preemptivo = (algo_==SchedulingAlgo::SRT ||
                       algo_==SchedulingAlgo::RR ||
                       algo_==SchedulingAlgo::PRIORITY);
    if (preemptivo || runningIdx_ < 0) {
        scheduleNext();
    }

    executionHistory_.push_back(
        runningIdx_>=0 ? procs_[runningIdx_].pid : "idle"
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
    switch(algo_) {

        // Algoritmo FIFO
        case SchedulingAlgo::FIFO:
        if (runningIdx_<0 && !readyQueue_.empty()) {
            runningIdx_ = readyQueue_.front();
            readyQueue_.pop_front();
        }
        break;

        // Algoritmo Shortest Job Fist
        case SchedulingAlgo::SJF:
        if (runningIdx_<0 && !readyQueue_.empty()) {
            // busca el índice en readyQueue_ con menor burst
            auto it = std::min_element(
                readyQueue_.begin(), readyQueue_.end(),
                [&](int a, int b){
                    return procs_[a].burst < procs_[b].burst;
                }
            );
            runningIdx_ = *it;
            readyQueue_.erase(it);
        }
        break;

        // Algoritmo Shortest Remaining Time (SRT)
        case SchedulingAlgo::SRT:
        {
            if (runningIdx_ >= 0){
                readyQueue_.push_back(runningIdx_);
                runningIdx_ = -1;
            }

            if(!readyQueue_.empty()){
                auto it = std::min_element(
                    readyQueue_.begin(), readyQueue_.end(),
                    [&](int a, int b){
                        return procs_[a].burst < procs_[b].burst;
                    }
                );
                runningIdx_ = *it;
                readyQueue_.erase(it);
            }

        }
        break;

        //Algoritmo Priority Scheduling
        case SchedulingAlgo::PRIORITY:
        {
            if (runningIdx_ >= 0) {
                readyQueue_.push_back(runningIdx_);
                runningIdx_ = -1;
            }

            if (!readyQueue_.empty()) {
                auto it = std::min_element(
                    readyQueue_.begin(), readyQueue_.end(),
                    [&](int a, int b){
                        return procs_[a].priority < procs_[b].priority;
                    }
                );
                runningIdx_ = *it;
                readyQueue_.erase(it);
            }
        }
        break;


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
