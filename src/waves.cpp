#include "waves.hpp"
#include <algorithm>
#include <cmath>

// Color cíclico para gotas: rojo, amarillo, verde, naranja
static void pick_cycle_color(int idx, float& r, float& g, float& b) {
    switch (idx % 4) {
        case 0: r=1.00f; g=0.12f; b=0.10f; break; // rojo
        case 1: r=1.00f; g=0.92f; b=0.12f; break; // amarillo
        case 2: r=0.20f; g=0.90f; b=0.30f; break; // verde
        default:r=1.00f; g=0.55f; b=0.12f; break; // naranja
    }
}

// Pequeño hash 2D para "micro-jitter" del radio (romper anillos perfectos)
static inline float hash2(int x, int y){
    uint32_t h = uint32_t(x)*374761393u + uint32_t(y)*668265263u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return float((h ^ (h >> 16u)) & 0x00FFFFFFu) / float(0x01000000); // [0,1)
}

World::World(const AppConfig& c)
: cfg(c), rng(c.seed) {
    drops.resize(cfg.N);
    size_t SZ = size_t(cfg.width) * size_t(cfg.height);
    H .assign(SZ, 0.0f);
    CR.assign(SZ, 0.0f);
    CG.assign(SZ, 0.0f);
    CB.assign(SZ, 0.0f);
}

void World::respawn_drop(Drop& d, float now_s) {
    d.x = rng.rb(0.0f, float(cfg.width));
    d.y = rng.rb(0.0f, float(cfg.height));
    d.t0 = now_s - rng.rb(0.0f, 0.25f);

    d.A0    = rng.rb(wp.A0_min, wp.A0_max);
    d.alpha = rng.rb(wp.alpha_min, wp.alpha_max);
    d.sigma = rng.rb(wp.sigma_min, wp.sigma_max);
    d.f     = rng.rb(wp.f_min, wp.f_max);
    d.c     = rng.rb(wp.c_min, wp.c_max);
    d.maxLife = rng.rb(wp.life_min, wp.life_max);

    d.cap_delta = rng.rb(wp.cap_delta_min, wp.cap_delta_max);
    d.cap_sigma = rng.rb(wp.cap_sigma_min, wp.cap_sigma_max);
    d.cap_gain  = rng.rb(wp.cap_gain_min,  wp.cap_gain_max);

    d.splash_amp   = rng.rb(wp.splash_amp_min, wp.splash_amp_max) * d.A0;
    d.splash_decay = rng.rb(wp.splash_decay_min, wp.splash_decay_max);
    d.splash_r0    = rng.rb(wp.splash_r0_min, wp.splash_r0_max);
    d.splash_m     = rng.rbi(wp.splash_m_min, wp.splash_m_max);
    d.splash_phi   = rng.rb(0.0f, float(2*M_PI));

    pick_cycle_color(nextColorIdx++, d.col_r, d.col_g, d.col_b);
}

void World::init(float now_s) {
    for (auto& d : drops) respawn_drop(d, now_s);
}
void World::maybe_respawn(float now_s) {
    for (auto& d : drops) {
        float age = now_s - d.t0;
        if (age > d.maxLife) respawn_drop(d, now_s);
    }
}

// ---- Perfil realista: "derivada de Gauss" en el anillo ----
// h(r) ~ -( (r - ct)/sigma ) * exp(-((r - ct)^2) / (2 sigma^2))
// => una cresta con valle pegado, sin "espirales finas".
float ripple_contrib(float x, float y, float t, const Drop& d) {
    float tau = t - d.t0;
    if (tau <= 0.0f) return 0.0f;

    float dx = x - d.x, dy = y - d.y;
    float dist2 = dx*dx + dy*dy;
    float dist = std::sqrt(dist2);
    float ring = d.c * tau;

    // Micro-jitter muy leve al radio (rompe simetría perfecta)
    dist += (hash2(int(std::floor(x)), int(std::floor(y))) - 0.5f) * 0.35f;

    float s     = (dist - ring) / std::max(1e-3f, d.sigma);
    float env   = std::exp(-0.5f * s*s);
    float dgauss= -s * env;                  // derivada de gauss
    float att   = 1.0f / std::sqrt(1.0f + 0.015f * dist);
    float damp  = std::exp(- d.alpha * tau);
    float main  = d.A0 * damp * dgauss * att;

    // Capilares: dos lóbulos finos (también derivada de gauss)
    float cap = 0.0f;
    {
        float s1 = (dist - (ring - d.cap_delta)) / std::max(1e-3f, d.cap_sigma);
        float s2 = (dist - (ring + d.cap_delta)) / std::max(1e-3f, d.cap_sigma);
        float g1 = -s1 * std::exp(-0.5f*s1*s1);
        float g2 = -s2 * std::exp(-0.5f*s2*s2);
        float damp_c = std::exp(- (d.alpha*1.25f) * tau);
        cap = d.cap_gain * d.A0 * damp_c * 0.5f * (g1 + g2) * att;
    }

    // Splash inicial (corona breve)
    float splash = 0.0f;
    {
        const float tauSplashMax = 0.25f;
        if (tau <= tauSplashMax) {
            float rho = d.splash_r0;
            float r2  = (dist2)/(2.0f*rho*rho);
            float ang = std::atan2(dy, dx);
            float crown = 1.0f + 0.25f * std::cos(d.splash_m * ang + d.splash_phi);
            splash = d.splash_amp * std::exp(- d.splash_decay * tau) * std::exp(-r2) * crown;
        }
    }
    return main + cap + splash;
}
