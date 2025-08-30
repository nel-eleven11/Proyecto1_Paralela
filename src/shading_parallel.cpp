#include "shading.hpp"
#include <algorithm>
#include <cmath>
#include <omp.h>

static inline Uint32 pack_ARGB(Uint8 a, Uint8 R, Uint8 G, Uint8 B) {
    return (Uint32(a)<<24) | (Uint32(R)<<16) | (Uint32(G)<<8) | Uint32(B);
}
static inline Uint8 to_byte(float x){
    int v = int(std::round(255.0f * std::clamp(x, 0.0f, 1.0f)));
    return (Uint8)v;
}
static inline float saturate(float x){ return std::clamp(x, 0.0f, 1.0f); }

struct Vec3 { float x,y,z; };
static inline Vec3 v3(float x,float y,float z){ return {x,y,z}; }
static inline Vec3 add(Vec3 a, Vec3 b){ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
static inline Vec3 sub(Vec3 a, Vec3 b){ return {a.x-b.x,a.y-b.y,a.z-b.z}; }
static inline Vec3 mul(Vec3 a, float s){ return {a.x*s,a.y*s,a.z*s}; }
static inline float dot(Vec3 a, Vec3 b){ return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline float invlen(Vec3 a){ return 1.0f/std::sqrt(std::max(1e-8f, dot(a,a))); }
static inline Vec3 norm(Vec3 a){ return mul(a, invlen(a)); }
static inline Vec3 reflect(Vec3 I, Vec3 N){ return sub(mul(N, 2.0f*dot(N,I)), I); }

// Refracción (Snell) simple; devuelve false si hay TIR
static inline bool refract(Vec3 I, Vec3 N, float eta, Vec3& T){
    float cosi = -dot(N, I);
    float cost2 = 1.0f - eta*eta*(1.0f - cosi*cosi);
    if (cost2 < 0.0f) return false;
    T = add( mul(I, eta),
             mul(N, (eta*cosi - std::sqrt(cost2))) );
    return true;
}

static inline Vec3 ramp_aqua(float t) {
    Vec3 c0 = v3(0.03f, 0.07f, 0.12f);
    Vec3 c1 = v3(0.10f, 0.28f, 0.45f);
    Vec3 c2 = v3(0.20f, 0.55f, 0.78f);
    if (t < 0.5f) return add(mul(c0, 1.0f-2*t), mul(c1, 2*t));
    return add(mul(c1, 2.0f*(1.0f-t)), mul(c2, 2.0f*(t-0.5f)));
}
static inline Vec3 ramp_mix(float t) {
    Vec3 a = v3(0.05f, 0.12f, 0.18f);
    Vec3 b = v3(0.08f, 0.35f, 0.55f);
    Vec3 c = v3(0.18f, 0.65f, 0.70f);
    Vec3 d = v3(0.06f, 0.40f, 0.30f);
    if (t < 0.33f)        return add(mul(a, 1.0f - t/0.33f), mul(b, t/0.33f));
    else if (t < 0.66f)   return add(mul(b, 1.0f - (t-0.33f)/0.33f), mul(c, (t-0.33f)/0.33f));
    else                  return add(mul(c, 1.0f - (t-0.66f)/0.34f), mul(d, (t-0.66f)/0.34f));
}

// Cielo procedural (para reflexión) y "fondo" acuático para refracción
static inline Vec3 sample_env(Vec3 dir) {
    float u = 0.5f * (dir.x + 1.0f);
    float v = 0.5f * (dir.y + 1.0f);
    Vec3 horizon = v3(0.90f, 0.95f, 1.00f);
    Vec3 zenith  = v3(0.52f, 0.70f, 0.88f);
    Vec3 sky = add( mul(horizon, saturate(1.0f - v)),
                    mul(zenith,  saturate(v)) );
    Vec3 tint = v3(0.02f,0.02f,0.03f);
    sky = add(sky, mul(tint, 0.15f*(std::sin(6.28318f*u)*std::sin(3.14159f*v))));
    if (dir.z < 0.0f) { // mirando "abajo"
        Vec3 deep = v3(0.02f, 0.05f, 0.08f);
        return add(mul(deep, 0.8f), mul(sky, 0.2f));
    }
    return sky;
}
static inline Vec3 sample_underwater(Vec3 dir){
    // Fondo acuático suave para la refracción (más verdoso/oscuro)
    float v = 0.5f * (dir.y + 1.0f);
    Vec3 deep  = v3(0.03f, 0.07f, 0.10f);
    Vec3 green = v3(0.04f, 0.12f, 0.09f);
    return add( mul(deep, 1.0f - v), mul(green, v) );
}

static inline float gamma_encode(float x){ return std::pow(saturate(x), 1.0f/2.2f); }

bool create_pixel_buffer(SDL_Renderer* r, int w, int h, PixelBuffer& out) {
    out.w = w; out.h = h;
    out.tex = SDL_CreateTexture(r, out.format, SDL_TEXTUREACCESS_STREAMING, w, h);
    return out.tex != nullptr;
}

void shade_and_present(SDL_Renderer* renderer, PixelBuffer& pb,
                       const std::vector<float>& H,
                       const std::vector<float>& CR,
                       const std::vector<float>& CG,
                       const std::vector<float>& CB,
                       float slopeScale,
                       int palette_mode,
                       bool ink_enabled,
                       float ink_strength)
{
    void* pixels=nullptr; int pitch=0;
    if (SDL_LockTexture(pb.tex, nullptr, &pixels, &pitch) != 0) {
        SDL_SetRenderDrawColor(renderer, 10,14,22,255);
        SDL_RenderClear(renderer);
        return;
    }

    const int W=pb.w, Hh=pb.h;
    Uint8* base = static_cast<Uint8*>(pixels);

    Vec3 L = norm(v3(-0.4f, -0.7f, 0.6f));
    Vec3 V = v3(0.0f, 0.0f, 1.0f);
    Vec3 Hhvec = norm(add(L, V));

    const Vec3 absorption = v3(0.35f, 0.18f, 0.05f);
    const float ambientK = 0.18f;
    const float diffK    = 0.62f;
    const float specK    = 0.25f;   // especular un poco más visible en cresta
    const float shininess= 90.0f;

    auto Hidx = [&](int x,int y)->float {
        x = std::clamp(x, 0, W-1);
        y = std::clamp(y, 0, Hh-1);
        return H[size_t(y)*size_t(W) + size_t(x)];
    };
    auto Cidx = [&](const std::vector<float>& C,int x,int y)->float{
        x = std::clamp(x, 0, W-1);
        y = std::clamp(y, 0, Hh-1);
        return C[size_t(y)*size_t(W) + size_t(x)];
    };

    #pragma omp parallel for collapse(2)
    for (int y=0; y<Hh; ++y) {
        for (int x=0; x<W; ++x) {
            float hC = Hidx(x,y);
            float dhdx = 0.5f * (Hidx(x+1,y) - Hidx(x-1,y));
            float dhdy = 0.5f * (Hidx(x,y+1) - Hidx(x,y-1));
            float slopeMag = std::sqrt(dhdx*dhdx + dhdy*dhdy);
            Vec3 N = norm(v3(-slopeScale*dhdx, -slopeScale*dhdy, 1.0f));

            float ndotl = std::max(0.0f, dot(N, L));
            float ndoth = std::max(0.0f, dot(N, Hhvec));
            float spec  = std::pow(ndoth, shininess);

            // Agua base (real) con absorción
            float thickness = std::abs(hC);
            Vec3 baseWater  = v3(0.04f, 0.10f, 0.16f);
            Vec3 trans = v3(std::exp(-absorption.x*thickness),
                            std::exp(-absorption.y*thickness),
                            std::exp(-absorption.z*thickness));
            Vec3 diffuseWater = v3(baseWater.x*trans.x, baseWater.y*trans.y, baseWater.z*trans.z);

            if (palette_mode == 0 || palette_mode == 1) {
                float t = 0.5f + 0.5f * std::tanh(0.75f * hC);
                diffuseWater = (palette_mode==0) ? ramp_aqua(t) : ramp_mix(t);
            }

            // ---- Tinta (tiñe el difuso) ----
            if (ink_enabled) {
                float r = Cidx(CR,x,y), g = Cidx(CG,x,y), b = Cidx(CB,x,y);
                float sum = std::max(1e-6f, r+g+b);
                float s   = saturate(ink_strength * sum);
                Vec3 ink = v3(r/sum, g/sum, b/sum);
                diffuseWater = add( mul(diffuseWater, (1.0f - s)),
                                    mul(ink, s) );
            }

            // Fresnel + reflexión + refracción
            float cosNV = std::max(0.0f, dot(N, V));
            float F0 = 0.02f;
            float Fresnel = F0 + (1.0f - F0)*std::pow(1.0f - cosNV, 5.0f);

            Vec3 I = mul(V, -1.0f);
            Vec3 R = reflect(I, N);
            Vec3 envRefl = sample_env(R);

            Vec3 T; bool ok = refract(I, N, 1.0f/1.33f, T);
            Vec3 envRefr = ok ? sample_underwater(T) : diffuseWater;

            Vec3 ambient = mul(diffuseWater, ambientK);
            Vec3 diffuse = mul(diffuseWater, diffK * ndotl);
            Vec3 specC   = v3(0.96f, 0.98f, 1.00f);
            Vec3 local   = add(ambient, add(diffuse, mul(specC, specK * spec)));

            // Mezcla Fresnel
            Vec3 colorLR = add( mul(envRefr, 1.0f - Fresnel),
                                mul(envRefl, Fresnel) );
            Vec3 color = add(local, colorLR);

            // Rim highlight sutil en crestas (depende de la pendiente)
            float rim = saturate((slopeMag * slopeScale - 0.25f) * 1.6f);
            color = add(color, mul(v3(1.0f,1.0f,1.0f), 0.07f * rim));

            // Vignette
            float ux = (x + 0.5f) / float(W);
            float uy = (y + 0.5f) / float(Hh);
            float dx = ux - 0.5f, dy = uy - 0.5f;
            float r2 = dx*dx + dy*dy;
            float vign = 1.0f - 0.15f * std::pow(std::min(1.0f, r2*3.2f), 1.2f);
            color = mul(color, vign);

            // Micro modulación
            float micro = 0.02f * std::tanh(0.8f * hC);
            color = add(color, v3(micro, micro, micro));

            Uint8 R8 = to_byte(gamma_encode(color.x));
            Uint8 G8 = to_byte(gamma_encode(color.y));
            Uint8 B8 = to_byte(gamma_encode(color.z));
            
            Uint32* row = reinterpret_cast<Uint32*>(base + y*size_t(pitch));
            row[x] = pack_ARGB(255, R8, G8, B8);
        }
    }

    SDL_UnlockTexture(pb.tex);
    SDL_RenderCopy(renderer, pb.tex, nullptr, nullptr);
}
