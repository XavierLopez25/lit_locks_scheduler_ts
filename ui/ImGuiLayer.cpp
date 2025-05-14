#include "ImGuiLayer.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stdexcept>

ImGuiLayer::ImGuiLayer(
    const char* title,
    std::vector<Process>& processes,
    std::vector<Resource>& resources,
    std::vector<Action>& actions,
    int width,
    int height
) : windowTitle(title)
  , winW(width)
  , winH(height)
  , processes_(&processes)
  , resources_(&resources)
  , actions_(&actions)
{
    init();
}

ImGuiLayer::~ImGuiLayer()
{
    cleanup();
}

void ImGuiLayer::init()
{
    // 1) Inicializa GLFW
    if (!glfwInit())
        throw std::runtime_error("Failed to init GLFW");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    // 2) Crea ventana
    window = glfwCreateWindow(winW, winH, windowTitle, nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // 3) Inicializar ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // 4) Inicializar backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void ImGuiLayer::renderLoop()
{
    while (!glfwWindowShouldClose(window))
    {
        // 1) Eventos
        glfwPollEvents();
        // 2) Nuevo frame ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Simulador2025");
        ImGui::Text("¡Hola, Simulador está corriendo!");
        ImGui::End();

        // panel para ver/lipiar datos
        showDataPanel();

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
