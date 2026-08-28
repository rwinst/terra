#pragma once
#include <cstdint>
#include <array>

// Classic permutation-table Perlin noise (Ken Perlin's well-known gradient
// noise algorithm). Deterministic for a given seed - the same seed always
// produces the same world, which is what lets us save just the seed plus
// edits rather than the whole terrain (we still save the full edited grid
// for simplicity/robustness, but determinism is what makes world gen
// reproducible/testable in the first place).
class PerlinNoise {
public:
    explicit PerlinNoise(uint32_t seed);

    // Single-octave noise, output clamped to roughly [-1, 1].
    double noise2D(double x, double y) const;

    // Fractal Brownian motion: layered octaves of noise2D for more natural,
    // detailed variation. Output normalized to roughly [-1, 1].
    double fbm2D(double x, double y, int octaves, double persistence, double lacunarity) const;

private:
    std::array<int, 512> perm{};

    static double fade(double t);
    static double lerp(double t, double a, double b);
    static double grad(int hash, double x, double y);
};
