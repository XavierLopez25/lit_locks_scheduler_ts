#include "Parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

// Función genérica para partir una línea por comas y eliminar espacios
static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> elems;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        // recorta espacios al inicio y final
        const auto l = item.find_first_not_of(" \t");
        const auto r = item.find_last_not_of(" \t");
        elems.push_back(item.substr(l, r - l + 1));
    }
    return elems;
}

std::vector<Process> loadProcesses(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("No se pudo abrir " + path);

    std::vector<Process> list;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto tokens = split(line);
        if (tokens.size() != 4)
            throw std::runtime_error("Formato inválido en procesos: " + line);

        Process p;
        p.pid      = tokens[0];
        p.burst    = std::stoi(tokens[1]);
        p.arrival  = std::stoi(tokens[2]);
        p.priority = std::stoi(tokens[3]);
        list.push_back(p);
    }
    return list;
}

std::vector<Resource> loadResources(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("No se pudo abrir " + path);

    std::vector<Resource> list;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto tokens = split(line);
        if (tokens.size() != 2)
            throw std::runtime_error("Formato inválido en recursos: " + line);

        Resource r;
        r.name    = tokens[0];
        r.count   = std::stoi(tokens[1]);
        list.push_back(r);
    }
    return list;
}

std::vector<Action> loadActions(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("No se pudo abrir " + path);

    std::vector<Action> list;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto tokens = split(line);
        if (tokens.size() != 4)
            throw std::runtime_error("Formato inválido en acciones: " + line);

        Action a;
        a.pid    = tokens[0];
        a.type   = tokens[1];    // "READ" o "WRITE"
        a.res    = tokens[2];    // nombre del recurso
        a.cycle  = std::stoi(tokens[3]);
        list.push_back(a);
    }
    return list;
}
