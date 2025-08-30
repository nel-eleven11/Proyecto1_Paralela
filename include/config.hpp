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
    float slope  = 6.0f;    // normales
    bool  fpslog = false;   // FPS en consola
    int   palette = 2;      // 0=aqua, 1=mix, 2=real (defecto)
    bool  novsync = false;  // medir cómputo puro
    bool  profile = false;  // tiempos sim/render
    
    // ---- Spawn control ----
    float spawn_rate = 1.0f;  // multiplier for drop lifespan (higher = slower spawn)

    // ---- Tinta/mezcla ----
    bool  ink_enabled   = true;   // habilitar tinte multicolor
    float ink_gain      = 0.55f;  // inyección por gota (0..1 aprox)
    float ink_decay     = 0.5f;   // s^-1 (mezcla/atenuación por tiempo)
    float ink_blur_mix  = 0.10f;  // [0..1] mezcla con blur 3x3 por frame
    float ink_strength  = 0.85f;  // [0..1] cuánto tiñe el agua visualmente
};

inline void print_usage(const char* prog) {
    std::cout << "Uso: " << prog
              << " --width W --height H --N N [--seed S] [--slope K] [--spawn-rate R]"
                 " [--fpslog] [--palette {aqua|mix|real}] [--novsync] [--profile]"
                 " [--ink {0|1}] [--ink-gain G] [--ink-decay L] [--ink-blur B] [--ink-strength S]\n";
}

inline bool parse_int(const char* s, int& out, int minv, int maxv) {
    try { long v = std::stol(s); if (v<minv || v>maxv) return false; out=int(v); return true; }
    catch (...) { return false; }
}
inline bool parse_float(const char* s, float& out, float minv, float maxv) {
    try { float v = std::stof(s); if (v<minv || v>maxv) return false; out=v; return true; }
    catch (...) { return false; }
}

inline AppConfig parse_args(int argc, char** argv) {
    AppConfig cfg; bool gotW=false, gotH=false, gotN=false;
    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* name){ if (i+1>=argc) throw std::runtime_error(std::string("Falta valor para ")+name); return argv[++i]; };

        if (a=="--width"||a=="-w"){ const char* v=need(a.c_str()); if(!parse_int(v,cfg.width,640,16384))  throw std::runtime_error("width invalido (>=640)"); gotW=true; }
        else if (a=="--height"||a=="-h"){ const char* v=need(a.c_str()); if(!parse_int(v,cfg.height,480,16384)) throw std::runtime_error("height invalido (>=480)"); gotH=true; }
        else if (a=="--N"||a=="-n"){ const char* v=need(a.c_str()); if(!parse_int(v,cfg.N,1,std::numeric_limits<int>::max())) throw std::runtime_error("N invalido (>=1)"); gotN=true; }
        else if (a=="--seed"){ const char* v=need(a.c_str()); int tmp; if(!parse_int(v,tmp,-1,std::numeric_limits<int>::max())) throw std::runtime_error("seed invalida"); cfg.seed=tmp; }
        else if (a=="--slope"){ const char* v=need(a.c_str()); float tmp; if(!parse_float(v,tmp,0.1f,40.0f)) throw std::runtime_error("slope invalida (0.1..40)"); cfg.slope=tmp; }
        else if (a=="--spawn-rate"){ const char* v=need(a.c_str()); float tmp; if(!parse_float(v,tmp,0.1f,10.0f)) throw std::runtime_error("spawn-rate invalida (0.1..10)"); cfg.spawn_rate=tmp; }
        else if (a=="--fpslog"){ cfg.fpslog=true; }
        else if (a=="--palette"){ const char* v=need(a.c_str()); std::string s=v; if(s=="aqua") cfg.palette=0; else if(s=="mix"||s=="aquamix") cfg.palette=1; else if(s=="real") cfg.palette=2; else throw std::runtime_error("palette invalida (aqua|mix|real)"); }
        else if (a=="--novsync"){ cfg.novsync=true; }
        else if (a=="--profile"){ cfg.profile=true; }
        else if (a=="--ink"){ const char* v=need(a.c_str()); int tmp; if(!parse_int(v,tmp,0,1)) throw std::runtime_error("ink debe ser 0|1"); cfg.ink_enabled=(tmp!=0); }
        else if (a=="--ink-gain"){ const char* v=need(a.c_str()); float tmp; if(!parse_float(v,tmp,0.0f,3.0f)) throw std::runtime_error("ink-gain 0..3"); cfg.ink_gain=tmp; }
        else if (a=="--ink-decay"){ const char* v=need(a.c_str()); float tmp; if(!parse_float(v,tmp,0.0f,5.0f)) throw std::runtime_error("ink-decay 0..5"); cfg.ink_decay=tmp; }
        else if (a=="--ink-blur"){ const char* v=need(a.c_str()); float tmp; if(!parse_float(v,tmp,0.0f,1.0f)) throw std::runtime_error("ink-blur 0..1"); cfg.ink_blur_mix=tmp; }
        else if (a=="--ink-strength"){ const char* v=need(a.c_str()); float tmp; if(!parse_float(v,tmp,0.0f,2.0f)) throw std::runtime_error("ink-strength 0..2"); cfg.ink_strength=tmp; }
        else if (a=="--help"||a=="-?"){ print_usage(argv[0]); std::exit(0); }
        else { std::ostringstream oss; oss<<"Argumento desconocido: "<<a; throw std::runtime_error(oss.str()); }
    }
    if (!gotW || !gotH || !gotN) throw std::runtime_error("Parametros requeridos: --width, --height, --N");
    return cfg;
}
