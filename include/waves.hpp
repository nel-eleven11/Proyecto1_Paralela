#pragma once
#include <vector>
#include <cmath>
#include "config.hpp"
#include "rng.hpp"

// ------------------- Modelo físico -------------------
struct Drop {
    float x, y;   // posicion (px)
    float t0;     // tiempo de impacto (s)

    float A0;     // amplitud base
    float alpha;  // disipación temporal (s^-1)
    float sigma;  // ancho de cresta (px)
    float f;      // frecuencia portadora (Hz)
    float c;      // velocidad expansión (px/s)
    float maxLife;// vida maxima (s)

    // ripples capilares
    float cap_delta;
    float cap_sigma;
    float cap_gain;

    // splash inicial
    float splash_amp;
    float splash_decay;
    float splash_r0;
    int   splash_m;
    float splash_phi;

    // color de la gota (tinta 0..1 por canal)
    float col_r, col_g, col_b;
};

struct WaveParams {
    float A0_min   = 0.6f,  A0_max   = 1.1f;
    float alpha_min= 0.6f,  alpha_max= 1.2f;
    float sigma_min= 2.5f,  sigma_max= 6.5f;
    float f_min    = 2.0f,  f_max    = 4.0f;
    float c_min    = 90.0f, c_max    = 140.0f;
    float life_min = 2.0f,  life_max = 4.0f;

    float cap_delta_min = 2.0f, cap_delta_max = 5.0f;
    float cap_sigma_min = 1.0f, cap_sigma_max = 2.5f;
    float cap_gain_min  = 0.20f, cap_gain_max  = 0.40f;

    float splash_amp_min   = 0.10f, splash_amp_max   = 0.35f;
    float splash_decay_min = 6.0f,  splash_decay_max = 10.0f;
    float splash_r0_min    = 3.0f,  splash_r0_max    = 7.0f;
    int   splash_m_min     = 6,     splash_m_max     = 10;
};

struct World {
    AppConfig cfg;
    WaveParams wp;
    RNG rng;
    std::vector<Drop> drops;
    std::vector<float> H;   // heightfield
    std::vector<float> CR;  // tinta R
    std::vector<float> CG;  // tinta G
    std::vector<float> CB;  // tinta B

    int nextColorIdx = 0;   // para ciclar colores de gotas

    World(const AppConfig& c);

    void respawn_drop(Drop& d, float now_s);
    void init(float now_s);
    void maybe_respawn(float now_s);
};

// contribución de onda realista (main + capilares + splash)
float ripple_contrib(float x, float y, float t, const Drop& d);
