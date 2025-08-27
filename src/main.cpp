#include <SDL.h>
#include <cmath>
#include <vector>
#include <random>
#include <string>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <algorithm>

struct AppConfig {
    int width  = 800;
    int height = 600;
    int N      = 5;   // gotas activas
};

static void print_usage(const char* prog) {
    std::cout << "Uso: " << prog << " --width W --height H --N N\n"
              << "  --width/-w   Ancho de ventana (min 640)\n"
              << "  --height/-h  Alto de ventana  (min 480)\n"
              << "  --N/-n       Numero de gotas activas (>=1)\n";
}

static bool parse_int(const char* s, int& out, int minv, int maxv) {
    try {
        long v = std::stol(s);
        if (v < minv || v > maxv) return false;
        out = static_cast<int>(v);
        return true;
    } catch (...) { return false; }
}

static AppConfig parse_args(int argc, char** argv) {
    AppConfig cfg;
    bool gotW=false, gotH=false, gotN=false;
    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        auto need_value = [&](const char* name) {
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

// ===================== FISICA & PARAMS =====================

struct Drop {
    // Posición e impacto
    float x, y;     // px
    float t0;       // s

    // "Fuerza" inicial ~ amplitud base
    float A0;       // amplitud
    float alpha;    // disipación temporal (s^-1)

    // Ondas
    float sigma;    // ancho cresta (px)
    float f;        // frecuencia portadora (Hz)
    float c;        // velocidad expansión (px/s)
    float maxLife;  // vida máx (s) antes de respawn

    // Ripples capilares alrededor de la cresta principal
    float cap_delta;   // desplazamiento de los satélites respecto a la cresta (px)
    float cap_sigma;   // ancho de los satélites (px)
    float cap_gain;    // ganancia relativa

    // Salpicadura (sólo al inicio, muy amortiguada)
    float splash_amp;   // amplitud de splash
    float splash_decay; // disipación del splash (s^-1)
    float splash_r0;    // tamaño base del splash (px)
    int   splash_m;     // número de lóbulos de la corona
    float splash_phi;   // fase angular aleatoria
};

struct WaveParams {
    // Rango de aleatorios para diversidad visual
    float A0_min   = 0.6f,  A0_max   = 1.1f;
    float alpha_min= 0.6f,  alpha_max= 1.2f;
    float sigma_min= 2.5f,  sigma_max= 6.5f;
    float f_min    = 2.0f,  f_max    = 4.0f;
    float c_min    = 90.0f, c_max    = 140.0f;
    float life_min = 2.0f,  life_max = 4.0f;

    // Capilares (pequeñas ondulaciones cercanas a la cresta)
    float cap_delta_min = 2.0f, cap_delta_max = 5.0f;
    float cap_sigma_min = 1.0f, cap_sigma_max = 2.5f;
    float cap_gain_min  = 0.20f, cap_gain_max  = 0.40f;

    // Splash (muy breve y sutil)
    float splash_amp_min   = 0.10f, splash_amp_max   = 0.35f;
    float splash_decay_min = 6.0f,  splash_decay_max = 10.0f;
    float splash_r0_min    = 3.0f,  splash_r0_max    = 7.0f;
    int   splash_m_min     = 6,     splash_m_max     = 10;
};

struct World {
    AppConfig cfg;
    WaveParams wp;
    std::mt19937 rng;
    std::uniform_real_distribution<float> U{0.0f,1.0f};
    std::vector<Drop> drops;
    // Height field temporal (para shading por normales)
    std::vector<float> H;

    World(const AppConfig& c): cfg(c) {
        std::random_device rd; rng.seed(rd());
        drops.resize(cfg.N);
        H.resize(size_t(cfg.width) * size_t(cfg.height), 0.0f);
    }

    float rb(float a, float b) { return a + (b-a)*U(rng); }
    int   rbi(int a, int b) { return a + int(std::floor((b-a+1)*U(rng))); }

    void respawn_drop(Drop& d, float now_s) {
        d.x = rb(0.0f, float(cfg.width));
        d.y = rb(0.0f, float(cfg.height));
        d.t0 = now_s - rb(0.0f, 0.25f); // dispersión de inicio

        // "fuerza" -> amplitud base y splash correlacionados
        d.A0    = rb(wp.A0_min, wp.A0_max);
        d.alpha = rb(wp.alpha_min, wp.alpha_max);
        d.sigma = rb(wp.sigma_min, wp.sigma_max);
        d.f     = rb(wp.f_min, wp.f_max);
        d.c     = rb(wp.c_min, wp.c_max);
        d.maxLife = rb(wp.life_min, wp.life_max);

        // capilares
        d.cap_delta = rb(wp.cap_delta_min, wp.cap_delta_max);
        d.cap_sigma = rb(wp.cap_sigma_min, wp.cap_sigma_max);
        d.cap_gain  = rb(wp.cap_gain_min,  wp.cap_gain_max);

        // splash
        d.splash_amp   = rb(wp.splash_amp_min, wp.splash_amp_max) * d.A0; // ligado a fuerza
        d.splash_decay = rb(wp.splash_decay_min, wp.splash_decay_max);
        d.splash_r0    = rb(wp.splash_r0_min, wp.splash_r0_max);
        d.splash_m     = rbi(wp.splash_m_min, wp.splash_m_max);
        d.splash_phi   = rb(0.0f, float(2*M_PI));
    }

    void init(float now_s) {
        for (auto& d : drops) respawn_drop(d, now_s);
    }
    void maybe_respawn(float now_s) {
        for (auto& d : drops) {
            float age = now_s - d.t0;
            if (age > d.maxLife) respawn_drop(d, now_s);
        }
    }
};

// ---------- contribución de una gota (ripple + capilares + splash) ----------
static inline float ripple_contrib(float x, float y, float t, const Drop& d) {
    float tau = t - d.t0;
    if (tau <= 0.0f) return 0.0f;

    float dx = x - d.x, dy = y - d.y;
    float dist = std::sqrt(dx*dx + dy*dy);
    float ring = d.c * tau;

    // Cresta principal (gauss en (dist - ring)) + portadora temporal suave
    float gauss_main = std::exp(- ( (dist - ring)*(dist - ring) ) / (2.0f * d.sigma * d.sigma));
    float damp  = std::exp(- d.alpha * tau);
    float carrier_t = std::cos(2.0f * float(M_PI) * d.f * tau); // textura temporal sutil
    float main = d.A0 * damp * gauss_main * carrier_t;

    // Ripples capilares: dos satélites alrededor de la cresta, finos y más amortiguados
    float cap = 0.0f;
    {
        float d1 = dist - (ring - d.cap_delta);
        float d2 = dist - (ring + d.cap_delta);
        float g1 = std::exp(-(d1*d1)/(2.0f*d.cap_sigma*d.cap_sigma));
        float g2 = std::exp(-(d2*d2)/(2.0f*d.cap_sigma*d.cap_sigma));
        // amortiguamiento un poco más fuerte para capilares
        float damp_c = std::exp(- (d.alpha*1.25f) * tau);
        cap = d.cap_gain * d.A0 * damp_c * (g1 + g2) * 0.5f;
    }

    // Splash inicial: corona suave, angular, muy breve
    float splash = 0.0f;
    {
        // Sólo influye al inicio para no saturar
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

// ===================== RENDER & SHADING =====================

struct PixelBuffer {
    SDL_Texture* tex = nullptr;
    int w=0, h=0;
    Uint32 format = SDL_PIXELFORMAT_ARGB8888;
};

static bool create_pixel_buffer(SDL_Renderer* r, int w, int h, PixelBuffer& out) {
    out.w = w; out.h = h;
    out.tex = SDL_CreateTexture(r, out.format,
                                SDL_TEXTUREACCESS_STREAMING, w, h);
    return out.tex != nullptr;
}

static inline Uint32 pack_ARGB(Uint8 a, Uint8 R, Uint8 G, Uint8 B) {
    return (Uint32(a)<<24) | (Uint32(R)<<16) | (Uint32(G)<<8) | Uint32(B);
}

struct Vec3 { float x,y,z; };
static inline Vec3 v3(float x,float y,float z){ return {x,y,z}; }
static inline Vec3 add(Vec3 a, Vec3 b){ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
static inline Vec3 mul(Vec3 a, float s){ return {a.x*s,a.y*s,a.z*s}; }
static inline float dot(Vec3 a, Vec3 b){ return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline float invlen(Vec3 a){ return 1.0f/std::sqrt(std::max(1e-8f, dot(a,a))); }
static inline Vec3 norm(Vec3 a){ return mul(a, invlen(a)); }

static inline Uint8 to_byte(float x){
    int v = int(std::round(255.0f * std::clamp(x, 0.0f, 1.0f)));
    return (Uint8)v;
}

// Shading “agua”: normales de H, iluminación fría y sutil
static void shade_and_present(SDL_Renderer* renderer, PixelBuffer& pb,
                              const std::vector<float>& H, float slopeScale=6.0f)
{
    void* pixels=nullptr; int pitch=0;
    if (SDL_LockTexture(pb.tex, nullptr, &pixels, &pitch) != 0) {
        SDL_SetRenderDrawColor(renderer, 10,14,22,255);
        SDL_RenderClear(renderer);
        return;
    }

    const int W=pb.w, Hh=pb.h;
    Uint8* base = static_cast<Uint8*>(pixels);

    // Iluminación: luz rasante desde arriba-izquierda, vista frontal
    Vec3 L = norm(v3(-0.4f, -0.7f, 0.6f));
    Vec3 V = v3(0.0f, 0.0f, 1.0f);
    Vec3 Hhvec = norm(add(L, V));

    // Paleta “lluvia real”: agua fría
    // base (ambient): azul profundo; diffuse: azul medio; spec: casi blanco-azulado
    Vec3 ambientC = v3(0.045f, 0.08f, 0.12f);  // ~#0b1420
    Vec3 diffuseC = v3(0.12f,  0.35f, 0.55f);  // ~#1f5a8c
    Vec3 specC    = v3(0.80f,  0.90f, 1.00f);  // reflejo frío

    const float ambientK = 0.35f;
    const float diffK    = 0.75f;
    const float specK    = 0.35f;
    const float shininess= 32.0f;

    auto Hidx = [&](int x,int y)->float {
        x = std::clamp(x, 0, W-1);
        y = std::clamp(y, 0, Hh-1);
        return H[size_t(y)*size_t(W) + size_t(x)];
    };

    for (int y=0; y<Hh; ++y) {
        Uint32* row = reinterpret_cast<Uint32*>(base + y*size_t(pitch));
        for (int x=0; x<W; ++x) {
            // Derivadas centrales (pixel size = 1)
            float dhdx = 0.5f * (Hidx(x+1,y) - Hidx(x-1,y));
            float dhdy = 0.5f * (Hidx(x,y+1) - Hidx(x,y-1));
            // Normal “agua”: z hacia el observador; amplificamos pendiente
            Vec3 N = norm(v3(-slopeScale*dhdx, -slopeScale*dhdy, 1.0f));

            float ndotl = std::max(0.0f, dot(N, L));
            float ndoth = std::max(0.0f, dot(N, Hhvec));
            float spec  = std::pow(ndoth, shininess);

            // Composición
            Vec3 color = add(
                mul(ambientC, ambientK),
                add( mul(diffuseC, diffK * ndotl),
                     mul(specC,    specK * spec) )
            );

            // Micro modulación con el propio H para ligerísima variación
            float micro = 0.02f * std::tanh(0.8f * Hidx(x,y));
            color = add(color, v3(micro, micro, micro));

            Uint8 R = to_byte(color.x);
            Uint8 G = to_byte(color.y);
            Uint8 B = to_byte(color.z);
            row[x] = pack_ARGB(255, R, G, B);
        }
    }

    SDL_UnlockTexture(pb.tex);
    SDL_RenderCopy(renderer, pb.tex, nullptr, nullptr);
}

// Acumula H(x,y) = suma de contribuciones (superposición lineal)
static void accumulate_heightfield_sequential(std::vector<float>& H,
                                              int W, int Hh,
                                              const std::vector<Drop>& drops,
                                              float t_now)
{
    std::fill(H.begin(), H.end(), 0.0f);
    for (int y=0; y<Hh; ++y) {
        for (int x=0; x<W; ++x) {
            float acc = 0.0f;
            float fx = float(x) + 0.5f;
            float fy = float(y) + 0.5f;
            for (const auto& d : drops) {
                acc += ripple_contrib(fx, fy, t_now, d);
            }
            H[size_t(y)*size_t(W) + size_t(x)] = acc;
        }
    }
}

// ===================== MAIN / LOOP =====================

int main(int argc, char** argv) {
    try {
        AppConfig cfg = parse_args(argc, argv);

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            std::cerr << "SDL_Init error: " << SDL_GetError() << "\n";
            return 1;
        }

        SDL_Window* window = SDL_CreateWindow(
            "Rain Ripples (Secuencial Base + Shading)",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            cfg.width, cfg.height, SDL_WINDOW_SHOWN
        );
        if (!window) { std::cerr << "SDL_CreateWindow: " << SDL_GetError() << "\n"; SDL_Quit(); return 1; }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) { std::cerr << "SDL_CreateRenderer: " << SDL_GetError() << "\n";
            SDL_DestroyWindow(window); SDL_Quit(); return 1; }

        PixelBuffer pb;
        if (!create_pixel_buffer(renderer, cfg.width, cfg.height, pb)) {
            std::cerr << "SDL_CreateTexture: " << SDL_GetError() << "\n";
            SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 1;
        }

        World world(cfg);
        Uint64 pf = SDL_GetPerformanceFrequency();
        Uint64 t0 = SDL_GetPerformanceCounter();
        world.init(0.0f);

        bool running = true;
        SDL_Event ev;

        // FPS
        double fps_accum = 0.0; int fps_frames = 0; double fps_smoothed = 0.0;
        auto update_title = [&](double fps){
            std::ostringstream tt;
            tt<<"Rain Ripples"
              <<" | "<<cfg.width<<"x"<<cfg.height
              <<" | N="<<cfg.N
              <<" | FPS="<<(int)std::round(fps);
            SDL_SetWindowTitle(window, tt.str().c_str());
        };
        update_title(0.0);

        while (running) {
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) running = false;
                else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
            }

            // Tiempo
            Uint64 t1 = SDL_GetPerformanceCounter();
            double dt = double(t1 - t0)/double(pf);
            t0 = t1;
            static double accTime = 0.0; accTime += dt;
            float t_now = float(accTime);

            world.maybe_respawn(t_now);

            // Simulación (superposición lineal) -> height field
            accumulate_heightfield_sequential(world.H, cfg.width, cfg.height, world.drops, t_now);

            // Render con shading “agua”
            SDL_SetRenderDrawColor(renderer, 8,12,18,255);
            SDL_RenderClear(renderer);
            shade_and_present(renderer, pb, world.H, /*slopeScale=*/6.0f);
            SDL_RenderPresent(renderer);

            // FPS ~1s
            fps_accum += dt; fps_frames++;
            if (fps_accum >= 1.0) {
                double fps_inst = fps_frames / fps_accum;
                fps_smoothed = (fps_smoothed==0.0) ? fps_inst : (0.8*fps_smoothed + 0.2*fps_inst);
                update_title(fps_smoothed);
                fps_accum = 0.0; fps_frames = 0;
            }
        }

        SDL_DestroyTexture(pb.tex);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        print_usage(argv[0]);
        return 1;
    }
}
