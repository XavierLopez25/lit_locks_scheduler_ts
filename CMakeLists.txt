


cmake_minimum_required(VERSION 3.10)
project(lit_locks_scheduler_ts)
set(CMAKE_CXX_STANDARD 17)


if (POLICY CMP0148)
  cmake_policy(SET CMP0148 OLD)
endif()

include(FetchContent)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.13.10       
)
FetchContent_MakeAvailable(catch2)

enable_testing()


# Para evitar el warning de OpenGL: prefiere GLVND
if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
endif()



find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

# Incluir headers
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/ui
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/simulation
    ${GLFW_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/imgui/backends
)

link_directories(${GLFW_LIBRARY_DIRS})

# Fuentes de la aplicación
set(APP_SRC
  src/main.cpp
  src/Parser.cpp
  ui/ImGuiLayer.cpp
  src/simulation/SimulationEngine.cpp  
)

# Fuentes de ImGui core
set(IMGUI_SRC
  external/imgui/imgui.cpp
  external/imgui/imgui_draw.cpp
  external/imgui/imgui_widgets.cpp
  external/imgui/imgui_tables.cpp
  external/imgui/imgui_demo.cpp
)

# Fuentes de backends (GLFW + OpenGL3)
set(IMGUI_BACKENDS
  external/imgui/backends/imgui_impl_glfw.cpp
  external/imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(${PROJECT_NAME}
  ${APP_SRC}
  ${IMGUI_SRC}
  ${IMGUI_BACKENDS}
)

target_link_libraries(${PROJECT_NAME}
    ${GLFW_LIBRARIES}
    ${OPENGL_LIBRARIES}      # ← la variable que agrupa todos los libs de OpenGL
)

# 1) Crear ejecutable de tests
add_executable(ParserTests
  tests/test_parser.cpp
  src/Parser.cpp           # definiciones reales del parser
)

# 2) Incluir cabeceras
target_include_directories(ParserTests PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include
)

# 3) Linkear Catch2
target_link_libraries(ParserTests PRIVATE
  Catch2::Catch2
)

# 4) Registrar en CTest
add_test(NAME ParserTests COMMAND ParserTests)

# 5) Definir el directorio de datos
add_compile_definitions(DATA_DIR="${CMAKE_SOURCE_DIR}/data")
