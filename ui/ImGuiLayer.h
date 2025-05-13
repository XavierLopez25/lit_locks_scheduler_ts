#pragma once

#include <imgui.h>
#include <GLFW/glfw3.h>

class ImGuiLayer {
public:
    ImGuiLayer(const char* title, int width = 1280, int height = 720);
    ~ImGuiLayer();

    // Arranca el bucle principal de la UI
    void run();

private:
    void init();
    void renderLoop();
    void cleanup();

    GLFWwindow* window = nullptr;
    const char* windowTitle;
    int winW, winH;
};
