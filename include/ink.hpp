#pragma once
#include <vector>

// Decaimiento + blur mezclado (difusión) por frame
void ink_postprocess(
    std::vector<float>& CR,
    std::vector<float>& CG,
    std::vector<float>& CB,
    int W, int H,
    float dt,
    float decay_lambda,  // s^-1
    float blur_mix       // 0..1, mezcla de box blur 3x3
);
