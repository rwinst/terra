#include "Noise.h"
#include <random>
#include <cmath>
#include <algorithm>

PerlinNoise::PerlinNoise(uint32_t seed) {
    std::array<int, 256> p{};
    for (int i = 0; i < 256; i++) p[i] = i;

    std::mt19937 rng(seed);
    for (int i = 255; i > 0; i--) {
        std::uniform_int_distribution<int> dist(0, i);
        int j = dist(rng);
        std::swap(p[i], p[j]);
    }
    for (int i = 0; i < 512; i++) perm[i] = p[i & 255];
}

double PerlinNoise::fade(double t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double PerlinNoise::lerp(double t, double a, double b) {
    return a + t * (b - a);
}

double PerlinNoise::grad(int hash, double x, double y) {
    // 8 evenly-spaced unit gradient vectors. Using only the 4 diagonal
    // directions (a common simplification) produces a visible cross-hatch
    // grid artifact; these extra directions fix that.
    static constexpr double kHalfSqrt2 = 0.70710678118654752440;
    static constexpr double gx[8] = {  1.0,  kHalfSqrt2,  0.0, -kHalfSqrt2, -1.0, -kHalfSqrt2,  0.0,  kHalfSqrt2 };
    static constexpr double gy[8] = {  0.0,  kHalfSqrt2,  1.0,  kHalfSqrt2,  0.0, -kHalfSqrt2, -1.0, -kHalfSqrt2 };
    int h = hash & 7;
    return gx[h] * x + gy[h] * y;
}

double PerlinNoise::noise2D(double x, double y) const {
    int xi = static_cast<int>(std::floor(x)) & 255;
    int yi = static_cast<int>(std::floor(y)) & 255;

    double xf = x - std::floor(x);
    double yf = y - std::floor(y);

    double u = fade(xf);
    double v = fade(yf);

    int aa = perm[perm[xi] + yi];
    int ab = perm[perm[xi] + yi + 1];
    int ba = perm[perm[xi + 1] + yi];
    int bb = perm[perm[xi + 1] + yi + 1];

    double x1 = lerp(u, grad(aa, xf, yf), grad(ba, xf - 1.0, yf));
    double x2 = lerp(u, grad(ab, xf, yf - 1.0), grad(bb, xf - 1.0, yf - 1.0));
    double result = lerp(v, x1, x2) * 0.7; // empirical scale to land near [-1,1]

    return std::clamp(result, -1.0, 1.0);
}

double PerlinNoise::fbm2D(double x, double y, int octaves, double persistence, double lacunarity) const {
    double total = 0.0, amplitude = 1.0, frequency = 1.0, maxValue = 0.0;
    for (int i = 0; i < octaves; i++) {
        total += noise2D(x * frequency, y * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return maxValue > 0.0 ? total / maxValue : 0.0;
}
