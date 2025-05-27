#include "SimulationEngine.h"
#include "SyncPrimitives/SyncPrimitives.h"
#include "Process.h"      
#include "Resource.h"
#include "common/SimMode.h"
#include "Action.h"
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

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

    if (algo_ == SchedulingAlgo::SJF ||
        algo_ == SchedulingAlgo::PRIORITY) {
        // Carga todos los procesos en la cola de listos de golpe
        for (int i = 0; i < (int)procs_.size(); ++i) {
            readyQueue_.push_back(i);
        }
    }

    syncLog_.clear();

    sync_.mutexes.clear();
    sync_.semaphores.clear();

    for (auto &r : origRes_) {
        if (r.count == 1)
            sync_.mutexes[r.name] = Mutex{};
        else
            sync_.semaphores[r.name] = Semaphore(r.count);
    }
}

bool SimulationEngine::isMutex(const std::string& name) const {
    return sync_.mutexes.count(name) > 0;
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

    if (mode_ == SimMode::SCHEDULING) {
        // 1) arrivals
        if (algo_ == SchedulingAlgo::FIFO || 
            algo_ == SchedulingAlgo::SRT  ||
            algo_ == SchedulingAlgo::RR) {
            handleArrivals();
        }

        // 2) scheduling
        bool preemptivo = (algo_==SchedulingAlgo::SRT ||
                           algo_==SchedulingAlgo::RR ||
                           algo_==SchedulingAlgo::PRIORITY);
        if (preemptivo || runningIdx_ < 0) {
            scheduleNext();
        }

        // 3) record & execute
        executionHistory_.push_back(
            runningIdx_>=0 ? procs_[runningIdx_].pid : "idle"
        );
        executeRunning();

    } else {
        // —————— MODO SYNCHRONIZATION ——————
        // sólo procesar acciones de sincronización
        handleSyncActions();
    }
}

void SimulationEngine::handleArrivals() {
    for (int i = 0; i < (int)procs_.size(); ++i) {
        if (procs_[i].arrival == cycle_) {
            readyQueue_.push_back(i);
        }
    }
}

int SimulationEngine::findProcessIndex(const std::string& pid) const {
    for (int i = 0; i < (int)procs_.size(); ++i) {
        if (procs_[i].pid == pid)
            return i;
    }
    return -1; // No encontrado
}

void SimulationEngine::handleSyncActions() {
    for (auto &act : acts_) {
        if (act.cycle != cycle_) continue;

        int idx = findProcessIndex(act.pid);
        if (idx < 0) continue;

        Process& p = procs_[idx];

        // Helper para registrar evento
        auto logEvent = [&](SyncResult r){
            syncLog_.push_back({ cycle_, idx, act.res, r });
        };

        if (act.type == "READ" || act.type == "WRITE") {
            auto& s = sync_.semaphores[act.res];  

            if (s.count > 0) {
                s.count--;
                logEvent(SyncResult::ACCESSED);
            } else {
                p.state = ProcState::BLOCKED;
                s.waitQueue.push_back(idx);
                logEvent(SyncResult::WAITING);
            }
        }

        if (act.type == "LOCK") {
            auto &m = sync_.mutexes[act.res];
            if (!m.locked) {
                m.locked = true;
                m.ownerIdx = idx;
                logEvent(SyncResult::ACCESSED);
            } else {
                p.state = ProcState::BLOCKED;
                m.waitQueue.push_back(idx);
                logEvent(SyncResult::WAITING);
            }

        } else if (act.type == "UNLOCK") {
            auto &m = sync_.mutexes[act.res];
            if (m.ownerIdx == idx) {
                if (m.waitQueue.empty()) {
                    m.locked = false;
                    m.ownerIdx = -1;
                } else {
                    int next = m.waitQueue.front();
                    m.waitQueue.pop_front();
                    m.ownerIdx = next;
                    procs_[next].state = ProcState::READY;
                    readyQueue_.push_back(next);
                }
            }

        } else if (act.type == "WAIT") {
            auto &s = sync_.semaphores[act.res];
            if (s.count > 0) {
                s.count--;
                logEvent(SyncResult::ACCESSED);
            } else {
                p.state = ProcState::BLOCKED;
                s.waitQueue.push_back(idx);
                logEvent(SyncResult::WAITING);
            }

        } else if (act.type == "SIGNAL") {
            auto &s = sync_.semaphores[act.res];
            if (s.waitQueue.empty()) {
                s.count++;
            } else {
                int next = s.waitQueue.front();
                s.waitQueue.pop_front();
                procs_[next].state = ProcState::READY;
                readyQueue_.push_back(next);
            }
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

        // Algoritmo Round Robin
        case SchedulingAlgo::RR:
        {
            if (runningIdx_ >= 0 && rrCounter_ >= rrQuantum_) {
                readyQueue_.push_back(runningIdx_);
                runningIdx_ = -1;
                rrCounter_ = 0;
            }

            if (runningIdx_ < 0 && !readyQueue_.empty()){
                runningIdx_ = readyQueue_.front();
                readyQueue_.pop_front();
                rrCounter_ = 0;
            }
        }
        break;
    }
}

float SimulationEngine::getAverageWaitingTime() const {
    float total = 0.0f;
    int count = 0;
    std::unordered_map<std::string, int> firstAppearance;

    for (int i = 0; i < (int)executionHistory_.size(); ++i) {
        const std::string& pid = executionHistory_[i];
        if (pid == "idle") continue;
        if (!firstAppearance.count(pid)) {
            firstAppearance[pid] = i;
        }
    }

    for (const auto& proc : procs_) {
        if (firstAppearance.count(proc.pid)) {
            int wait = firstAppearance[proc.pid] - proc.arrival;
            total += wait;
            count++;
        }
    }

    return count == 0 ? 0.0f : total / count;
}

void SimulationEngine::executeRunning() {
    if (runningIdx_ < 0) return;
    procs_[runningIdx_].burst--;
    rrCounter_++;
    if (procs_[runningIdx_].burst <= 0) {
        runningIdx_ = -1;
        rrCounter_ = 0;
    }
}
