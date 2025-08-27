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

// -------------------- Modelo de gotas/ondas --------------------
struct Drop {
    float x;      // posicion en px
    float y;
    float t0;     // tiempo de impacto (s)
    float A0;     // amplitud inicial
    float alpha;  // disipacion temporal
    float sigma;  // ancho del anillo
    float f;      // frecuencia portadora (ciclos/seg)
    float c;      // velocidad de expansion (px/seg)
    float maxLife;// vida maxima (s) para respawn
};

// Parámetros globales "estéticos" 
struct WaveParams {
    float A0_min   = 0.6f,  A0_max   = 1.0f;
    float alpha_min= 0.6f,  alpha_max= 1.2f;   // s^-1
    float sigma_min= 2.0f,  sigma_max= 6.0f;   // px
    float f_min    = 2.0f,  f_max    = 4.0f;   // Hz
    float c_min    = 80.0f, c_max    = 140.0f; // px/s
    float life_min = 2.0f,  life_max = 4.0f;   // s
    float gain     = 0.8f;                      // ganancia de visualizacion
};

struct World {
    AppConfig cfg;
    WaveParams wp;
    std::mt19937 rng;
    std::uniform_real_distribution<float> unif01{0.0f,1.0f};
    std::vector<Drop> drops; // N gotas activas

    World(const AppConfig& c): cfg(c) {
        std::random_device rd;
        rng.seed(rd());
        drops.resize(cfg.N);
    }

    float rand_between(float a, float b) {
        return a + (b - a) * unif01(rng);
    }

    void respawn_drop(Drop& d, float now_s) {
        d.x = rand_between(0.0f, float(cfg.width));
        d.y = rand_between(0.0f, float(cfg.height));
        // Para que no caigan todas exactamente al mismo tiempo, t0 puede estar en el pasado ligero.
        d.t0 = now_s - rand_between(0.0f, 0.25f);
        d.A0 = rand_between(wp.A0_min, wp.A0_max);
        d.alpha = rand_between(wp.alpha_min, wp.alpha_max);
        d.sigma = rand_between(wp.sigma_min, wp.sigma_max);
        d.f = rand_between(wp.f_min, wp.f_max);
        d.c = rand_between(wp.c_min, wp.c_max);
        d.maxLife = rand_between(wp.life_min, wp.life_max);
    }

    void init(float now_s) {
        for (auto& d : drops) respawn_drop(d, now_s);
    }

    void maybe_respawn(float now_s) {
        for (auto& d : drops) {
            float age = now_s - d.t0;
            if (age > d.maxLife) {
                respawn_drop(d, now_s);
            }
        }
    }
};

// Contribucion de UNA gota a un punto (x,y) en tiempo t (seg)
static inline float ripple_contrib(float x, float y, float t, const Drop& d) {
    float tau = t - d.t0;                // edad de la onda
    if (tau <= 0.0f) return 0.0f;        // aun no impacta
    float dx = x - d.x;
    float dy = y - d.y;
    float dist = std::sqrt(dx*dx + dy*dy);
    float ring = d.c * tau;              // radio de la cresta principal
    float gauss = std::exp(- ( (dist - ring)*(dist - ring) ) / (2.0f * d.sigma * d.sigma));
    float damp  = std::exp(- d.alpha * tau);
    float carrier = std::cos(2.0f * float(M_PI) * d.f * tau);
    return d.A0 * damp * gauss * carrier;
}

// -------------------- Render en textura (software) --------------------
struct PixelBuffer {
    SDL_Texture* tex = nullptr;
    int w=0, h=0;
    Uint32 format = SDL_PIXELFORMAT_ARGB8888;
    int pitch=0;
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

// Mapea suma de amplitudes [-inf,inf] a gris [0,255] con compresion suave
static inline Uint8 tone_map(float v, float gain) {
    // compresion suave: tanh(gain*v) -> [-1,1]
    float g = std::tanh(gain * v);
    float y = 0.5f * (g + 1.0f); // [0,1]
    int   p = int(std::round(255.0f * std::clamp(y, 0.0f, 1.0f)));
    return Uint8(p);
}

static void render_ripples_sequential(SDL_Renderer* renderer, PixelBuffer& pb, const World& world, float t) {
    void* pixels = nullptr;
    int pitch   = 0;
    if (SDL_LockTexture(pb.tex, nullptr, &pixels, &pitch) != 0) {
        // Si no se puede bloquear la textura, dibujamos fondo plano
        SDL_SetRenderDrawColor(renderer, 16,16,16,255);
        SDL_RenderClear(renderer);
        return;
    }

    // pitch en bytes; cada pixel = 4 bytes ARGB8888
    Uint8* base = static_cast<Uint8*>(pixels);
    const int W = pb.w, H = pb.h;

    for (int y=0; y<H; ++y) {
        Uint32* row = reinterpret_cast<Uint32*>(base + y * pitch);
        for (int x=0; x<W; ++x) {
            // Superposicion
            float acc = 0.0f;
            // Sumatoria de todas las gotas
            for (const auto& d : world.drops) {
                acc += ripple_contrib(float(x)+0.5f, float(y)+0.5f, t, d);
            }
            Uint8 g = tone_map(acc, world.wp.gain);
            // Un leve tinte azulado para "agua"
            Uint8 r = Uint8(std::max(0, g - 10));
            Uint8 b = Uint8(std::min(255, int(g) + 20));
            row[x] = pack_ARGB(255, r, g, b);
        }
    }

    SDL_UnlockTexture(pb.tex);
    SDL_RenderCopy(renderer, pb.tex, nullptr, nullptr);
}

// -------------------- Main / Loop --------------------
int main(int argc, char** argv) {
    try {
        AppConfig cfg = parse_args(argc, argv);

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            std::cerr << "SDL_Init error: " << SDL_GetError() << "\n";
            return 1;
        }

        SDL_Window* window = SDL_CreateWindow(
            "Rain Ripples (Base Secuencial)",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            cfg.width, cfg.height, SDL_WINDOW_SHOWN
        );
        if (!window) {
            std::cerr << "SDL_CreateWindow error: " << SDL_GetError() << "\n";
            SDL_Quit(); return 1;
        }

        // VSYNC activado para que no se dispare el consumo
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            std::cerr << "SDL_CreateRenderer error: " << SDL_GetError() << "\n";
            SDL_DestroyWindow(window); SDL_Quit(); return 1;
        }

        PixelBuffer pb;
        if (!create_pixel_buffer(renderer, cfg.width, cfg.height, pb)) {
            std::cerr << "SDL_CreateTexture error: " << SDL_GetError() << "\n";
            SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 1;
        }

        // Mundo / gotas
        World world(cfg);
        Uint64 pf = SDL_GetPerformanceFrequency();
        Uint64 t0 = SDL_GetPerformanceCounter();
        float t0s = 0.0f;
        world.init(t0s);

        bool running = true;
        SDL_Event ev;

        // FPS
        double fps_accum = 0.0; int fps_frames = 0; double fps_smoothed = 0.0;
        auto update_title = [&](double fps){
            std::ostringstream tt;
            tt<<"Rain Ripples (Base Secuencial)"
              <<" | "<<cfg.width<<"x"<<cfg.height
              <<" | N="<<cfg.N
              <<" | FPS="<<(int)std::round(fps);
            SDL_SetWindowTitle(window, tt.str().c_str());
        };
        update_title(0.0);

        while (running) {
            // Eventos
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) running = false;
                else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
            }

            // Tiempo actual en segundos
            Uint64 t1 = SDL_GetPerformanceCounter();
            double dt = double(t1 - t0)/double(pf);
            t0 = t1;
            static double accTime = 0.0;
            accTime += dt;
            float t_now = float(accTime);

            // Respawn de gotas según vida
            world.maybe_respawn(t_now);

            // Render de la superposicion (secuencial)
            SDL_SetRenderDrawColor(renderer, 8,12,18,255);
            SDL_RenderClear(renderer);
            render_ripples_sequential(renderer, pb, world, t_now);
            SDL_RenderPresent(renderer);

            // FPS cada ~1s
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
