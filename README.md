# Simulador2025

Una aplicación en C++ con Dear ImGui que simula:

- Algoritmos de calendarización de procesos (FIFO, SJF, SRT, Round-Robin, Priority)  
- Mecanismos de sincronización (mutex, semáforos)  

Además incluye un parser genérico para cargar desde archivos `.txt` los procesos, recursos y acciones, y un suite de tests con Catch2 para validar el parser.

---

## 📋 Requisitos

- **Sistema**: WSL (Ubuntu 20.04+), Linux o macOS  
- **Compilador**: GCC ≥ 9 / Clang ≥ 10  
- **CMake**: ≥ 3.10 (recomendado 3.13+)  
- **Paquetes del sistema** (instálalos en WSL con `sudo apt install`):  
  ```bash
  build-essential cmake pkg-config \
  libglfw3-dev libglew-dev libglu1-mesa-dev mesa-common-dev \
  libx11-dev libxrandr-dev libxi-dev libxinerama-dev libxcursor-dev
  ```

## 🗂️ Estructura de carpetas

```
.
├── CMakeLists.txt           # Configuración general + tests + FetchContent(Catch2)
├── README.md
├── data/                    # Archivos de ejemplo (.txt)
│   ├── processes.txt
│   ├── resources.txt
│   └── actions.txt
├── external/imgui/          # Dear ImGui (submódulo Git)
├── include/                 # (opcional) headers comunes
├── src/
│   ├── main.cpp             # Punto de entrada: carga datos y lanza UI
│   ├── Parser.h/.cpp        # Parser de procesos, recursos y acciones
│   ├── Process.h
│   ├── Resource.h
│   └── Action.h
├── ui/
│   ├── ImGuiLayer.h/.cpp    # Inicialización y bucle de Dear ImGui + GLFW/OpenGL3
├── tests/
│   └── test_parser.cpp      # Suite de tests Catch2 para el parser
└── build/                   # Directorio de compilación (se crea tras cmake)
```

---

## 📐 Formato de los archivos `data/*.txt`

* **processes.txt**
  Cada línea: `<PID>, <BT>, <AT>, <Priority>`

  ```text
  P1, 5, 0, 2
  P2, 3, 1, 1
  ```
* **resources.txt**
  Cada línea: `<RESOURCE_NAME>, <COUNT>`

  ```text
  R1, 1
  R2, 2
  ```
* **actions.txt**
  Cada línea: `<PID>, <ACTION>, <RESOURCE>, <CYCLE>`

  ```text
  P1, READ,  R1, 0
  P2, WRITE, R2, 1
  ```

---

## ⚙️ Construir el proyecto

```bash
git clone https://github.com/XavierLopez25/lit_locks_scheduler_ts.git
cd lit_locks_scheduler_ts

# Inicializar ImGui como submódulo
git submodule add https://github.com/ocornut/imgui.git external/imgui
git submodule update --init --recursive

# Crear carpeta de build
mkdir -p build && cd build

# Configurar CMake (define DATA_DIR como ruta absoluta a carpeta data/)
cmake ..

# Compilar ejecutable principal y tests
make -j
```

> **Nota:**
>
> * El proyecto ya usa FetchContent para Catch2 y aplica la política CMP0072 para OpenGL/GLVND.
> * `DATA_DIR` se compila apuntando a `<repo>/data`, de modo que el binario siempre carga desde allí, aun si se ejecuta dentro de `build/`.

---

## ▶️ Ejecutar la aplicación

Primero dale permisos a los scripts de bash:

```bash
chmod +x compile.sh run.sh test.sh
```

Luego compila el proyecto:

```bash
./compile.sh
```

Para correr la app con frontend:

```bash
./run.sh
```

El programa cargará automáticamente:

* `DATA_DIR/processes.txt`
* `DATA_DIR/resources.txt`
* `DATA_DIR/actions.txt`

Si lo prefieres, puedes modificar `main.cpp` para recibir las rutas por argumentos.

---

## ✅ Ejecutar la suite de tests

```bash
./test.sh
```
