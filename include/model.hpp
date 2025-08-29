#pragma once
#include <vector>
#include "waves.hpp"

// Acumula campo H y, opcionalmente, inyecta tinta CR/CG/CB
void accumulate_heightfield_sequential(
    std::vector<float>& H,
    std::vector<float>& CR,
    std::vector<float>& CG,
    std::vector<float>& CB,
    int W, int Hh,
    const std::vector<Drop>& drops,
    float t_now,
    bool  ink_enabled,
    float ink_gain
);
