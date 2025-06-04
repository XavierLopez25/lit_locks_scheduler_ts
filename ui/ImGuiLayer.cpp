#include "ImGuiLayer.h"
#include "common/SimMode.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stdexcept>
#include <unordered_map>
#include <random>

ImGuiLayer::ImGuiLayer(
    const char* title,
    std::vector<Process>& processes,
    std::vector<Resource>& resources,
    std::vector<Action>& actions,
    int width,
    int height
)
  : windowTitle(title)
  , winW(width)
  , winH(height)
  , processes_(&processes)
  , resources_(&resources)
  , actions_(&actions)
  , engine_(
      processes,      // origProcs
      resources,      // origRes
      actions,        // origActs
      SchedulingAlgo::FIFO,  // algoritmo por defecto
      /*rrQuantum=*/1
    )
{
    init();
    assignPidColors();
}

ImGuiLayer::~ImGuiLayer()
{
    cleanup();
}

void ImGuiLayer::init()
{

    if (!glfwInit())
        throw std::runtime_error("Failed to init GLFW");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    window = glfwCreateWindow(winW, winH, windowTitle, nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void ImGuiLayer::assignPidColors() {
    // Motor RNG con semilla fija para reproducibilidad
    static std::mt19937_64 rng{12345};
    std::uniform_int_distribution<int> dist(50, 230);

    pidColors_.clear();
    for (auto& p : *processes_) {
        const auto& id = p.pid;
        if (pidColors_.count(id) == 0) {
            int r = dist(rng), g = dist(rng), b = dist(rng);
            pidColors_[id] = IM_COL32(r, g, b, 255);
        }
    }
}

void ImGuiLayer::renderLoop()
{
    static double last = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        showDataPanel();

        // ── Selector de modo ─────────────────────────────────────────
        static int mode = 0;
        ImGui::Text("Modo:");
        ImGui::SameLine(); ImGui::RadioButton("Calendarización", &mode, 0);
        ImGui::SameLine(); ImGui::RadioButton("Sincronización",  &mode, 1);
        engine_.setMode(mode == 0 ? SimMode::SCHEDULING : SimMode::SYNCHRONIZATION);

        // ── Controles comunes ─────────────────────────────────────────
        if (ImGui::Button(running_ ? "Pause" : "Start")) running_ = !running_;
        ImGui::SameLine();
        if (ImGui::Button("Step")) {
            engine_.tick();
            if (engine_.isFinished()) running_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            engine_.reset();
            running_ = false;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Speed", &speed_, 0.1f, 10.0f);

        // ── Auto-tick en cualquiera de los modos ──────────────────────
        if (running_ && now - last >= 1.0 / speed_) {
            engine_.tick();
            last = now;
            if (engine_.isFinished()) running_ = false;
        }

        //---- Variables para el panel de métricas de calendarización ----
        static bool selected[5] = { true, false, false, false, false };  // FCFS activo por defecto
        static const char* algoNames[5] = { "FCFS", "SJF", "SRT", "RR", "Priority" };

        if (mode == 0) {
            // —————— PANEL DE CALENDARIZACIÓN ——————
            if (ImGui::CollapsingHeader("Simulación (Calendarización)")) {
                ImGui::Text("Algoritmo de calendarización:");
                // RadioButtons para elegir algoritmo 
                static int algoIdx = 0;
                ImGui::SameLine(); ImGui::RadioButton("FIFO##gantt",     &algoIdx, 0);
                ImGui::SameLine(); ImGui::RadioButton("SJF##gantt",      &algoIdx, 1);
                ImGui::SameLine(); ImGui::RadioButton("SRT##gantt",      &algoIdx, 2);
                ImGui::SameLine(); ImGui::RadioButton("RR##gantt",       &algoIdx, 3);
                ImGui::SameLine(); ImGui::RadioButton("Priority##gantt", &algoIdx, 4);

                if (engine_.getAlgorithm() != static_cast<SchedulingAlgo>(algoIdx)) {
                    engine_.setAlgorithm(static_cast<SchedulingAlgo>(algoIdx));
                    engine_.reset();
                    running_ = false;
                }

                if(algoIdx == static_cast<int>(SchedulingAlgo::RR)) {
                    ImGui::SliderInt("Quantum", &engine_.rrQuantum_, 1, 10);
                }

                ImGui::Text("Ciclo: %d", engine_.currentCycle());

                ImGui::Text("Running PID: %s",
                    engine_.runningIndex() < 0
                        ? "idle"
                        : engine_.procs()[engine_.runningIndex()].pid.c_str()
                );


                ImGui::Text("Ready queue:");
                for (auto idx : engine_.readyQueue()) {
                    ImGui::SameLine();
                    ImGui::Text("%s", engine_.procs()[idx].pid.c_str());
                }

                if (engine_.isFinished()) {
                    float avgWait = engine_.getAverageWaitingTime();
                    ImGui::Separator();
                    ImGui::Text("Resumen de eficiencia:");
                    ImGui::Text("Tiempo promedio de espera: %.2f ciclos", avgWait);
                }
            }
            if (ImGui::CollapsingHeader("Diagrama de Gantt con ciclos y burst")) {
                auto& history = engine_.getExecutionHistory();
                const float boxW    = 30.0f;
                const float boxH    = 25.0f;
                const float spacing = 2.0f;
                const ImU32 colorIdle = IM_COL32(120,120,120,255);

                const float topMargin    = 30;   
                const float bottomMargin = 20;   
                const float totalHeight  = (topMargin + boxH + bottomMargin);  

                ImGui::BeginChild("GanttScroll",
                    ImGui::GetContentRegionAvail(),
                    true,
                    ImGuiWindowFlags_HorizontalScrollbar
                );

                ImVec2 startPos = ImGui::GetCursorScreenPos();
                auto  drawList = ImGui::GetWindowDrawList();

                // Dibuja TODOS los ciclos arriba 
                const float cycleOffsetY = startPos.y - 5;  
                for (int i = 0; i < (int)history.size(); ++i) {
                    std::string num = std::to_string(i);
                    float textW = ImGui::CalcTextSize(num.c_str()).x;
                    float x = startPos.x + i*(boxW+spacing) + (boxW - textW)/2;
                    drawList->AddText({x, cycleOffsetY}, IM_COL32(200,200,200,255), num.c_str());
                }

                //  Dibuja barras y burst acumulado 
                float x = startPos.x;
                float y = startPos.y + 10;     
                int cumulative = 0;
                int segmentStart = 0;
                for (int i = 0; i < (int)history.size(); ++i) {
                    const auto& pid = history[i];
                    ImU32 color = colorIdle;
                    if (pidColors_.count(pid)) {
                        color = pidColors_[pid];
                    }

                    // barra
                    drawList->AddRectFilled({x, y}, {x+boxW, y+boxH}, color);
                    drawList->AddText({x+5,y+5}, IM_COL32(255,255,255,255), pid.c_str());

                    bool isEnd = (i+1 == (int)history.size() || history[i+1] != pid);
                    if (pid != "idle" && isEnd) {
                        int length = i - segmentStart + 1;
                        cumulative += length;      
                        auto txt = std::to_string(cumulative);
                        float tw = ImGui::CalcTextSize(txt.c_str()).x;
                        float tx = x + (boxW - tw)/2;
                        float ty = y + boxH + 2;   // justo debajo
                        drawList->AddText({tx, ty}, IM_COL32(255,255,0,255), txt.c_str());
                        segmentStart = i+1;
                    }

                    x += boxW + spacing;
                }

                // Reserva el espacio para el scroll 
                ImGui::Dummy(ImVec2(
                    history.size() * (boxW+spacing),
                    totalHeight
                ));

                ImGui::EndChild();
            }
            if (ImGui::CollapsingHeader("Resumen de métricas de calendarización")) {
                ImGui::Text("Seleccione los algoritmos a comparar:");
                for (int i = 0; i < 5; ++i) {
                    std::string label = std::string(algoNames[i]) + "##cmp";
                    ImGui::Checkbox(label.c_str(), &selected[i]);
                    if (i < 4) ImGui::SameLine();
                }

                // Slider para configurar Quantum (si se selecciona RR)
                static int quantumForComparison = 1;
                bool rrSelected = selected[static_cast<int>(SchedulingAlgo::RR)];
                if (rrSelected) {
                    ImGui::SliderInt("Quantum (para RR)##cmp", &quantumForComparison, 1, 10);
                }

                static bool showResults = false;
                if (ImGui::Button("Comparar##cmp")) {
                    showResults = true;
                }

                SimulationEngine tempEngine = engine_;  
                
                if (showResults) {
                    ImGui::Separator();
                    ImGui::Text("Resultados (avg waiting time):");

                    for (int i = 0; i < 5; ++i) {
                        if (!selected[i]) continue;

                        // 1. Configurar algoritmo
                        tempEngine.setAlgorithm(static_cast<SchedulingAlgo>(i));

                        // Si es RR, aplicar quantum configurado
                        if (i == static_cast<int>(SchedulingAlgo::RR)) {
                            tempEngine.rrQuantum_ = quantumForComparison;
                        }

                        // 2. Resetear simulación
                        tempEngine.reset();

                        // 3. Ejecutar simulación completa
                        while (!tempEngine.isFinished()) {
                            tempEngine.tick();
                        }

                        // 4. Calcular y mostrar métrica
                        float avg = tempEngine.getAverageWaitingTime();
                        ImGui::BulletText("%s: %.2f ciclos", algoNames[i], avg);
                    }
                }
            } 
        } else {
            // —————— PANEL DE SINCRONIZACIÓN ——————
            if (ImGui::CollapsingHeader("Simulación (Sincronización)")) {
                ImGui::Text("Ciclo: %d", engine_.currentCycle());

                static int syncFilter = 0;
                ImGui::Text("Ver:");
                
                ImGui::SameLine(); ImGui::RadioButton("Mutex",     &syncFilter, 0);
                ImGui::SameLine(); ImGui::RadioButton("Semáforos", &syncFilter, 1);

                ImGui::Separator();
                ImGui::Text("Leyenda:");
                ImGui::SameLine();

                ImDrawList* dl = ImGui::GetWindowDrawList();
                // Tamaño de cada ícono
                const float iconSize = 16.0f;
                // Espacio entre íconos y texto
                const float pad     = 4.0f;

                // — ADQUIRE (triángulo hacia abajo, celeste) —
                ImGui::Text("ADQUIRE"); ImGui::SameLine();
                {
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    ImU32 col = IM_COL32(0,200,255,255);
                    dl->AddTriangleFilled(
                        { p.x,            p.y + iconSize },
                        { p.x + iconSize, p.y + iconSize },
                        { p.x + iconSize*0.5f, p.y },
                        col
                    );
                    ImGui::Dummy({ iconSize + pad, iconSize });
                    ImGui::SameLine();
                }

                // — RELEASE (círculo verde) —
                ImGui::Text("RELEASE"); ImGui::SameLine();
                {
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    ImU32 col = IM_COL32(0,200,0,255);
                    dl->AddCircleFilled(
                        { p.x + iconSize*0.5f, p.y + iconSize*0.5f },
                        iconSize*0.5f, col
                    );
                    ImGui::Dummy({ iconSize + pad, iconSize });
                    ImGui::SameLine();
                }

                // — WAIT (cruz roja) —
                ImGui::Text(" WAIT"); ImGui::SameLine();
                {
                    ImVec2 p1 = ImGui::GetCursorScreenPos();
                    ImU32 col = IM_COL32(255,0,0,255);  // rojo
                    dl->AddLine({ p1.x,           p1.y },
                                { p1.x + iconSize, p1.y + iconSize }, col, 2.0f);
                    dl->AddLine({ p1.x,           p1.y + iconSize },
                                { p1.x + iconSize, p1.y }, col, 2.0f);
                    ImGui::Dummy({ iconSize + pad, iconSize });
                    ImGui::SameLine();
                }

                // — ACCESSED (cuadrado verde) —
                ImGui::Text("ACCESSED"); ImGui::SameLine();
                {
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    dl->AddRectFilled(
                        { p.x, p.y },
                        { p.x + iconSize, p.y + iconSize },
                        IM_COL32(0,200,0,255)
                    );
                    ImGui::Dummy({ iconSize + pad, iconSize });
                    ImGui::SameLine();
                }

                // — SIGNAL (triángulo invertido, amarillo) —
                ImGui::Text(" SIGNAL"); ImGui::SameLine();
                {
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    ImU32 col = IM_COL32(255,200,0,255);  
                    dl->AddTriangleFilled(
                        { p.x,            p.y + iconSize },
                        { p.x + iconSize, p.y + iconSize },
                        { p.x + iconSize*0.5f, p.y },
                        col
                    );
                    ImGui::Dummy({ iconSize + pad, iconSize });
                    ImGui::SameLine();
                }

                // — READ (cuadrado azul) —
                ImGui::Text("READ"); ImGui::SameLine();
                {
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    ImU32 colRead = IM_COL32(0,  0, 255, 255);  // azul puro
                    dl->AddRectFilled(
                        { p.x,         p.y },
                        { p.x + iconSize, p.y + iconSize },
                        colRead
                    );
                    ImGui::Dummy({ iconSize + pad, iconSize });
                    ImGui::SameLine();
                }

                // — WRITE (cuadrado morado claro) —
                ImGui::Text("WRITE"); ImGui::SameLine();
                {
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    ImU32 colWrite = IM_COL32(200, 150, 255, 255);  // morado claro
                    dl->AddRectFilled(
                        { p.x,           p.y },
                        { p.x + iconSize, p.y + iconSize },
                        colWrite
                    );
                    ImGui::Dummy({ iconSize + pad, iconSize });
                    ImGui::SameLine();
                }

                ImGui::Separator();

                // ----------------------------------------
                // 1) Inicio del área scrollable
                // ----------------------------------------
                const float blockW   = 20.0f;
                const float blockH   = 20.0f;
                const float spX      = 2.0f;
                const float spY      = 5.0f;
                float  labelWidth    = 60.0f;  

                ImGui::BeginChild("SyncTimeline", ImVec2(0, 200),
                                true, ImGuiWindowFlags_HorizontalScrollbar);

                // Origen del área de dibujo (esquina superior izquierda del grid)
                ImVec2 origin = ImGui::GetCursorScreenPos();

                // ----------------------------------------
                // 2) DIBUJAR ETIQUETAS DE CICLO (Encabezado)
                // ----------------------------------------
                auto const& log = engine_.getSyncLog();
                int maxCycle = log.empty() ? 0 : log.back().cycle;
                for (int c = 0; c <= maxCycle; ++c) {
                    float x = origin.x + labelWidth + c * (blockW + spX);
                    float y = origin.y;
                    char buf[8];
                    int  tx = std::snprintf(buf, sizeof(buf), "%d", c);
                    ImVec2 tsz = ImGui::CalcTextSize(buf);
                    dl->AddText({ x + (blockW - tsz.x)/2, y }, IM_COL32(200,200,200,255), buf);
                }

                // ----------------------------------------
                // 3) CALCULAR POSICIÓN Y DIBUJAR NOMBRES
                // ----------------------------------------
                // Reserva fila para cada PID en orden de aparición
                std::unordered_map<int, float> rowY;
                float nextY = origin.y + blockH + spY;  
                for (int i = 0; i < (int)processes_->size(); ++i) {
                    rowY[i] = nextY;
                    dl->AddText(
                    { origin.x, nextY + (blockH - ImGui::GetFontSize())*0.5f },
                    IM_COL32(255,255,255,255),
                    engine_.procs()[i].pid.c_str()
                    );
                    nextY += blockH + spY;
                }

                // ----------------------------------------
                // 4) DIBUJAR BLOQUES DE EVENTOS
                // ----------------------------------------
                bool   viewMutex = (syncFilter == 0);

                const ImU32 colLock    = IM_COL32(0,200,255,255);
                const ImU32 colUnlock  = IM_COL32(0,150,0,255);
                const ImU32 colWait    = IM_COL32(200,0,0,255);
                const ImU32 colSignal  = IM_COL32(255,200,0,255);

                const float semRadius = blockW * 0.4f; 
                const float halfSize  = semRadius; 

                for (auto const& e : log) {
                    bool isM = engine_.isMutex(e.res);
                    if (viewMutex != isM) continue;

                    float x = origin.x + labelWidth + e.cycle*(blockW+spX);
                    float y = rowY[e.pidIdx];
                    ImVec2 center = { x+blockW*0.5f, y+blockH*0.5f };

                    if (isM) {
                        // ————— MODO MUTEX —————
                        if (e.action == SyncAction::ADQUIRE) {
                            if (e.result == SyncResult::ACCESSED) {
                                dl->AddTriangleFilled(
                                    { center.x - halfSize, center.y + halfSize },
                                    { center.x + halfSize, center.y + halfSize },
                                    { center.x,           center.y - halfSize },
                                    colLock
                                );
                            } else {
                                ImVec2 a1 = { center.x - halfSize, center.y - halfSize };
                                ImVec2 a2 = { center.x + halfSize, center.y + halfSize };
                                ImVec2 b1 = { center.x - halfSize, center.y + halfSize };
                                ImVec2 b2 = { center.x + halfSize, center.y - halfSize };
                                dl->AddLine(a1, a2, colWait, 2.0f);
                                dl->AddLine(b1, b2, colWait, 2.0f);
                            }
                        } else if (e.action == SyncAction::READ) {
                            ImU32 colReadOk = IM_COL32(0, 0, 255, 255);
                            ImVec2 p0 = { center.x - halfSize, center.y - halfSize };
                            ImVec2 p1 = { center.x + halfSize, center.y + halfSize };
                            dl->AddRectFilled(p0, p1, colReadOk);
                        }
                        else if (e.action == SyncAction::WRITE) {
                            ImU32 colWriteOk = IM_COL32(200, 150, 255, 255);
                            ImVec2 p0 = { center.x - halfSize, center.y - halfSize };
                            ImVec2 p1 = { center.x + halfSize, center.y + halfSize };
                            dl->AddRectFilled(p0, p1, colWriteOk);

                        } else if (e.action == SyncAction::RELEASE) {
                            dl->AddCircleFilled(
                                center,
                                semRadius,  
                                colUnlock
                            );
                        }

                    } else {
                        // ————— MODO SEMÁFORO —————
                        if (e.action == SyncAction::READ) {
                            // Si READ tuvo éxito (ACCESSED), dibujamos cuadrado azul. Si fue WAITING, dibujamos la "X" roja.
                            if (e.result == SyncResult::ACCESSED) {
                                ImU32 colReadOk = IM_COL32(0, 0, 255, 255);          // azul puro
                                ImVec2 p0 = { center.x - halfSize, center.y - halfSize };
                                ImVec2 p1 = { center.x + halfSize, center.y + halfSize };
                                dl->AddRectFilled(p0, p1, colReadOk);
                            } else { // SyncResult::WAITING
                                // Misma "X" roja que usas para WAIT, porque READ bloqueado es una espera
                                ImU32 colWait = IM_COL32(200, 0, 0, 255);
                                ImVec2 a1 = { center.x - halfSize, center.y - halfSize };
                                ImVec2 a2 = { center.x + halfSize, center.y + halfSize };
                                ImVec2 b1 = { center.x - halfSize, center.y + halfSize };
                                ImVec2 b2 = { center.x + halfSize, center.y - halfSize };
                                dl->AddLine(a1, a2, colWait, 2.0f);
                                dl->AddLine(b1, b2, colWait, 2.0f);
                            }

                        } else if (e.action == SyncAction::WRITE) {
                            // Si WRITE tuvo éxito (ACCESSED), dibujamos cuadrado morado claro. Si fue WAITING, usamos misma "X" roja.
                            if (e.result == SyncResult::ACCESSED) {
                                ImU32 colWriteOk = IM_COL32(200, 150, 255, 255);   // morado claro
                                ImVec2 p0 = { center.x - halfSize, center.y - halfSize };
                                ImVec2 p1 = { center.x + halfSize, center.y + halfSize };
                                dl->AddRectFilled(p0, p1, colWriteOk);
                            } else { // SyncResult::WAITING
                                ImU32 colWait = IM_COL32(200, 0, 0, 255);
                                ImVec2 a1 = { center.x - halfSize, center.y - halfSize };
                                ImVec2 a2 = { center.x + halfSize, center.y + halfSize };
                                ImVec2 b1 = { center.x - halfSize, center.y + halfSize };
                                ImVec2 b2 = { center.x + halfSize, center.y - halfSize };
                                dl->AddLine(a1, a2, colWait, 2.0f);
                                dl->AddLine(b1, b2, colWait, 2.0f);
                            }

                        } else if (e.action == SyncAction::WAIT && e.result == SyncResult::WAITING) {
                            // espera de semáforo
                            ImU32 colWait = IM_COL32(200, 0, 0, 255);
                            ImVec2 a1 = { center.x - halfSize, center.y - halfSize };
                            ImVec2 a2 = { center.x + halfSize, center.y + halfSize };
                            ImVec2 b1 = { center.x - halfSize, center.y + halfSize };
                            ImVec2 b2 = { center.x + halfSize, center.y - halfSize };
                            dl->AddLine(a1, a2, colWait, 2.0f);
                            dl->AddLine(b1, b2, colWait, 2.0f);

                        } else if (e.action == SyncAction::SIGNAL) {
                            // señalización de semáforo
                            dl->AddTriangleFilled(
                                { center.x - halfSize, center.y + halfSize },
                                { center.x + halfSize, center.y + halfSize },
                                { center.x,           center.y - halfSize },
                                colSignal
                            );

                        } else if (e.action == SyncAction::WAKE) {
                            // “WAKE” incluye el acceso inmediato al recurso
                            ImVec2 p0 = { center.x - halfSize, center.y - halfSize };
                            ImVec2 p1 = { center.x + halfSize, center.y + halfSize };
                            ImU32 colWake = IM_COL32(0, 200, 0, 255);  // verde para acceso automático
                            dl->AddRectFilled(p0, p1, colWake);
                        }

                    }
                }

                // ----------------------------------------
                // 5) EXPANDIR PARA SCROLL HORIZONTAL
                // ----------------------------------------
                float totalW = labelWidth + (maxCycle+1)*(blockW + spX);
                ImGui::Dummy(ImVec2(totalW, 0.0f));

                ImGui::EndChild();

                if (ImGui::CollapsingHeader("Estado de Recursos")) {
                    // --- MUTEXES ---
                    ImGui::Text("Mutexes:");
                    for (auto const& [name, m] : engine_.getMutexes()) {
                        ImGui::Bullet();
                        if (m.locked) {
                            // Muestra nombre y dueño
                            const std::string& ownerPid = 
                                (m.ownerIdx >= 0 ? engine_.procs()[m.ownerIdx].pid : "??");
                            ImGui::Text("%s: LOCKED por %s", name.c_str(), ownerPid.c_str());

                            // Muestra la cola de espera con los nombres
                            if (!m.waitQueue.empty()) {
                                ImGui::Text("  Cola de espera:");
                                ImGui::SameLine();
                                for (size_t i = 0; i < m.waitQueue.size(); ++i) {
                                    const auto& pid = engine_.procs()[ m.waitQueue[i] ].pid;
                                    ImGui::Text("%s%s", pid.c_str(), 
                                                i+1 < m.waitQueue.size() ? ", " : "");
                                    if (i+1 < m.waitQueue.size())
                                        ImGui::SameLine();
                                }
                            } else {
                                ImGui::Text("  Cola de espera: (vacía)");
                            }
                        } else {
                            ImGui::Text("%s: LIBRE", name.c_str());
                        }

                    }
                    ImGui::Separator();
                    // --- SEMAPHORES ---
                    ImGui::Text("Semáforos:");
                    for (auto const& [name, s] : engine_.getSemaphores()) {
                        ImGui::Bullet();
                        // Muestra el valor actual
                        ImGui::Text("%s: valor = %d", name.c_str(), s.count);

                        // Muestra la cola de espera con los PIDs
                        if (!s.waitQueue.empty()) {
                            ImGui::Text("  Cola de espera:");
                            ImGui::SameLine();
                            for (size_t i = 0; i < s.waitQueue.size(); ++i) {
                                const auto& pid = engine_.procs()[ s.waitQueue[i] ].pid;
                                ImGui::Text("%s%s",
                                            pid.c_str(),
                                            (i + 1 < s.waitQueue.size()) ? ", " : "");
                                if (i + 1 < s.waitQueue.size())
                                    ImGui::SameLine();
                            }
                        } else {
                            ImGui::Text("  Cola de espera: (vacía)");
                        }
                    }

                }
            }
        }

        // 3) Renderizar
        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
}

void ImGuiLayer::showDataPanel() {
    ImGui::Begin("Data Viewer");

    // Lista de procesos
    if (ImGui::CollapsingHeader("Processes")) {
        for (const auto& p : *processes_) {
            ImGui::BulletText(
              "%s: burst=%d, arrival=%d, priority=%d",
              p.pid.c_str(), p.burst, p.arrival, p.priority
            );
        }
    }

    // Lista de recursos
    if (ImGui::CollapsingHeader("Resources")) {
        for (const auto& r : *resources_) {
            ImGui::BulletText(
              "%s: count=%d",
              r.name.c_str(), r.count
            );
        }
    }

    // Lista de acciones
    if (ImGui::CollapsingHeader("Actions")) {
        for (const auto& a : *actions_) {
            ImGui::BulletText(
              "%s: %s %s @ cycle %d",
              a.pid.c_str(),
              a.type.c_str(),
              a.res.c_str(),
              a.cycle
            );
        }
    }

    ImGui::End();
}

void ImGuiLayer::run()
{
    renderLoop();
}

void ImGuiLayer::cleanup()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}