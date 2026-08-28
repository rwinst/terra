#pragma once
#include <cstdint>
#include "Color.h"

// Every block in the world is one of these. Order matters for save-file
// compatibility (we serialize as raw bytes), so only ever append new
// types at the end, never reorder/remove existing ones.
enum class TileType : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Wood,
    Leaves,
    CoalOre,
    IronOre,
    GoldOre,
    DiamondOre,
    Bedrock,
    Water,
    Torch,
    Count
};

// Sentinel meaning "no tool can ever mine this" (used by Bedrock).
constexpr int TOOL_TIER_UNMINEABLE = 255;

struct TileInfo {
    const char* name;
    Color color;
    bool solid;        // blocks physics movement
    bool minable;       // can the player ever break it
    bool emitsLight;    // torch-like
    float hardness;     // base seconds-to-mine at a mining speed multiplier of 1.0
    int   minToolTier;  // 0 = bare hands, see Item.h for tier meanings; TOOL_TIER_UNMINEABLE = never
};

// Returns the static property table entry for a tile type. Defined in
// TileType.cpp; pure data, no dependencies, safe to call from anywhere
// (rendering, world gen, tests) without pulling in graphics headers.
const TileInfo& getTileInfo(TileType t);

// Deterministic, dependency-free per-coordinate hash used to add subtle
// per-tile color variation so flat-colored tiles don't look perfectly
// uniform. Same inputs always give the same output (important so the
// "texture" doesn't flicker between frames).
inline uint32_t tileHash(int x, int y) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Applies a small deterministic per-tile color jitter for visual texture.
inline Color varyColor(Color base, int x, int y, int amount) {
    uint32_t h = tileHash(x, y);
    int dr = static_cast<int>(h % (2 * amount + 1)) - amount;
    int dg = static_cast<int>((h / 7) % (2 * amount + 1)) - amount;
    int db = static_cast<int>((h / 49) % (2 * amount + 1)) - amount;
    auto clampByte = [](int v) {
        if (v < 0) return 0;
        if (v > 255) return 255;
        return v;
    };
    return Color(
        static_cast<uint8_t>(clampByte(base.r + dr)),
        static_cast<uint8_t>(clampByte(base.g + dg)),
        static_cast<uint8_t>(clampByte(base.b + db)),
        base.a
    );
}
