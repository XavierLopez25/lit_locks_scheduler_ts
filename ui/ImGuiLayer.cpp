#include "ImGuiLayer.h"
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
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        showDataPanel();

        if (ImGui::CollapsingHeader("Simulación")) {

            // RadioButtons para elegir algoritmo 
            static int algoIdx = 0;
            ImGui::Text("Algoritmo:");
            ImGui::SameLine(); ImGui::RadioButton("FIFO",     &algoIdx, 0);
            ImGui::SameLine(); ImGui::RadioButton("SJF",      &algoIdx, 1);
            ImGui::SameLine(); ImGui::RadioButton("SRT",      &algoIdx, 2);
            ImGui::SameLine(); ImGui::RadioButton("RR",       &algoIdx, 3);
            ImGui::SameLine(); ImGui::RadioButton("Priority", &algoIdx, 4);

            if (engine_.getAlgorithm() != static_cast<SchedulingAlgo>(algoIdx)) {
                engine_.setAlgorithm(static_cast<SchedulingAlgo>(algoIdx));
                engine_.reset();
                running_ = false;
            }

            if(algoIdx == static_cast<int>(SchedulingAlgo::RR)) {
                ImGui::SliderInt("Quantum", &engine_.rrQuantum_, 1, 10);
            }

            ImGui::Text("Ciclo: %d", engine_.currentCycle());

            static double last = glfwGetTime();
            double now = glfwGetTime();

            if (ImGui::Button(running_ ? "Pause" : "Start")) running_ = !running_;
            ImGui::SameLine();
            if (ImGui::Button("Step")) {
                engine_.tick();
                if (engine_.isFinished()) {
                    running_ = false;
                }
            }

            if (running_ && now - last >= 1.0 / speed_) {
                engine_.tick();
                last = now;
                if (engine_.isFinished()) {
                    running_ = false;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                engine_.reset();
                running_ = false;
            }

            ImGui::SliderFloat("Speed (ticks/s)", &speed_, 0.1f, 10.0f);

            // auto‐tick
            if (running_ && now - last >= 1.0/speed_) {
                engine_.tick();
                last = now;
                if (engine_.isFinished()) running_ = false;
            }

            ImGui::Text("Running PID: %s",
                engine_.runningIndex() < 0
                    ? "idle"
                    : engine_.procs()[engine_.runningIndex()].pid.c_str()
            );

            if (engine_.isFinished()) {
                float avgWait = engine_.getAverageWaitingTime();
                ImGui::Separator();
                ImGui::Text("Resumen de eficiencia:");
                ImGui::Text("Tiempo promedio de espera: %.2f ciclos", avgWait);
            }

            ImGui::Text("Ready queue:");
            for (auto idx : engine_.readyQueue()) {
                ImGui::SameLine();
                ImGui::Text("%s", engine_.procs()[idx].pid.c_str());
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

    // Botón para limpiar todo
    if (ImGui::Button("Clear All Data")) {
        processes_->clear();
        resources_->clear();
        actions_->clear();
    }

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
