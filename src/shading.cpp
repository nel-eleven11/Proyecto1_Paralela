#include "shading.hpp"
#include <algorithm>
#include <cmath>

static inline Uint32 pack_ARGB(Uint8 a, Uint8 R, Uint8 G, Uint8 B) {
    return (Uint32(a)<<24) | (Uint32(R)<<16) | (Uint32(G)<<8) | Uint32(B);
}
static inline Uint8 to_byte(float x){
    int v = int(std::round(255.0f * std::clamp(x, 0.0f, 1.0f)));
    return (Uint8)v;
}

struct Vec3 { float x,y,z; };
static inline Vec3 v3(float x,float y,float z){ return {x,y,z}; }
static inline Vec3 add(Vec3 a, Vec3 b){ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
static inline Vec3 mul(Vec3 a, float s){ return {a.x*s,a.y*s,a.z*s}; }
static inline float dot(Vec3 a, Vec3 b){ return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline float invlen(Vec3 a){ return 1.0f/std::sqrt(std::max(1e-8f, dot(a,a))); }
static inline Vec3 norm(Vec3 a){ return mul(a, invlen(a)); }

// Paletas frías para “varios colores” acuáticos
static inline Vec3 ramp_aqua(float t) {
    // t in [0,1]: mezcla entre azules/celestes
    Vec3 c0 = v3(0.03f, 0.07f, 0.12f); // profundo
    Vec3 c1 = v3(0.10f, 0.28f, 0.45f); // medio
    Vec3 c2 = v3(0.20f, 0.55f, 0.78f); // claro
    if (t < 0.5f) return add(mul(c0, 1.0f-2*t), mul(c1, 2*t));
    return add(mul(c1, 2.0f*(1.0f-t)), mul(c2, 2.0f*(t-0.5f)));
}
static inline Vec3 ramp_mix(float t) {
    // Un poco más de variedad (cian/azul/verdoso)
    Vec3 a = v3(0.05f, 0.12f, 0.18f);
    Vec3 b = v3(0.08f, 0.35f, 0.55f);
    Vec3 c = v3(0.18f, 0.65f, 0.70f);
    Vec3 d = v3(0.06f, 0.40f, 0.30f);
    if (t < 0.33f)        return add(mul(a, 1.0f - t/0.33f), mul(b, t/0.33f));
    else if (t < 0.66f)   return add(mul(b, 1.0f - (t-0.33f)/0.33f), mul(c, (t-0.33f)/0.33f));
    else                  return add(mul(c, 1.0f - (t-0.66f)/0.34f), mul(d, (t-0.66f)/0.34f));
}

bool create_pixel_buffer(SDL_Renderer* r, int w, int h, PixelBuffer& out) {
    out.w = w; out.h = h;
    out.tex = SDL_CreateTexture(r, out.format, SDL_TEXTUREACCESS_STREAMING, w, h);
    return out.tex != nullptr;
}

void shade_and_present(SDL_Renderer* renderer, PixelBuffer& pb,
                       const std::vector<float>& H, float slopeScale,
                       int palette_mode)
{
    void* pixels=nullptr; int pitch=0;
    if (SDL_LockTexture(pb.tex, nullptr, &pixels, &pitch) != 0) {
        SDL_SetRenderDrawColor(renderer, 10,14,22,255);
        SDL_RenderClear(renderer);
        return;
    }

    const int W=pb.w, Hh=pb.h;
    Uint8* base = static_cast<Uint8*>(pixels);

    Vec3 L = norm(v3(-0.4f, -0.7f, 0.6f)); // luz rasante
    Vec3 V = v3(0.0f, 0.0f, 1.0f);
    Vec3 Hhvec = norm(add(L, V));

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
            float dhdx = 0.5f * (Hidx(x+1,y) - Hidx(x-1,y));
            float dhdy = 0.5f * (Hidx(x,y+1) - Hidx(x,y-1));
            Vec3 N = norm(v3(-slopeScale*dhdx, -slopeScale*dhdy, 1.0f));

            float ndotl = std::max(0.0f, dot(N, L));
            float ndoth = std::max(0.0f, dot(N, Hhvec));
            float spec  = std::pow(ndoth, shininess);

            // base acuática por rampa -> varios tonos azules
            float t = 0.5f + 0.5f * std::tanh(0.75f * Hidx(x,y)); // [-1,1] -> [0,1]
            Vec3 base = (palette_mode==0) ? ramp_aqua(t) : ramp_mix(t);

            // iluminación (Lambert + Blinn-Phong)
            Vec3 ambient = mul(base, ambientK);
            Vec3 diffuse = mul(base, diffK * ndotl);
            Vec3 specC   = v3(0.80f, 0.90f, 1.00f);
            Vec3 color   = add(ambient, add(diffuse, mul(specC, specK * spec)));

            Uint8 R = to_byte(color.x);
            Uint8 G = to_byte(color.y);
            Uint8 B = to_byte(color.z);
            row[x] = pack_ARGB(255, R, G, B);
        }
    }

    SDL_UnlockTexture(pb.tex);
    SDL_RenderCopy(renderer, pb.tex, nullptr, nullptr);
}
