#include "model.hpp"
#include <algorithm>
#include <cmath>

void accumulate_heightfield_sequential(
    std::vector<float>& H,
    int W, int Hh,
    const std::vector<Drop>& drops,
    float t_now)
{
    std::fill(H.begin(), H.end(), 0.0f);

    // Recorremos por gota, y para cada una solo pintamos la "banda" donde
    // su gaussiana principal + capilares + splash aportan algo significativo.
    for (const auto& d : drops) {
        float tau = t_now - d.t0;
        if (tau <= 0.0f) continue;

        float ring = d.c * tau;

        // Ancho conservador de la banda de influencia
        const float band =
            3.0f * d.sigma +
            (d.cap_delta + 3.0f * d.cap_sigma) +
            d.splash_r0;

        float rmin = std::max(0.0f, ring - band);
        float rmax = ring + band;
        float rmin2 = rmin*rmin;
        float rmax2 = rmax*rmax;

        // Caja acotada alrededor del anillo
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

                // Fuera de banda: sin costo caro
                if (dist2 < rmin2 || dist2 > rmax2) continue;

                float dist = std::sqrt(dist2);

                // --- Cresta principal ---
                float gauss_main = std::exp(- ( (dist - ring)*(dist - ring) )
                                            / (2.0f * d.sigma * d.sigma));
                float damp  = std::exp(- d.alpha * tau);
                float carrier_t = std::cos(2.0f * float(M_PI) * d.f * tau);
                float main = d.A0 * damp * gauss_main * carrier_t;

                // --- Capilares ---
                float cap = 0.0f;
                {
                    float d1 = dist - (ring - d.cap_delta);
                    float d2 = dist - (ring + d.cap_delta);
                    float g1 = std::exp(-(d1*d1)/(2.0f*d.cap_sigma*d.cap_sigma));
                    float g2 = std::exp(-(d2*d2)/(2.0f*d.cap_sigma*d.cap_sigma));
                    float damp_c = std::exp(- (d.alpha*1.25f) * tau);
                    cap = d.cap_gain * d.A0 * damp_c * (g1 + g2) * 0.5f;
                }

                // --- Splash inicial (muy breve) ---
                float splash = 0.0f;
                {
                    const float tauSplashMax = 0.25f;
                    if (tau <= tauSplashMax) {
                        float rho = d.splash_r0;
                        float r2  = (dist2)/(2.0f*rho*rho); // usar dist2 directo
                        float ang = std::atan2(dy, dx);
                        float crown = 1.0f + 0.25f * std::cos(d.splash_m * ang + d.splash_phi);
                        splash = d.splash_amp * std::exp(- d.splash_decay * tau) * std::exp(-r2) * crown;
                    }
                }

                H[size_t(y)*size_t(W) + size_t(x)] += (main + cap + splash);
            }
        }
    }
}
