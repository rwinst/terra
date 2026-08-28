#pragma once
#include <cstdint>

// A dependency-free color type. The SDL render layer converts this to
// SDL_Color at the point of drawing; every other system (world gen,
// lighting, tests) only ever sees this plain struct.
struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;

    Color() = default;
    Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    Color scaled(float mul) const {
        auto clamp = [](float v) {
            if (v < 0.f) return 0.f;
            if (v > 255.f) return 255.f;
            return v;
        };
        return Color(
            static_cast<uint8_t>(clamp(r * mul)),
            static_cast<uint8_t>(clamp(g * mul)),
            static_cast<uint8_t>(clamp(b * mul)),
            a
        );
    }

    static Color lerp(const Color& a_, const Color& b_, float t) {
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        auto L = [&](uint8_t x, uint8_t y) {
            return static_cast<uint8_t>(x + (y - x) * t);
        };
        return Color(L(a_.r, b_.r), L(a_.g, b_.g), L(a_.b, b_.b), L(a_.a, b_.a));
    }
};
