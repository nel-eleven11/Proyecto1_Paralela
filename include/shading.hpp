#pragma once
#include <SDL.h>
#include <vector>

struct PixelBuffer {
    SDL_Texture* tex = nullptr;
    int w=0, h=0;
    Uint32 format = SDL_PIXELFORMAT_ARGB8888;
};

// Crea textura de streaming para el backbuffer
bool create_pixel_buffer(SDL_Renderer* r, int w, int h, PixelBuffer& out);

// Sombrado tipo agua: calcula normales sobre H y compone color.
// palette_mode: 0 aqua, 1 mezcla aqua con variaciones
void shade_and_present(
    SDL_Renderer* renderer,
    PixelBuffer& pb,
    const std::vector<float>& H,
    float slopeScale,
    int palette_mode
);
