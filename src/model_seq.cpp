#include "model.hpp"
#include <algorithm>
#include <cmath>

// Hash 2D igual que en waves.cpp para el jitter
static inline float hash2(int x, int y){
    uint32_t h = uint32_t(x)*374761393u + uint32_t(y)*668265263u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return float((h ^ (h >> 16u)) & 0x00FFFFFFu) / float(0x01000000);
}

void accumulate_heightfield_sequential(
    std::vector<float>& H,
    std::vector<float>& CR,
    std::vector<float>& CG,
    std::vector<float>& CB,
    int W, int Hh,
    const std::vector<Drop>& drops,
    float t_now,
    bool  ink_enabled,
    float ink_gain)
{
    std::fill(H.begin(), H.end(), 0.0f);

    for (const auto& d : drops) {
        float tau = t_now - d.t0;
        if (tau <= 0.0f) continue;

        float ring = d.c * tau;

        // Banda de influencia
        const float band =
            3.0f * d.sigma +
            (d.cap_delta + 3.0f * d.cap_sigma) +
            d.splash_r0;

        float rmin = std::max(0.0f, ring - band);
        float rmax = ring + band;
        float rmin2 = rmin*rmin;
        float rmax2 = rmax*rmax;

        int xmin = std::max(0, int(std::floor(d.x - rmax - 2)));
        int xmax = std::min(W-1, int(std::ceil (d.x + rmax + 2)));
        int ymin = std::max(0, int(std::floor(d.y - rmax - 2)));
        int ymax = std::min(Hh-1, int(std::ceil (d.y + rmax + 2)));

        for (int y=ymin; y<=ymax; ++y) {
            float fy = float(y) + 0.5f;
            float dy = fy - d.y;
            for (int x=xmin; x<=xmax; ++x) {
                float fx = float(x) + 0.5f;
                float dx = fx - d.x;
                float dist2 = dx*dx + dy*dy;
                if (dist2 < rmin2 || dist2 > rmax2) continue;

                float dist = std::sqrt(dist2);

                // micro-jitter al radio
                dist += (hash2(x,y) - 0.5f) * 0.35f;

                // ---- Derivada de Gauss como perfil principal ----
                float s     = (dist - ring) / std::max(1e-3f, d.sigma);
                float env   = std::exp(-0.5f * s*s);
                float dgauss= -s * env;
                float att   = 1.0f / std::sqrt(1.0f + 0.015f * dist);
                float damp  = std::exp(- d.alpha * tau);
                float main  = d.A0 * damp * dgauss * att;

                // ---- Capilares ----
                float cap = 0.0f;
                {
                    float s1 = (dist - (ring - d.cap_delta)) / std::max(1e-3f, d.cap_sigma);
                    float s2 = (dist - (ring + d.cap_delta)) / std::max(1e-3f, d.cap_sigma);
                    float g1 = -s1 * std::exp(-0.5f*s1*s1);
                    float g2 = -s2 * std::exp(-0.5f*s2*s2);
                    float damp_c = std::exp(- (d.alpha*1.25f) * tau);
                    cap = d.cap_gain * d.A0 * damp_c * 0.5f * (g1 + g2) * att;
                }

                // ---- Splash (breve) ----
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

                size_t idx = size_t(y)*size_t(W) + size_t(x);
                H[idx] += (main + cap + splash);

                // ---- Tinta: solo la envolvente (sin oscilación) ----
                if (ink_enabled) {
                    float ink_w = ink_gain * damp *
                                  std::exp(-0.5f * ((dist - ring)*(dist - ring)) /
                                           (std::max(1e-3f, d.sigma)*std::max(1e-3f, d.sigma))) *
                                  att;
                    CR[idx] += ink_w * d.col_r;
                    CG[idx] += ink_w * d.col_g;
                    CB[idx] += ink_w * d.col_b;
                }
            }
        }
    }
}
