#include "ink.hpp"
#include <algorithm>
#include <cmath>

static inline float clamp01(float x){ return std::clamp(x, 0.0f, 1.0f); }

void ink_postprocess(
    std::vector<float>& CR,
    std::vector<float>& CG,
    std::vector<float>& CB,
    int W, int H,
    float dt,
    float decay_lambda,
    float blur_mix)
{
    size_t SZ = size_t(W)*size_t(H);
    // Decay exponencial por canal
    float kdec = std::exp(-decay_lambda * std::max(0.0f, dt));
    for (size_t i=0;i<SZ;++i){ CR[i]*=kdec; CG[i]*=kdec; CB[i]*=kdec; }

    if (blur_mix <= 0.0f) return;

    // Box blur 3x3 y mezcla
    std::vector<float> TR(SZ), TG(SZ), TB(SZ);
    auto at = [&](int x,int y)->size_t{
        x = std::clamp(x,0,W-1); y = std::clamp(y,0,H-1);
        return size_t(y)*size_t(W)+size_t(x);
    };
    for (int y=0;y<H;++y){
        for (int x=0;x<W;++x){
            float sr=0, sg=0, sb=0;
            for (int j=-1;j<=1;++j)
                for (int i=-1;i<=1;++i){
                    size_t id = at(x+i,y+j);
                    sr += CR[id]; sg += CG[id]; sb += CB[id];
                }
            size_t idx = size_t(y)*size_t(W)+size_t(x);
            TR[idx] = sr/9.0f; TG[idx] = sg/9.0f; TB[idx] = sb/9.0f;
        }
    }
    float keep = 1.0f - blur_mix;
    for (size_t i=0;i<SZ;++i){
        CR[i] = clamp01(keep*CR[i] + blur_mix*TR[i]);
        CG[i] = clamp01(keep*CG[i] + blur_mix*TG[i]);
        CB[i] = clamp01(keep*CB[i] + blur_mix*TB[i]);
    }
}
