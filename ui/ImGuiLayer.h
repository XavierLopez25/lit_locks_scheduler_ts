#pragma once
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <unordered_map>
#include <random>
#include "Process.h"
#include "Resource.h"
#include "Action.h"
#include "simulation/SimulationEngine.h" 

class ImGuiLayer {
public:
    // Recibe referencias a los vectores cargados
    ImGuiLayer(
      const char* title,
      std::vector<Process>& processes,
      std::vector<Resource>& resources,
      std::vector<Action>& actions,
      int width = 1280,
      int height = 720
    );
    ~ImGuiLayer();
    void run();

private:
    void init();
    void renderLoop();
    void showDataPanel();      
    void cleanup();
    void assignPidColors();

    // Punteros a los datos que se quiere mostrar o limpiar
    std::vector<Process>*  processes_;
    std::vector<Resource>* resources_;
    std::vector<Action>*   actions_;
    std::unordered_map<std::string, ImU32> pidColors_;

    GLFWwindow* window = nullptr;
    const char* windowTitle;
    int winW, winH;

    // motor de simulación y control
    SimulationEngine engine_;   
    bool            running_ = false;
    float           speed_   = 1.0f;
};
