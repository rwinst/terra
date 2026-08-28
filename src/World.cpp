#include "World.h"
#include "Config.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <cstring>

World::World(int width, int height, uint32_t seed_)
    : w(width), h(height), seed(seed_),
      tiles(static_cast<size_t>(width) * height, TileType::Air),
      light(static_cast<size_t>(width) * height, 0.0f),
      heightMap(width, 0),
      noise(seed_)
{
}

int World::clampX(int x) const {
    if (x < 0) return 0;
    if (x >= w) return w - 1;
    return x;
}

TileType World::getTile(int x, int y) const {
    if (y < 0) return TileType::Air;       // open sky above the map
    if (y >= h) return TileType::Bedrock;   // safety floor below the map
    return tiles[idx(clampX(x), y)];
}

void World::setTile(int x, int y, TileType t) {
    // Unlike getTile, writes outside real bounds are simply dropped rather
    // than clamped - clamping a write could silently corrupt a valid edge
    // tile, which is worse than a no-op.
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    tiles[idx(x, y)] = t;
}

bool World::isSolid(int x, int y) const {
    return getTileInfo(getTile(x, y)).solid;
}

int World::surfaceHeight(int x) const {
    return heightMap[clampX(x)];
}

Zone World::zoneAt(int y) const {
    if (y < Config::ZONE_SURFACE_END) return Zone::Surface;
    if (y < Config::ZONE_UNDERGROUND_END) return Zone::Underground;
    if (y < Config::ZONE_CAVERNS_END) return Zone::Caverns;
    return Zone::TheDeep;
}

const char* World::zoneName(Zone z) const {
    switch (z) {
        case Zone::Surface:     return "Surface";
        case Zone::Underground: return "Underground";
        case Zone::Caverns:     return "Caverns";
        case Zone::TheDeep:     return "The Deep";
    }
    return "Unknown";
}

// =================================================================
// Generation pipeline
// =================================================================

void World::generate() {
    generateHeightmap();
    fillLayers();
    carveCaves();
    placeOres();
    placeBedrock();
    placeTrees();
    chooseSpawn();
    recomputeLightingFull();
}

void World::generateHeightmap() {
    heightMap.assign(w, Config::SURFACE_BASE_HEIGHT);

    for (int x = 0; x < w; x++) {
        double n = noise.fbm2D(x * 0.018, 0.0, 4, 0.5, 2.0);
        int height_ = Config::SURFACE_BASE_HEIGHT + static_cast<int>(std::lround(n * Config::SURFACE_AMPLITUDE));
        height_ = std::clamp(height_, 12, h - Config::BEDROCK_ROWS - 25);
        heightMap[x] = height_;
    }

    // Light smoothing pass (3-tap box blur) so the noise never produces an
    // un-walkable single-column cliff. Frequency is already low so this is
    // a safety net more than a major shaping pass.
    for (int pass = 0; pass < 2; pass++) {
        std::vector<int> smoothed = heightMap;
        for (int x = 0; x < w; x++) {
            int left  = (x > 0)     ? heightMap[x - 1] : heightMap[x];
            int right = (x < w - 1) ? heightMap[x + 1] : heightMap[x];
            smoothed[x] = static_cast<int>(std::lround((left + heightMap[x] + right) / 3.0));
        }
        heightMap = smoothed;
    }
}

void World::fillLayers() {
    for (int x = 0; x < w; x++) {
        int surf = heightMap[x];
        int dirtDepth = 4 + static_cast<int>(tileHash(x + static_cast<int>(seed), 7) % 3); // 4..6

        for (int y = 0; y < h; y++) {
            TileType t;
            if (y < surf)              t = TileType::Air;
            else if (y == surf)        t = TileType::Grass;
            else if (y < surf + dirtDepth) t = TileType::Dirt;
            else                       t = TileType::Stone;
            tiles[idx(x, y)] = t;
        }
    }
}

void World::carveCaves() {
    for (int x = 0; x < w; x++) {
        int surf = heightMap[x];
        int stoneStart = surf + 8; // buffer of solid dirt/stone before caves may appear

        for (int y = stoneStart; y < h - Config::BEDROCK_ROWS; y++) {
            if (tiles[idx(x, y)] != TileType::Stone) continue;

            double cv = noise.fbm2D(x * 0.045, y * 0.065, 3, 0.55, 2.0);
            float depthFactor = std::clamp(
                static_cast<float>(y - stoneStart) / static_cast<float>(std::max(1, h - stoneStart)),
                0.0f, 1.0f);
            // Caves get more frequent (lower threshold) the deeper you go,
            // so the surface layer is mostly solid and "The Deep" is airy.
            // (Calibrated empirically against this noise's actual value
            // distribution: 0.18 -> ~3% open, 0.05 -> ~30% open.)
            double threshold = 0.18 - 0.13 * depthFactor;

            if (cv > threshold) {
                tiles[idx(x, y)] = TileType::Air;
            }
        }
    }
}

void World::placeOres() {
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h - Config::BEDROCK_ROWS; y++) {
            if (tiles[idx(x, y)] != TileType::Stone) continue;

            // Large arbitrary coordinate offsets decorrelate each ore's
            // noise field from the others (and from the cave noise) while
            // still reusing a single noise generator instance.
            double coalV = noise.fbm2D(x * 0.11 + 500.0,  y * 0.11 + 500.0,  3, 0.5, 2.0);
            double ironV = noise.fbm2D(x * 0.10 + 1500.0, y * 0.10 + 1500.0, 3, 0.5, 2.0);
            double goldV = noise.fbm2D(x * 0.095 + 3000.0, y * 0.095 + 3000.0, 3, 0.5, 2.0);
            double diaV  = noise.fbm2D(x * 0.09 + 5000.0, y * 0.09 + 5000.0, 3, 0.5, 2.0);

            // Thresholds calibrated empirically against this noise's value
            // distribution to give a descending rarity curve: coal ~8% of
            // eligible stone, iron ~3%, gold ~1.3%, diamond ~0.5%.
            if (y >= Config::ZONE_CAVERNS_END - 10 && diaV > 0.24) {
                tiles[idx(x, y)] = TileType::DiamondOre;
            } else if (y >= Config::ZONE_UNDERGROUND_END && goldV > 0.21) {
                tiles[idx(x, y)] = TileType::GoldOre;
            } else if (y >= Config::ZONE_SURFACE_END + 5 && ironV > 0.18) {
                tiles[idx(x, y)] = TileType::IronOre;
            } else if (coalV > 0.14) {
                tiles[idx(x, y)] = TileType::CoalOre;
            }
        }
    }
}

void World::placeBedrock() {
    for (int y = h - Config::BEDROCK_ROWS; y < h; y++)
        for (int x = 0; x < w; x++)
            tiles[idx(x, y)] = TileType::Bedrock;
}

void World::placeTrees() {
    int lastTreeX = -100;
    for (int x = 2; x < w - 2; x++) {
        if (x - lastTreeX < 5) continue;
        if (tiles[idx(x, heightMap[x])] != TileType::Grass) continue;

        double t = noise.fbm2D(x * 0.3 + 777.0, 314.0 + seed * 0.0001, 2, 0.5, 2.0);
        if (t < 0.08) continue; // calibrated for a reasonably dense, but not solid, forest

        int trunkHeight = 3 + static_cast<int>(tileHash(x + static_cast<int>(seed), 55) % 3); // 3..5
        int surf = heightMap[x];
        int top = surf - trunkHeight; // row of the topmost trunk tile
        if (top - 2 < 0) continue;

        for (int ty = top; ty < surf; ty++) {
            tiles[idx(x, ty)] = TileType::Wood;
        }

        auto leafIfAir = [&](int lx, int ly) {
            if (lx < 0 || lx >= w || ly < 0 || ly >= h) return;
            if (tiles[idx(lx, ly)] == TileType::Air) tiles[idx(lx, ly)] = TileType::Leaves;
        };
        leafIfAir(x - 1, top - 1); leafIfAir(x, top - 1); leafIfAir(x + 1, top - 1);
        leafIfAir(x - 1, top);                             leafIfAir(x + 1, top);
        leafIfAir(x - 1, top + 1);                          leafIfAir(x + 1, top + 1);

        lastTreeX = x;
    }
}

void World::chooseSpawn() {
    int sx = w / 2;
    int surf = heightMap[sx];

    for (int dx = -2; dx <= 2; dx++) {
        int cx = sx + dx;
        if (cx < 0 || cx >= w) continue;
        for (int y = surf - 8; y < surf; y++) {
            if (y < 0) continue;
            tiles[idx(cx, y)] = TileType::Air;
        }
    }

    spawnPx = sx * Config::TILE_SIZE + Config::TILE_SIZE / 2;
    spawnPy = (surf - 5) * Config::TILE_SIZE;
}

// =================================================================
// Lighting
// =================================================================

int World::findSkylightEntryRow(int x) const {
    for (int y = 0; y < h; y++) {
        if (getTileInfo(rawTile(x, y)).solid) return y;
    }
    return h;
}

void World::recomputeLightingFull() {
    std::fill(light.begin(), light.end(), 0.0f);
    std::deque<std::pair<int, int>> q;

    for (int x = 0; x < w; x++) {
        int entry = findSkylightEntryRow(x);
        for (int y = 0; y < entry; y++) {
            light[idx(x, y)] = 1.0f;
            q.push_back({x, y});
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (rawTile(x, y) == TileType::Torch) {
                light[idx(x, y)] = std::max(light[idx(x, y)], Config::TORCH_LIGHT);
                q.push_back({x, y});
            }
        }
    }

    static const int dx4[4] = {1, -1, 0, 0};
    static const int dy4[4] = {0, 0, 1, -1};

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop_front();
        float cur = light[idx(cx, cy)];
        if (cur <= 0.0f) continue;

        for (int d = 0; d < 4; d++) {
            int nx = cx + dx4[d], ny = cy + dy4[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            bool solidN = getTileInfo(rawTile(nx, ny)).solid;
            float falloff = solidN ? Config::LIGHT_FALLOFF_SOLID : Config::LIGHT_FALLOFF_AIR;
            float nv = cur - falloff;
            if (nv > light[idx(nx, ny)] + 1e-4f) {
                light[idx(nx, ny)] = nv;
                q.push_back({nx, ny});
            }
        }
    }

    for (auto& l : light) if (l < Config::AMBIENT_CAVE_LIGHT) l = Config::AMBIENT_CAVE_LIGHT;
}

void World::recomputeLightingRegion(int cx, int cy, int radius) {
    int x0 = std::max(0, cx - radius);
    int x1 = std::min(w - 1, cx + radius);
    int y0 = std::max(0, cy - radius);
    int y1 = std::min(h - 1, cy + radius);

    std::deque<std::pair<int, int>> q;

    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            light[idx(x, y)] = 0.0f;

    for (int x = x0; x <= x1; x++) {
        int entry = findSkylightEntryRow(x);
        for (int y = y0; y < entry && y <= y1; y++) {
            light[idx(x, y)] = 1.0f;
            q.push_back({x, y});
        }
    }
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (rawTile(x, y) == TileType::Torch) {
                light[idx(x, y)] = std::max(light[idx(x, y)], Config::TORCH_LIGHT);
                q.push_back({x, y});
            }
        }
    }

    // Seed from just outside the box using its already-correct values, so
    // light can flow in from the rest of the world instead of the box
    // edges incorrectly going dark.
    auto tryBorder = [&](int x, int y) {
        if (x < 0 || x >= w || y < 0 || y >= h) return;
        if (x >= x0 && x <= x1 && y >= y0 && y <= y1) return;
        q.push_back({x, y});
    };
    for (int x = x0; x <= x1; x++) { tryBorder(x, y0 - 1); tryBorder(x, y1 + 1); }
    for (int y = y0; y <= y1; y++) { tryBorder(x0 - 1, y); tryBorder(x1 + 1, y); }

    static const int dx4[4] = {1, -1, 0, 0};
    static const int dy4[4] = {0, 0, 1, -1};

    while (!q.empty()) {
        auto [px, py] = q.front();
        q.pop_front();
        float cur = light[idx(px, py)];
        if (cur <= 0.0f) continue;

        for (int d = 0; d < 4; d++) {
            int nx = px + dx4[d], ny = py + dy4[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            bool solidN = getTileInfo(rawTile(nx, ny)).solid;
            float falloff = solidN ? Config::LIGHT_FALLOFF_SOLID : Config::LIGHT_FALLOFF_AIR;
            float nv = cur - falloff;
            if (nv > light[idx(nx, ny)] + 1e-4f) {
                light[idx(nx, ny)] = nv;
                q.push_back({nx, ny});
            }
        }
    }

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            int i = idx(x, y);
            if (light[i] < Config::AMBIENT_CAVE_LIGHT) light[i] = Config::AMBIENT_CAVE_LIGHT;
        }
    }
}

float World::getLight(int x, int y) const {
    if (y < 0) return 1.0f;
    if (y >= h) return 0.0f;
    return light[idx(clampX(x), y)];
}

// =================================================================
// Rendering helpers (plain Color out, no graphics library involved)
// =================================================================

Color World::getTileRenderColor(int x, int y) const {
    TileType t = getTile(x, y);
    const TileInfo& info = getTileInfo(t);
    Color base = varyColor(info.color, x, y, 10);
    float lightVal = getLight(x, y);
    float mul = 0.04f + 0.96f * lightVal; // never fully vanish to black
    return base.scaled(mul);
}

Color World::getBackgroundColor(int x, int y, float dayPhase01) const {
    int surf = surfaceHeight(x);
    if (y < surf) {
        Color nightTop(10, 12, 35);
        Color dayTop(120, 180, 235);
        return Color::lerp(nightTop, dayTop, dayPhase01);
    }
    Color caveBg(40, 33, 45);
    float lightVal = getLight(x, y);
    float mul = 0.05f + 0.95f * lightVal;
    return caveBg.scaled(mul);
}

// =================================================================
// Serialization
// =================================================================

bool World::writeTo(std::ostream& os) const {
    const char magic[4] = {'T', 'W', 'L', 'D'};
    os.write(magic, 4);
    int32_t ww = w, hh = h;
    os.write(reinterpret_cast<const char*>(&ww), sizeof(ww));
    os.write(reinterpret_cast<const char*>(&hh), sizeof(hh));
    os.write(reinterpret_cast<const char*>(&seed), sizeof(seed));
    os.write(reinterpret_cast<const char*>(&spawnPx), sizeof(spawnPx));
    os.write(reinterpret_cast<const char*>(&spawnPy), sizeof(spawnPy));

    std::vector<uint8_t> raw(tiles.size());
    for (size_t i = 0; i < tiles.size(); i++) raw[i] = static_cast<uint8_t>(tiles[i]);
    os.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    return os.good();
}

bool World::readFrom(std::istream& is) {
    char magic[4];
    is.read(magic, 4);
    if (!is.good() || std::memcmp(magic, "TWLD", 4) != 0) return false;

    int32_t ww = 0, hh = 0;
    is.read(reinterpret_cast<char*>(&ww), sizeof(ww));
    is.read(reinterpret_cast<char*>(&hh), sizeof(hh));
    is.read(reinterpret_cast<char*>(&seed), sizeof(seed));
    is.read(reinterpret_cast<char*>(&spawnPx), sizeof(spawnPx));
    is.read(reinterpret_cast<char*>(&spawnPy), sizeof(spawnPy));
    if (!is.good() || ww <= 0 || hh <= 0 || ww > 100000 || hh > 100000) return false;

    w = ww;
    h = hh;
    tiles.assign(static_cast<size_t>(w) * h, TileType::Air);
    light.assign(static_cast<size_t>(w) * h, 0.0f);
    heightMap.assign(w, 0);
    noise = PerlinNoise(seed);

    std::vector<uint8_t> raw(static_cast<size_t>(w) * h);
    is.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    if (!is.good()) return false;

    for (size_t i = 0; i < raw.size(); i++) {
        if (raw[i] >= static_cast<uint8_t>(TileType::Count)) return false;
        tiles[i] = static_cast<TileType>(raw[i]);
    }

    for (int x = 0; x < w; x++) {
        int y = 0;
        while (y < h && !getTileInfo(rawTile(x, y)).solid) y++;
        heightMap[x] = y;
    }

    recomputeLightingFull();
    return true;
}
