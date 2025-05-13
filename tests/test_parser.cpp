#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "Parser.h"
#include "Process.h"
#include "Resource.h"
#include "Action.h"

#include <fstream>

// Función auxiliar para escribir un fichero temporal
static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream ofs(path);
    REQUIRE(ofs.is_open());
    ofs << content;
}

TEST_CASE("loadProcesses parsea correctamente líneas válidas", "[parser]") {
    const std::string fn = "tmp_procs.txt";
    writeFile(fn,
        "P1, 5, 0, 2\n"
        "P2, 10, 3, 1\n"
    );

    auto procs = loadProcesses(fn);
    REQUIRE(procs.size() == 2);

    CHECK(procs[0].pid      == "P1");
    CHECK(procs[0].burst    ==  5);
    CHECK(procs[0].arrival  ==  0);
    CHECK(procs[0].priority ==  2);

    CHECK(procs[1].pid      == "P2");
    CHECK(procs[1].burst    == 10);
    CHECK(procs[1].arrival  ==  3);
    CHECK(procs[1].priority ==  1);
}

TEST_CASE("loadProcesses lanza excepción con formato inválido", "[parser]") {
    const std::string fn = "tmp_bad.txt";
    writeFile(fn, "P1,5,0\n");  // solo 3 campos
    REQUIRE_THROWS_AS(loadProcesses(fn), std::runtime_error);
}

TEST_CASE("loadResources y loadActions", "[parser]") {
    // Recursos
    const std::string rfn = "tmp_res.txt";
    writeFile(rfn, "R1, 2\nR2, 1\n");
    auto res = loadResources(rfn);
    REQUIRE(res.size() == 2);
    CHECK(res[0].name == "R1");
    CHECK(res[0].count == 2);

    // Acciones
    const std::string afn = "tmp_act.txt";
    writeFile(afn,
        "P1, READ, R1, 0\n"
        "P2, WRITE, R2, 5\n"
    );
    auto acts = loadActions(afn);
    REQUIRE(acts.size() == 2);
    CHECK(acts[1].pid   == "P2");
    CHECK(acts[1].type  == "WRITE");
    CHECK(acts[1].res   == "R2");
    CHECK(acts[1].cycle ==  5);
}
