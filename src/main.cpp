#include "Parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <vector>
#include "ImGuiLayer.h"
#ifndef DATA_DIR
#define DATA_DIR "./data"
#endif


int main(int argc, char** argv) {
    try {
        
        std::string data_dir = DATA_DIR;

        auto processes = loadProcesses(data_dir + "/processes.txt");
        auto resources = loadResources(data_dir + "/resources.txt");
        auto actions   = loadActions(data_dir + "/actions.txt");

        ImGuiLayer app (

            "Simulador 2025",
            processes,
            resources,
            actions

        );

        app.run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
