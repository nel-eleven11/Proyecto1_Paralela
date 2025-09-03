# 🌊 Rain Ripples — Screensaver (Sequential Base, OpenMP‑Ready)

> **Proyecto 1 – Computación Paralela y Distribuida**  
> Simulador/”screensaver” de ondas por gotas de lluvia con **modelo físico simple**, **sombreado realista** y **FPS en vivo**.  
> Incluye **versión secuencial** y **versión paralela (OpenMP)**.

---

## 👥 Créditos

- **Autoría (equipo)**: 
  - Nelson García
  - Gabriel Paz
  - Joaquín Puente
- **Tecnologías**: C++17, SDL2, CMake.  


---

## ✨ Características

- **Ondas de lluvia** por *superposición lineal* de contribuciones de cada gota (interferencia constructiva/destructiva).
- **Modelo radial realista**: envolvente gaussiana, portadora radial y amortiguamiento temporal.
- **Detalles visuales**: *ripples capilares* alrededor de la cresta y *splash* inicial (corona breve y sutil).
- **Sombreado “agua”**:
  - Normales del **campo de alturas H(x,y)** por diferencias finitas.
  - Iluminación **Lambert + Blinn-Phong** con realce especular; mezcla tipo Fresnel (Schlick) y gradiente de cielo.
  - Ajustes suaves: absorción por espesor, leve vignette y corrección gamma.
- **FPS en el título** (y opcional por consola).
- **Backends**:
  - `screensaver` → **secuencial**
  - `screensaver_parallel` → **OpenMP**, configurable con hilos.
- **CMake + SDL2** multiplataforma (Linux, Windows, macOS/WSL).

---

## 🗂️ Estructura del repositorio

```
screensaver-openmp/
├─ CMakeLists.txt
├─ include/
│ ├─ config.hpp # AppConfig y helpers de CLI
│ ├─ rng.hpp # RNG reproducible (semilla opcional)
│ ├─ waves.hpp # Drop/WaveParams/World + ripple_contrib()
│ ├─ model.hpp # API para acumular el height field H(x,y)
│ ├─ shading.hpp # Sombreado basado en normales
│ └─ render_sdl.hpp # Helpers SDL (textura/buffer, present)
└─ src/
├─ main.cpp # Loop principal, eventos, FPS y selección de backend
├─ waves.cpp # Respawn de gotas y parámetros físicos/visuales
├─ model_seq.cpp # IMPLEMENTACIÓN SECUENCIAL (acumulación de H)
├─ model_omp.cpp # IMPLEMENTACIÓN PARALELA (OpenMP) de la acumulación
├─ shading.cpp # Cálculo de normales y composición del color
└─ render_sdl.cpp # SDL en hilo principal (ventana/renderer/textura)
```

---

## 🛠️ Dependencias

- **CMake** ≥ 3.20  
- **C++17** (gcc/clang/MSVC)  
- **SDL2** (runtime + dev headers)  
- **OpenMP**  
  - **GCC**: viene con `-fopenmp` (Linux/macOS/MinGW).  
  - **Clang**: instalar `libomp`/`openmp`.  
  - **MSVC** (Visual Studio): habilitar `/openmp`.

### Linux / WSL (Ubuntu recomendado)
```bash
sudo apt update
sudo apt install -y build-essential cmake libsdl2-dev
```

### Arch / EndeavourOS
```bash
sudo pacman -Syu
sudo pacman -S base-devel cmake sdl2
```

### macOS (Homebrew)
```bash
brew install cmake sdl2
```

### Windows (opciones)
- **WSL/Ubuntu** (recomendado para este proyecto) → usa las instrucciones de Linux.
- **MSVC + vcpkg** (alternativa nativa):
  ```bash
  # En una terminal de Developer para VS:
  git clone https://github.com/microsoft/vcpkg
  ./vcpkg/bootstrap-vcpkg.bat
  ./vcpkg/vcpkg install sdl2:x64-windows
  # Luego integra vcpkg con CMake GUI o define toolchain al configurar.
  ```

> **Nota WSL:** si usas WSLg (Windows 11) deberías obtener aceleración decente; con X11/VcXsrv podría ir más lento. El cuello principal de este proyecto está en la *simulación*, no en el *present*.


---

## ⚙️ Compilar

```bash
# desde la raíz del repo
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

> CMake activa optimizaciones (O3/fast‑math cuando aplica). Si cambias código o flags, vuelve a **configurar** y **compilar**.

---

## ▶️ Ejecutar

Secuencial
```bash
./build/screensaver --width 1024 --height 768 --N 8
```

Paralelo
```bash
./build/screensaver_parallel --width 1024 --height 768 --N 8
```

#### Control de hilos (elige uno)
export OMP_NUM_THREADS=8          # Linux/macOS
set OMP_NUM_THREADS=8             # Windows (cmd)
$env:OMP_NUM_THREADS=8            # PowerShell


### Parámetros CLI

| Flag | Descripción | Valores / Default |
|---|---|---|
| `--width, -w` | Ancho de ventana (min 640) | `800` |
| `--height, -h` | Alto de ventana (min 480) | `600` |
| `--N, -n` | Número de gotas activas | `5` |
| `--seed` | Semilla RNG (`-1` = aleatoria) | `-1` |
| `--slope` | Escala de pendiente para cálculo de normales | `6.0` |
| `--palette` | Paleta de color/sombreado | `aqua` \| `mix` \| **`real`** |
| `--fpslog` | Imprime FPS en consola | off |
| `--novsync` | Desactiva vsync (medición de cómputo puro) | off |
| `--profile` | Muestra tiempos `sim` y `shade+present` (ms) | off |

**Ejemplos**

```bash
# Demo realista (por defecto ya es 'real')
./build/screensaver -w 1280 -h 720 -n 10 --fpslog

# Medir cómputo puro y ver tiempos por etapa
./build/screensaver -w 800 -h 600 -n 6 --novsync --profile --fpslog

# Paleta estética “aqua” con superficie más plana
./build/screensaver -w 1024 -h 768 -n 8 --palette aqua --slope 5

# Paralelo con 8 hilos
OMP_NUM_THREADS=8 ./build/screensaver_parallel -w 1024 -h 768 -n 8
```

---

## 🧠 ¿Cómo funciona? (resumen técnico)

### 1) Modelo físico (campo de alturas **H(x,y,t)**)

Cada gota **d** aporta a un píxel **(x,y)** en el tiempo **t**:

- **Anillo principal**: envolvente gaussiana centrada en `dist - c*t`  
  `env = exp(-((dist - ring)^2)/(2*sigma^2))`
- **Portadora radial** (**realismo**):  
  `cos( k*(dist - ring) + φ_t )` con `k ≈ π/σ`
- **Atenuación radial**: `1/sqrt(1 + k_r * dist)`
- **Amortiguamiento temporal**: `exp(-alpha * τ)`, `τ = t - t0`
- **Ripples capilares**: dos gaussianas estrechas alrededor del anillo principal.
- **Splash** inicial: corona angular breve (`<= 0.25s`).

El **height field** es la **suma lineal** de todas las gotas.

> **Optimización (secuencial):** en vez de recorrer toda la imagen para cada gota, solo se procesa una **banda** [r_min,r_max] alrededor del anillo donde el aporte es significativo (*culling por anillo*). Esto reduce drásticamente el trabajo y aumenta FPS.

### 2) Sombreado “agua” (basado en normales)

- Normales `N` a partir de gradientes de **H** (diferencias finitas).
- **Lambert + Blinn‑Phong** (luz rasante desde arriba‑izquierda).
- **Fresnel (Schlick)**: mezcla entre **reflexión** del entorno y **difuso** de agua.  
  `F = F0 + (1-F0)*(1 - N·V)^5`, con `F0 ≈ 0.02` para agua.
- **Entorno**: *skybox procedural* súper barato (gradiente cielo ↔ horizonte).
- **Absorción espectral** por espesor (|H|): `T = e^{ -k_rgb * thickness }`.
- **Gamma correction** y **vignette** suave.

### 3) Paralelización (OpenMP):

- Versión paralela para la acumulación del height field, repartiendo trabajo por píxel o por tiles.
- SDL permanece en el hilo principal (presentación).

---

## 📈 Validación y FPS

- **Requisito**: mínimo **640×480** y **FPS ≥ 30** (ideal 60) en esa resolución.  
- **Indicadores**:  
  - Título de la ventana: `Rain Ripples | WxH | N=n | FPS=xx`
  - Consola (si `--fpslog`): `FPS= ...`
  - Perfilado (si `--profile`): `sim=... ms, shade+present=... ms`

> Si los FPS caen: baja `-n`, baja resolución, o usa `--novsync` para medir. La versión paralela con OpenMP absorberá los casos pesados.

---

## 🧩 Troubleshooting

- **La ventana va a 60 FPS exactos** aunque `--novsync`: revisa que tu backend SDL no fuerce vsync o que tu compositor (Wayland/Xorg) no lo imponga.
- **Renderer “software” en WSL**: usa WSLg (Win11) o ejecuta nativo en Windows (MSVC+vcpkg) para comparar. El coste dominante es la simulación; el dibujo rara vez es el cuello.
- **Pantalla negra**: verifica que `libsdl2-dev` esté instalado y que CMake encontró `SDL2::SDL2`.

---
