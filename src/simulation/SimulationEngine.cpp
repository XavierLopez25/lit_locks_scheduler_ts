#include "SimulationEngine.h"
#include "SyncPrimitives/SyncPrimitives.h"
#include "Process.h"      
#include "Resource.h"
#include "common/SimMode.h"
#include "Action.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>
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

    maxSyncCycle_ = 0;
    for (auto &a : origActs_) {
        if (a.cycle > maxSyncCycle_)
            maxSyncCycle_ = a.cycle;
    }

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

    if (mode_ == SimMode::SYNCHRONIZATION && cycle_ >= maxSyncCycle_) {
        return;
    }
    
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
        if (act.cycle != cycle_) 
            continue;

        int idx = findProcessIndex(act.pid);
        if (idx < 0) 
            continue;

        Process& p = procs_[idx];

        if (act.type == "SIGNAL" && p.state == ProcState::BLOCKED) {
            // si sigue bloqueado, no puede ejecutar SIGNAL
            continue;
        }

        // Helper para registrar evento
        auto logEvent = [&](SyncResult r, SyncAction a){
            syncLog_.push_back({ cycle_, idx, act.res, r, a });
        };

        auto logEventAt = [&](int evCycle, SyncResult r, SyncAction a){
            syncLog_.push_back({ evCycle,    
                                    idx,
                                    act.res,
                                    r,
                                    a 
                                });
        };
        // —— SEMÁFORO: READ / WRITE ——
        if (act.type == "READ" || act.type == "WRITE") {
            auto& s = sync_.semaphores[act.res];
            if (s.count > 0) {
                s.count--;
                logEvent(SyncResult::ACCESSED, SyncAction::READ);
            } else {
                p.state = ProcState::BLOCKED;
                s.waitQueue.push_back(idx);
                logEvent(SyncResult::WAITING, SyncAction::READ);
            }

        } else if (act.type == "LOCK") {

            auto &m = sync_.mutexes[act.res];

            // Ya es el dueño → error
            if (m.ownerIdx == idx) {
                std::cerr << "[Error] Proceso " << idx << " ya es dueño del mutex " << act.res
                        << " y volvió a hacer LOCK." << std::endl;
                continue;
            }

            // Acaba de recibir el mutex por UNLOCK → no debe volver a hacer LOCK
            if (procs_[idx].justGrantedMutex) {
                std::cerr << "[Error] Proceso " << idx << " ya recibió el mutex automáticamente en "
                        << "el ciclo anterior, no debe volver a pedir LOCK." << std::endl;
                continue;
            }

            // intento atómico de adquirir
            if (!m.locked) {
                // si estaba libre, me lo quedo
                m.locked   = true;
                m.ownerIdx = idx;
                logEventAt(cycle_, SyncResult::ACCESSED, SyncAction::LOCK);
            } else {
                // si estaba ocupado, me bloqueo hasta un unlock futuro
                p.state = ProcState::BLOCKED;
                m.waitQueue.push_back(idx);
                logEventAt(cycle_, SyncResult::WAITING, SyncAction::LOCK);
            }

        } else if (act.type == "UNLOCK") {
            auto &m = sync_.mutexes[act.res];
            // sólo el dueño puede hacer unlock
            if (m.ownerIdx != idx) 
                continue;

            // 1) dibujo el UNLOCK del dueño actual
            logEventAt(cycle_, SyncResult::ACCESSED, SyncAction::UNLOCK);

            // 2) si hay alguien en la cola, cédelo inmediatamente
            if (!m.waitQueue.empty()) {
                int next = m.waitQueue.front();
                m.waitQueue.pop_front();

                // reasignar dueño sin liberar el mutex
                m.ownerIdx = next;
                // (m.locked ya está true)
                procs_[next].justGrantedMutex = true;

                // despierta a next
                procs_[next].state = ProcState::READY;
                readyQueue_.push_back(next);

                // 3) dibujo el LOCK automático para el despertado
                syncLog_.push_back({
                    cycle_,            // mismo ciclo
                    next,              // nuevo dueño
                    act.res,
                    SyncResult::ACCESSED,
                    SyncAction::LOCK
                });
                procs_[next].justGrantedMutex = false;
            } else {
                // 4) si no hay nadie esperando, libero completamente
                m.locked   = false;
                m.ownerIdx = -1;
            }
        } else if (act.type == "WAIT") {
            auto &s = sync_.semaphores[act.res];
            if (s.count > 0) {
                // adquisición atómica
                s.count--;
                logEventAt(cycle_, SyncResult::ACCESSED, SyncAction::WAIT);
            } else {
                // bloqueo
                p.state = ProcState::BLOCKED;
                s.waitQueue.push_back(idx);
                logEventAt(cycle_, SyncResult::WAITING, SyncAction::WAIT);
            }

        } else if (act.type == "SIGNAL") {
            auto &s = sync_.semaphores[act.res];
            // 1) dibujo la señalización del que libera
            logEventAt(cycle_, SyncResult::ACCESSED, SyncAction::SIGNAL);

            if (!s.waitQueue.empty()) {
                // 2) despierto exactamente a uno y le concedo el recurso
                int next = s.waitQueue.front();
                s.waitQueue.pop_front();

                procs_[next].state = ProcState::READY;
                readyQueue_.push_back(next);

                // el wake incluye el acceso para next:
                syncLog_.push_back({
                    cycle_,
                    next,
                    act.res,
                    SyncResult::ACCESSED,
                    SyncAction::WAKE
                });
            } else {
                // 3) si nadie esperaba, incremento el contador
                s.count++;
            }
        }
    }

    // —— Finalmente, ordenar para el rendering —— 
    std::sort(syncLog_.begin(), syncLog_.end(),
        [](auto const &a, auto const &b){
            if (a.cycle != b.cycle) return a.cycle < b.cycle;
            return a.pidIdx < b.pidIdx;
        }
    );
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
