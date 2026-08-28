#pragma once
#include "Color.h"
#include <cstdint>
#include <string>
#include <functional>

// A tiny built-in pixel font so the game needs zero external font files and
// zero SDL_ttf dependency - just core SDL2. The glyph data here has no
// rendering-library dependency at all; the caller supplies a callback that
// draws one filled rectangle per "on" pixel, so this same code can render
// to an SDL_Renderer, a raw pixel buffer for testing, or anything else.
namespace BitmapFont {
    constexpr int GLYPH_WIDTH = 5;
    constexpr int GLYPH_HEIGHT = 7;

    // Returns 7 rows of 5-bit patterns (bit 4 = leftmost pixel), or nullptr
    // for characters with no glyph (rendered as blank space).
    const uint8_t* getGlyph(char c);

    using RectDrawFn = std::function<void(int x, int y, int w, int h, Color c)>;

    void drawText(const std::string& text, int x, int y, int scale, Color color, const RectDrawFn& drawRect);

    int textWidth(const std::string& text, int scale);
    int textHeight(int scale);
}
