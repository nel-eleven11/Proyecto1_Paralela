#include <SDL.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include "config.hpp"
#include "waves.hpp"
#include "model.hpp"
#include "shading.hpp"
#include "ink.hpp"

int main(int argc, char** argv) {
    try {
        AppConfig cfg = parse_args(argc, argv);

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            std::cerr << "SDL_Init error: " << SDL_GetError() << "\n";
            return 1;
        }

        SDL_Window* window = SDL_CreateWindow(
            "Rain Ripples (Secuencial + Ink)",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            cfg.width, cfg.height, SDL_WINDOW_SHOWN
        );
        if (!window) {
            std::cerr << "SDL_CreateWindow: " << SDL_GetError() << "\n";
            SDL_Quit(); return 1;
        }

        Uint32 renderer_flags = SDL_RENDERER_ACCELERATED;
        if (!cfg.novsync) renderer_flags |= SDL_RENDERER_PRESENTVSYNC;

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, renderer_flags);
        if (!renderer) {
            std::cerr << "SDL_CreateRenderer: " << SDL_GetError() << "\n";
            SDL_DestroyWindow(window); SDL_Quit(); return 1;
        }

        if (cfg.fpslog || cfg.profile) {
            SDL_RendererInfo ri{};
            if (SDL_GetRendererInfo(renderer, &ri)==0) {
                std::cout << "Renderer: " << (ri.name?ri.name:"<unknown>") << "\n";
            }
        }

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

            Uint64 t1 = SDL_GetPerformanceCounter();
            double dt = double(t1 - t0)/double(pf);
            t0 = t1;
            static double accTime = 0.0; accTime += dt;
            float t_now = float(accTime);

            world.maybe_respawn(t_now);

            // ---- Simulación + inyección de tinta ----
            Uint64 tA = 0, tB = 0, tC = 0;
            if (cfg.profile) tA = SDL_GetPerformanceCounter();

            accumulate_heightfield_sequential(
                world.H, world.CR, world.CG, world.CB,
                cfg.width, cfg.height, world.drops, t_now,
                cfg.ink_enabled, cfg.ink_gain
            );

            // Difusión/decay de tinta
            ink_postprocess(world.CR, world.CG, world.CB,
                            cfg.width, cfg.height,
                            float(dt), cfg.ink_decay, cfg.ink_blur_mix);

            if (cfg.profile) tB = SDL_GetPerformanceCounter();

            // ---- Render ----
            SDL_SetRenderDrawColor(renderer, 8,12,18,255);
            SDL_RenderClear(renderer);
            shade_and_present(renderer, pb, world.H,
                              world.CR, world.CG, world.CB,
                              cfg.slope, cfg.palette,
                              cfg.ink_enabled, cfg.ink_strength);
            SDL_RenderPresent(renderer);

            if (cfg.profile) {
                tC = SDL_GetPerformanceCounter();
                double k = 1000.0 / double(pf);
                double sim_ms   = (tB - tA) * k;
                double shade_ms = (tC - tB) * k;
                std::cout << "sim+ink=" << sim_ms << " ms, shade+present=" << shade_ms << " ms\n";
            }

            // ---- FPS (cada ~1s) ----
            fps_accum += dt; fps_frames++;
            if (fps_accum >= 1.0) {
                double fps_inst = fps_frames / fps_accum;
                fps_smoothed = (fps_smoothed==0.0) ? fps_inst : (0.8*fps_smoothed + 0.2*fps_inst);
                update_title(fps_smoothed);
                if (cfg.fpslog) std::cout << "FPS= " << fps_smoothed << "\n";
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
