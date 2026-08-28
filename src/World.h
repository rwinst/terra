#pragma once
#include <vector>
#include <cstdint>
#include <iostream>
#include "TileType.h"
#include "Color.h"
#include "Noise.h"

enum class Zone { Surface, Underground, Caverns, TheDeep };

class World {
public:
    World(int width, int height, uint32_t seed);

    void generate();

    // Tile access. Always returns something sane even for out-of-range
    // coordinates: above the map is open Air, below is Bedrock, and the
    // x axis is clamped - no caller needs to bounds-check first.
    TileType getTile(int x, int y) const;
    void setTile(int x, int y, TileType t);
    bool isSolid(int x, int y) const;

    int width() const { return w; }
    int height() const { return h; }
    uint32_t getSeed() const { return seed; }

    int surfaceHeight(int x) const; // cached generation-time heightmap (for spawn/zone/spawning logic)
    Zone zoneAt(int y) const;
    const char* zoneName(Zone z) const;

    // ---- Lighting ----
    float getLight(int x, int y) const; // 0..1
    void recomputeLightingFull();
    void recomputeLightingRegion(int cx, int cy, int radius);

    // ---- Rendering helpers (still SDL-free: plain Color out) ----
    Color getTileRenderColor(int x, int y) const;
    Color getBackgroundColor(int x, int y, float dayPhase01) const;

    int spawnX() const { return spawnPx; }
    int spawnY() const { return spawnPy; }

    // ---- Serialization ----
    bool writeTo(std::ostream& os) const;
    bool readFrom(std::istream& is);

private:
    int w, h;
    uint32_t seed;
    std::vector<TileType> tiles;
    std::vector<float> light;
    std::vector<int> heightMap;
    PerlinNoise noise;
    int spawnPx = 0, spawnPy = 0;

    int idx(int x, int y) const { return y * w + x; }
    int clampX(int x) const;
    TileType rawTile(int x, int y) const { return tiles[idx(x, y)]; } // no bounds checks; internal use only
    int findSkylightEntryRow(int x) const; // first solid row scanning down from 0, current tiles

    void generateHeightmap();
    void fillLayers();
    void carveCaves();
    void placeOres();
    void placeBedrock();
    void placeTrees();
    void chooseSpawn();
};
