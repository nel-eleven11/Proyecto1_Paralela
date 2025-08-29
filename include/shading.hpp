#pragma once
#include <SDL.h>
#include <vector>

struct PixelBuffer {
    SDL_Texture* tex = nullptr;
    int w=0, h=0;
    Uint32 format = SDL_PIXELFORMAT_ARGB8888;
};

bool create_pixel_buffer(SDL_Renderer* r, int w, int h, PixelBuffer& out);

// Sombrado “agua” con Fresnel/reflexión y tinta opcional
void shade_and_present(
    SDL_Renderer* renderer,
    PixelBuffer& pb,
    const std::vector<float>& H,
    const std::vector<float>& CR,
    const std::vector<float>& CG,
    const std::vector<float>& CB,
    float slopeScale,
    int palette_mode,
    bool ink_enabled,
    float ink_strength
);
