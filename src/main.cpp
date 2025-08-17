#include <SDL.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <limits>

struct AppConfig {
    int width = 800;
    int height = 600;
    int fps_target = 60;      // 0 = sin cap; si usas VSYNC, este cap es redundante
    int N = 200;              // aún no se usa; lo dejamos listo
    bool vsync = true;        // VSYNC por defecto
};

static void print_usage(const char* prog) {
    std::cout << "Uso: " << prog << " [--width W] [--height H] [--fps_target F] [--N N] [--no-vsync]\n"
              << "  --width/-w        Ancho de ventana (min 640)\n"
              << "  --height/-h       Alto de ventana (min 480)\n"
              << "  --fps_target/-f   FPS objetivo (0 = sin limite)\n"
              << "  --N/-n            Numero de elementos (aun no usado en Fase 1)\n"
              << "  --no-vsync        Desactiva VSYNC\n";
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
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need_value = [&](const char* name) {
            if (i + 1 >= argc) throw std::runtime_error(std::string("Falta valor para ") + name);
            return argv[++i];
        };
        if (a == "--width" || a == "-w") {
            const char* v = need_value(a.c_str());
            if (!parse_int(v, cfg.width, 640, 16384))
                throw std::runtime_error("width invalido (>=640)");
        } else if (a == "--height" || a == "-h") {
            const char* v = need_value(a.c_str());
            if (!parse_int(v, cfg.height, 480, 16384))
                throw std::runtime_error("height invalido (>=480)");
        } else if (a == "--fps_target" || a == "-f") {
            const char* v = need_value(a.c_str());
            if (!parse_int(v, cfg.fps_target, 0, 1000))
                throw std::runtime_error("fps_target invalido (>=0)");
        } else if (a == "--N" || a == "-n") {
            const char* v = need_value(a.c_str());
            if (!parse_int(v, cfg.N, 1, std::numeric_limits<int>::max()))
                throw std::runtime_error("N invalido (>=1)");
        } else if (a == "--no-vsync") {
            cfg.vsync = false;
        } else if (a == "--help" || a == "-h?" || a == "-?") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::ostringstream oss;
            oss << "Argumento desconocido: " << a;
            throw std::runtime_error(oss.str());
        }
    }
    return cfg;
}

int main(int argc, char** argv) {
    try {
        AppConfig cfg = parse_args(argc, argv);

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            std::cerr << "SDL_Init error: " << SDL_GetError() << "\n";
            return 1;
        }

        Uint32 winFlags = SDL_WINDOW_SHOWN;
        SDL_Window* window = SDL_CreateWindow(
            "Screensaver (Fase 1)",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            cfg.width, cfg.height, winFlags
        );
        if (!window) {
            std::cerr << "SDL_CreateWindow error: " << SDL_GetError() << "\n";
            SDL_Quit();
            return 1;
        }

        Uint32 rendFlags = SDL_RENDERER_ACCELERATED;
        if (cfg.vsync) rendFlags |= SDL_RENDERER_PRESENTVSYNC;

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, rendFlags);
        if (!renderer) {
            std::cerr << "SDL_CreateRenderer error: " << SDL_GetError() << "\n";
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        bool running = true;
        SDL_Event ev;

        // Timing / FPS
        Uint64 perfFreq = SDL_GetPerformanceFrequency();
        Uint64 t0 = SDL_GetPerformanceCounter();
        double fps_accum_time = 0.0;
        int fps_frames = 0;
        double fps_smoothed = 0.0;

        auto update_title = [&](double fps) {
            std::ostringstream title;
            title << "Screensaver (Fase 1)"
                  << " | " << cfg.width << "x" << cfg.height
                  << " | N=" << cfg.N
                  << " | FPS=" << static_cast<int>(fps + 0.5)
                  << (cfg.vsync ? " (VSYNC)" : "");
            SDL_SetWindowTitle(window, title.str().c_str());
        };

        update_title(0.0);

        while (running) {
            // Eventos
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) {
                    running = false;
                } else if (ev.type == SDL_KEYDOWN) {
                    if (ev.key.keysym.sym == SDLK_ESCAPE) running = false;
                }
            }

            // Delta time
            Uint64 t1 = SDL_GetPerformanceCounter();
            double dt = double(t1 - t0) / double(perfFreq);
            t0 = t1;


            // Render de fondo 
            double hue = fmod(SDL_GetTicks() * 0.05, 360.0); // 0..360
            // Pasamos hue a un gris dinámico simple (placeholder):
            Uint8 c = static_cast<Uint8>(127 + 127 * std::sin(SDL_GetTicks() * 0.002));
            (void)hue;

            SDL_SetRenderDrawColor(renderer, c, c, c, 255);
            SDL_RenderClear(renderer);

            // Placeholder: cuadro central para ver algo
            int rw = cfg.width / 10;
            int rh = cfg.height / 10;
            SDL_Rect r{cfg.width/2 - rw/2, cfg.height/2 - rh/2, rw, rh};
            SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
            SDL_RenderFillRect(renderer, &r);

            SDL_RenderPresent(renderer);

            // FPS (actualización ~1s)
            fps_accum_time += dt;
            fps_frames += 1;
            if (fps_accum_time >= 1.0) {
                double fps_inst = fps_frames / fps_accum_time;
                // simple suavizado
                fps_smoothed = (fps_smoothed == 0.0) ? fps_inst : (0.8 * fps_smoothed + 0.2 * fps_inst);
                update_title(fps_smoothed);
                fps_accum_time = 0.0;
                fps_frames = 0;
            }

            // Cap de FPS si VSYNC desactivado y fps_target > 0
            if (!cfg.vsync && cfg.fps_target > 0) {
                double target_dt = 1.0 / double(cfg.fps_target);
                Uint64 after = SDL_GetPerformanceCounter();
                double frame_dt = double(after - t1) / double(perfFreq);
                if (frame_dt < target_dt) {
                    Uint32 ms = static_cast<Uint32>((target_dt - frame_dt) * 1000.0);
                    if (ms > 0) SDL_Delay(ms);
                }
            }
        }

        // Limpieza
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
