#pragma once
#include <random>

struct RNG {
    std::mt19937 gen;
    std::uniform_real_distribution<float> U{0.0f,1.0f};

    RNG(int seed) {
        if (seed < 0) { std::random_device rd; gen.seed(rd()); }
        else          { gen.seed(static_cast<uint32_t>(seed)); }
    }
    inline float rb(float a, float b) { return a + (b-a)*U(gen); }
    inline int   rbi(int a, int b)    { return a + int((b-a+1)*U(gen)); }
};
