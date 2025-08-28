#include "waves.hpp"
#include <algorithm>

World::World(const AppConfig& c)
: cfg(c), rng(c.seed) {
    drops.resize(cfg.N);
    H.resize(size_t(cfg.width) * size_t(cfg.height), 0.0f);
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

float ripple_contrib(float x, float y, float t, const Drop& d) {
    float tau = t - d.t0;
    if (tau <= 0.0f) return 0.0f;

    float dx = x - d.x, dy = y - d.y;
    float dist = std::sqrt(dx*dx + dy*dy);
    float ring = d.c * tau;

    // cresta principal
    float gauss_main = std::exp(- ( (dist - ring)*(dist - ring) )
                                / (2.0f * d.sigma * d.sigma));
    float damp  = std::exp(- d.alpha * tau);
    float carrier_t = std::cos(2.0f * float(M_PI) * d.f * tau);
    float main = d.A0 * damp * gauss_main * carrier_t;

    // capilares
    float cap = 0.0f;
    {
        float d1 = dist - (ring - d.cap_delta);
        float d2 = dist - (ring + d.cap_delta);
        float g1 = std::exp(-(d1*d1)/(2.0f*d.cap_sigma*d.cap_sigma));
        float g2 = std::exp(-(d2*d2)/(2.0f*d.cap_sigma*d.cap_sigma));
        float damp_c = std::exp(- (d.alpha*1.25f) * tau);
        cap = d.cap_gain * d.A0 * damp_c * (g1 + g2) * 0.5f;
    }

    // splash inicial (breve)
    float splash = 0.0f;
    {
        const float tauSplashMax = 0.25f;
        if (tau <= tauSplashMax) {
            float rho = d.splash_r0;
            float r2  = (dist*dist)/(2.0f*rho*rho);
            float ang = std::atan2(dy, dx);
            float crown = 1.0f + 0.25f * std::cos(d.splash_m * ang + d.splash_phi);
            splash = d.splash_amp * std::exp(- d.splash_decay * tau) * std::exp(-r2) * crown;
        }
    }
    return main + cap + splash;
}
