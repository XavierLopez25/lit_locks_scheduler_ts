#!/usr/bin/env bash
set -euo pipefail

# Desde la raíz, entra a build y ejecuta la app
cd build
./lit_locks_scheduler_ts

# Si prefieres pasar rutas manualmente:
# ./lit_locks_scheduler_ts ../data/processes.txt ../data/resources.txt ../data/actions.txt
