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

if (POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
endif()

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

# ---------------------------------------------------
# 1) Ejecutable principal: lit_locks_scheduler_ts
# ---------------------------------------------------
set(APP_SRC
  src/main.cpp
  src/Parser.cpp
  ui/ImGuiLayer.cpp
  src/simulation/SimulationEngine.cpp  
)

set(IMGUI_SRC
  external/imgui/imgui.cpp
  external/imgui/imgui_draw.cpp
  external/imgui/imgui_widgets.cpp
  external/imgui/imgui_tables.cpp
  external/imgui/imgui_demo.cpp
)

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
    ${OPENGL_LIBRARIES}
)

# Incluir sólo para este target:
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include                        # Process.h, SyncEnums.h
        ${CMAKE_SOURCE_DIR}/src                             # Parser.h, SimulationEngine.h, etc.
        ${CMAKE_SOURCE_DIR}/src/simulation/syncprimitives    # SyncPrimitives.h
        ${CMAKE_SOURCE_DIR}/ui                              # ImGuiLayer.h
        ${GLFW_INCLUDE_DIRS}                                 # GLFW
        ${CMAKE_SOURCE_DIR}/external/imgui
        ${CMAKE_SOURCE_DIR}/external/imgui/backends
)

# ---------------------------------------------------
# 2) Ejecutable de tests: ParserTests
# ---------------------------------------------------
add_executable(ParserTests
  tests/test_parser.cpp
  src/Parser.cpp
)

# Incluir sólo para este target:
target_include_directories(ParserTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include                        # Process.h, SyncEnums.h
        ${CMAKE_SOURCE_DIR}/src/simulation/syncprimitives   # SyncPrimitives.h
        ${CMAKE_SOURCE_DIR}/src                             # Parser.h
)

target_link_libraries(ParserTests PRIVATE
  Catch2::Catch2
)

add_test(NAME ParserTests COMMAND ParserTests)

# ---------------------------------------------------
# 3) Definir el directorio de datos
# ---------------------------------------------------
add_compile_definitions(DATA_DIR="${CMAKE_SOURCE_DIR}/data")