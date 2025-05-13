#!/usr/bin/env bash
set -euo pipefail

# 1) Crear y entrar al directorio de build
mkdir -p build
cd build

# 2) Configurar CMake y compilar
cmake ..
make -j

echo "✅ Compilación completada."
