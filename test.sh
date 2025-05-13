#!/usr/bin/env bash
set -euo pipefail

# Desde la raíz, entra a build y corre los tests
cd build

# Opción 1: Con CTest
ctest -V

# Opción 2: Directamente
# ./ParserTests
