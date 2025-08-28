#pragma once
#include <vector>
#include "waves.hpp"

// Acumula el heightfield secuencialmente.
// Complejidad O(W*H*N). Ideal para paralelizar después por píxel/tile.
void accumulate_heightfield_sequential(
    std::vector<float>& H,
    int W, int Hh,
    const std::vector<Drop>& drops,
    float t_now
);
