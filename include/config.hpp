#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <limits>

struct AppConfig {
    int   width  = 800;     // >= 640
    int   height = 600;     // >= 480
    int   N      = 5;       // gotas activas (>=1)
    int   seed   = -1;      // -1 -> random_device
    float slope  = 6.0f;    // factor de pendiente para shading
    bool  fpslog = false;   // imprimir FPS en consola
    int   palette = 0;      // 0 aqua; 1 aqua-mix
    bool  novsync = false;  // NO usar vsync (medir cómputo puro)
    bool  profile = false;  // imprimir tiempos sim/render (ms)
};

inline void print_usage(const char* prog) {
    std::cout << "Uso: " << prog
              << " --width W --height H --N N [--seed S] [--slope K]"
                 " [--fpslog] [--palette {aqua|mix}] [--novsync] [--profile]\n";
}

inline bool parse_int(const char* s, int& out, int minv, int maxv) {
    try {
        long v = std::stol(s);
        if (v < minv || v > maxv) return false;
        out = static_cast<int>(v);
        return true;
    } catch (...) { return false; }
}

inline bool parse_float(const char* s, float& out, float minv, float maxv) {
    try {
        float v = std::stof(s);
        if (v < minv || v > maxv) return false;
        out = v; return true;
    } catch (...) { return false; }
}

inline AppConfig parse_args(int argc, char** argv) {
    AppConfig cfg;
    bool gotW=false, gotH=false, gotN=false;
    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        auto need_value = [&](const char* name){
            if (i+1 >= argc) throw std::runtime_error(std::string("Falta valor para ")+name);
            return argv[++i];
        };
        if (a=="--width" || a=="-w") {
            const char* v = need_value(a.c_str());
            if (!parse_int(v, cfg.width, 640, 16384))
                throw std::runtime_error("width invalido (>=640)");
            gotW=true;
        } else if (a=="--height" || a=="-h") {
            const char* v = need_value(a.c_str());
            if (!parse_int(v, cfg.height, 480, 16384))
                throw std::runtime_error("height invalido (>=480)");
            gotH=true;
        } else if (a=="--N" || a=="-n") {
            const char* v = need_value(a.c_str());
            if (!parse_int(v, cfg.N, 1, std::numeric_limits<int>::max()))
                throw std::runtime_error("N invalido (>=1)");
            gotN=true;
        } else if (a=="--seed") {
            const char* v = need_value(a.c_str());
            int tmp; if (!parse_int(v, tmp, -1, std::numeric_limits<int>::max()))
                throw std::runtime_error("seed invalida");
            cfg.seed = tmp;
        } else if (a=="--slope") {
            const char* v = need_value(a.c_str());
            float tmp; if (!parse_float(v, tmp, 0.1f, 40.0f))
                throw std::runtime_error("slope invalida (0.1..40)");
            cfg.slope = tmp;
        } else if (a=="--fpslog") {
            cfg.fpslog = true;
        } else if (a=="--palette") {
            const char* v = need_value(a.c_str());
            std::string s = v;
            if (s=="aqua") cfg.palette = 0;
            else if (s=="mix" || s=="aquamix") cfg.palette = 1;
            else throw std::runtime_error("palette invalida (aqua|mix)");
        } else if (a=="--novsync") {
            cfg.novsync = true;
        } else if (a=="--profile") {
            cfg.profile = true;
        } else if (a=="--help" || a=="-?") {
            print_usage(argv[0]); std::exit(0);
        } else {
            std::ostringstream oss; oss<<"Argumento desconocido: "<<a;
            throw std::runtime_error(oss.str());
        }
    }
    if (!gotW || !gotH || !gotN) {
        throw std::runtime_error("Parametros requeridos: --width, --height, --N");
    }
    return cfg;
}
