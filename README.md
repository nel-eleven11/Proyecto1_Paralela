# Proyecto1_Paralela

---

## Compilar y ejecutar:


Configurar
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER=g++
```

Compilar
```bash
cmake --build build -j
```

Ejecutar
```bash
./build/screensaver --width w --height h --fps_target f --N n
```