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
    res_              = origRes_;
    acts_             = origActs_;
    readyQueue_.clear();
    executionHistory_.clear();
    procs_            = origProcs_;

    for (auto& p : procs_) {
        p.remaining = p.burst;          // Tiempo restante de ejecución
        p.completionTime = -1;          // Aún no se ha completado
    }

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
        [](const auto& p){ return p.remaining <= 0; });
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

        // —— LECTURA / ESCRITURA ——
        if (act.type == "READ" || act.type == "WRITE") {
            // Si es semáforo: mantengo la lógica existente
            if (sync_.semaphores.count(act.res) > 0) {
                auto &s = sync_.semaphores[act.res];
                SyncAction accionLog = (act.type == "READ") ? SyncAction::READ : SyncAction::WRITE;
                if (s.count > 0) {
                    s.count--;
                    logEvent(SyncResult::ACCESSED, accionLog);
                } else {
                    p.state = ProcState::BLOCKED;
                    s.waitQueue.push_back({ idx, accionLog }); 
                    logEvent(SyncResult::WAITING, accionLog);
                }
                continue;
            }

            // Si es mutex, verificar que el proceso sea owner y luego loguear
            if (sync_.mutexes.count(act.res) > 0) {
                auto &m = sync_.mutexes[act.res];
                SyncAction accionLog = (act.type == "READ") ? SyncAction::READ : SyncAction::WRITE;

                // Solo el dueño (ownerIdx) puede hacer READ/WRITE
                if (m.ownerIdx == idx) {
                    logEvent(SyncResult::ACCESSED, accionLog);
                } else {
                    std::cerr << "[Error] Proceso " << p.pid
                            << " intenta hacer “" << act.type 
                            << "” sobre mutex “" << act.res 
                            << "” sin haber hecho ADQUIRE.\n";
                }
                continue;
            }

            // Si no se encontró en semáforos ni en mutexes → error
            std::cerr << "[Error] Recurso \"" << act.res
                    << "\" no encontrado en semáforos ni en mutexes.\n";
            return; 
        } else if (act.type == "ADQUIRE") {

            auto &m = sync_.mutexes[act.res];

            // Ya es el dueño → error
            if (m.ownerIdx == idx) {
                std::cerr << "[Error] Proceso " << idx << " ya es dueño del mutex " << act.res
                        << " y volvió a hacer ADQUIRE." << std::endl;
                continue;
            }

            // Acaba de recibir el mutex por RELEASE → no debe volver a hacer ADQUIRE
            if (procs_[idx].justGrantedMutex) {
                std::cerr << "[Error] Proceso " << idx << " ya recibió el mutex automáticamente en "
                        << "el ciclo anterior, no debe volver a pedir ADQUIRE." << std::endl;
                continue;
            }

            // intento atómico de adquirir
            if (!m.locked) {
                // si estaba libre, me lo quedo
                m.locked   = true;
                m.ownerIdx = idx;
                logEventAt(cycle_, SyncResult::ACCESSED, SyncAction::ADQUIRE);
            } else {
                // si estaba ocupado, me bloqueo hasta un RELEASE futuro
                p.state = ProcState::BLOCKED;
                m.waitQueue.push_back(idx);
                logEventAt(cycle_, SyncResult::WAITING, SyncAction::ADQUIRE);
            }

        } else if (act.type == "RELEASE") {
            auto &m = sync_.mutexes[act.res];

            // Verificar que el proceso idx sea el dueño actual:
            if (m.ownerIdx != idx) {
                std::cerr << "[Error] Ciclo " << cycle_ 
                        << ": Proceso " << p.pid
                        << " intenta RELEASE(\"" << act.res 
                        << "\") sin haber hecho ACQUIRE.\n";
                continue;  
            }

            // Si sí es dueño, procedes a liberar:
            logEventAt(cycle_, SyncResult::ACCESSED, SyncAction::RELEASE);
            if (!m.waitQueue.empty()) {
                // Pasas la propiedad al siguiente bloqueado…
                int next = m.waitQueue.front();
                m.waitQueue.pop_front();
                m.ownerIdx = next;
                procs_[next].state = ProcState::READY;
                readyQueue_.push_back(next);
                // Logueas ADQUIRE automático…
                syncLog_.push_back({
                    cycle_, next, act.res,
                    SyncResult::ACCESSED,
                    SyncAction::ADQUIRE
                });
            } else {
                // Si no hay nadie en cola, liberas el mutex por completo:
                m.locked = false;
                m.ownerIdx = -1;
            }
        } else if (act.type == "WAIT") {
            auto &s = sync_.semaphores[act.res];

            SyncAction accionLog = (act.type == "READ") ? SyncAction::READ : SyncAction::WRITE;

            if (s.count > 0) {
                // adquisición atómica
                s.count--;
                logEventAt(cycle_, SyncResult::ACCESSED, SyncAction::WAIT);
                p.acquiredSemaphores.insert(act.res);

            } else {
                // bloqueo
                p.state = ProcState::BLOCKED;
                s.waitQueue.push_back({ idx, accionLog });
                logEventAt(cycle_, SyncResult::WAITING, accionLog);
            }

        } else if (act.type == "SIGNAL") {
            auto &s = sync_.semaphores[act.res];

            // Primero, dibujamos el SIGNAL del proceso que lo llamó
            logEventAt(cycle_, SyncResult::ACCESSED, SyncAction::SIGNAL);

            if (!s.waitQueue.empty()) {
                auto frontPair = s.waitQueue.front();
                s.waitQueue.pop_front();

                int nextIdx = frontPair.first;               
                SyncAction blockedAction = frontPair.second;

                procs_[nextIdx].state = ProcState::READY;
                readyQueue_.push_back(nextIdx);

                syncLog_.push_back({
                    cycle_,
                    nextIdx,
                    act.res,
                    SyncResult::ACCESSED,
                    SyncAction::WAKE
                });

                syncLog_.push_back({
                    cycle_,
                    nextIdx,
                    act.res,
                    SyncResult::ACCESSED,
                    blockedAction
                });

            } else {
                // Si no había nadie esperando, incrementamos el contador como antes
                s.count++;
            }
        }
    }

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
        {
            if (runningIdx_ < 0 && !readyQueue_.empty()) {
                auto it = std::min_element(
                    readyQueue_.begin(), readyQueue_.end(),
                    [&](int a, int b) {
                        bool aReady = procs_[a].arrival <= cycle_;
                        bool bReady = procs_[b].arrival <= cycle_;

                        if (aReady && !bReady) return true;
                        if (!aReady && bReady) return false;
                        if (!aReady && !bReady) return false; // ninguno listo aún

                        return procs_[a].burst < procs_[b].burst;
                    }
                );

                // Si alguno estaba listo
                if (procs_[*it].arrival <= cycle_) {
                    runningIdx_ = *it;
                    readyQueue_.erase(it);
                }
            }
        }
        break;

        // Algoritmo Shortest Remaining Time (SRT)
        case SchedulingAlgo::SRT:
        {
            // Reunir procesos disponibles
            std::vector<int> available;
            for (int i : readyQueue_) {
                if (procs_[i].arrival <= cycle_) {
                    available.push_back(i);
                }
            }

            // Incluir el running actual si sigue vivo y llegó
            if (runningIdx_ >= 0 && procs_[runningIdx_].arrival <= cycle_) {
                available.push_back(runningIdx_);
            }

            if (!available.empty()) {
                // Elegir el que tenga menor remaining time
                auto it = std::min_element(
                    available.begin(), available.end(),
                    [&](int a, int b) {
                        return procs_[a].remaining < procs_[b].remaining;
                    }
                );

                int chosen = *it;

                if (chosen != runningIdx_) {
                    // Preempt → si el actual no es el mismo
                    if (runningIdx_ >= 0)
                        readyQueue_.push_back(runningIdx_);

                    // Remove from readyQueue_
                    auto pos = std::find(readyQueue_.begin(), readyQueue_.end(), chosen);
                    if (pos != readyQueue_.end())
                        readyQueue_.erase(pos);

                    runningIdx_ = chosen;
                }
            } else {
                // Nada listo
                runningIdx_ = -1;
            }
        }
        break;

        //Algoritmo Priority Scheduling
        case SchedulingAlgo::PRIORITY:
        {
            int nextIdx = -1;

            // Buscar el más prioritario entre los disponibles
            for (int i : readyQueue_) {
                if (procs_[i].arrival <= cycle_) { // ← AGREGAR ESTA CONDICIÓN
                    if (nextIdx == -1 || procs_[i].priority < procs_[nextIdx].priority) {
                        nextIdx = i;
                    }
                }
            }

            // Ver si hay uno más prioritario que el actual
            if (nextIdx >= 0 &&
                (runningIdx_ < 0 || procs_[nextIdx].priority < procs_[runningIdx_].priority))
            {
                if (runningIdx_ >= 0)
                    readyQueue_.push_back(runningIdx_);

                runningIdx_ = nextIdx;
                auto pos = std::find(readyQueue_.begin(), readyQueue_.end(), nextIdx);
                if (pos != readyQueue_.end())
                    readyQueue_.erase(pos);
            }

            // Si no hay nadie más prioritario, continuar el actual
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
        if (firstAppearance.count(proc.pid) && proc.completionTime != -1) {
            int wait = proc.completionTime - proc.arrival - proc.burst;
            total += wait;
            count++;
        }
    }

    return count == 0 ? 0.0f : total / count;
}

void SimulationEngine::executeRunning() {
    if (runningIdx_ < 0) return;

    auto& p = procs_[runningIdx_];
    p.remaining--;

    if (algo_ == SchedulingAlgo::RR)
        rrCounter_++;

    if (p.remaining <= 0 && p.completionTime == -1) {
        p.completionTime = cycle_ + 1;
        runningIdx_ = -1;
        rrCounter_ = 0;
    }
}
