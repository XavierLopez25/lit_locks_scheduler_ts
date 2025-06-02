# 📘 Simulador2025

Una aplicación interactiva en **C++** con **Dear ImGui** que simula:

- 🧠 Algoritmos de calendarización de procesos: `FIFO`, `SJF`, `SRT`, `Round-Robin`, `Priority`
- 🔒 Mecanismos de sincronización: `mutex`, `semáforos`
- 📂 Lectura desde archivos `.txt` para procesos, recursos y acciones
- 🧪 Tests automatizados con `Catch2`

---

## 📋 Requisitos

- **Sistema operativo:** Linux, WSL (Ubuntu 20.04+), o macOS
- **Compilador:** `GCC ≥ 9` o `Clang ≥ 10`
- **CMake:** `≥ 3.10` (se recomienda 3.13+)
- **Dependencias del sistema:**

```bash
sudo apt install build-essential cmake pkg-config \
libglfw3-dev libglew-dev libglu1-mesa-dev mesa-common-dev \
libx11-dev libxrandr-dev libxi-dev libxinerama-dev libxcursor-dev
```

---

## 🗂️ Estructura del Proyecto

```
.
├── CMakeLists.txt
├── README.md / README.Rmd
├── compile.sh / run.sh / test.sh
├── data/
│   ├── processes.txt
│   ├── resources.txt
│   └── actions.txt
├── external/imgui/
├── include/
├── src/
│   ├── main.cpp
│   ├── Parser.h/.cpp
│   ├── Process.h
│   ├── Resource.h
│   └── Action.h
├── ui/
│   └── ImGuiLayer.h/.cpp
├── tests/
│   └── test_parser.cpp
└── build/
```

---

## 📐 Formato de Archivos `.txt`

### `processes.txt`

```text
<PID>, <BT>, <AT>, <Priority>
P1, 5, 0, 2
P2, 3, 1, 1
```

### `resources.txt`

```text
<RESOURCE_NAME>, <COUNT>
R1, 1
R2, 2
```

### `actions.txt`

```text
<PID>, <ACTION>, <RESOURCE>, <CYCLE>
P1, READ,  R1, 0
P2, WRITE, R2, 1
```

---

## ⚙️ Construcción del Proyecto

```bash
git clone https://github.com/XavierLopez25/lit_locks_scheduler_ts.git
cd lit_locks_scheduler_ts

git submodule add https://github.com/ocornut/imgui.git external/imgui
git submodule update --init --recursive

mkdir -p build && cd build
cmake ..
make -j
```

> Nota: `DATA_DIR` se configura automáticamente a la carpeta `data/` del repositorio.

---

## ▶️ Ejecución

### Con interfaz gráfica:

```bash
chmod +x compile.sh run.sh test.sh
./compile.sh
./run.sh
```

### Archivos cargados automáticamente:

- `data/processes.txt`
- `data/resources.txt`
- `data/actions.txt`

---

## ✅ Ejecutar Tests

```bash
./test.sh
```

Usa `Catch2` para validar el parser con archivos reales. El suite se encuentra en `tests/test_parser.cpp`.

---

## 🔚 Notas Finales

- Puedes modificar `main.cpp` para aceptar rutas de archivo como argumentos.
- El diseño está pensado para ser fácilmente extensible: puedes añadir nuevos algoritmos, acciones o tipos de recursos sin romper la arquitectura actual.
