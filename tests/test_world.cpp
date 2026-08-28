#include "World.h"
#include "TileType.h"
#include "Config.h"
#include <cstdio>
#include <cstdint>
#include <vector>
#include <map>
#include <chrono>
#include <sstream>
#include <cassert>

static void writePPM(const char* path, int w, int h, const std::vector<uint8_t>& rgb) {
    FILE* f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb.data(), 1, rgb.size(), f);
    fclose(f);
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0); // abort() on a failed assert skips normal buffer flushing

    using clk = std::chrono::high_resolution_clock;

    auto t0 = clk::now();
    World world(Config::WORLD_WIDTH, Config::WORLD_HEIGHT, 12345u);
    world.generate();
    auto t1 = clk::now();

    double genMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("Generation took %.2f ms for %dx%d = %d tiles\n",
        genMs, world.width(), world.height(), world.width() * world.height());

    // ---- Tile distribution stats ----
    std::map<TileType, int> counts;
    for (int y = 0; y < world.height(); y++)
        for (int x = 0; x < world.width(); x++)
            counts[world.getTile(x, y)]++;

    int total = world.width() * world.height();
    printf("\n--- Tile distribution ---\n");
    for (auto& [t, c] : counts) {
        printf("%-12s %7d  (%.2f%%)\n", getTileInfo(t).name, c, 100.0 * c / total);
    }

    // sanity assertions
    assert(counts[TileType::Bedrock] == world.width() * Config::BEDROCK_ROWS);
    assert(counts[TileType::DiamondOre] > 0 && "expected at least some diamond to generate");
    assert(counts[TileType::DiamondOre] < counts[TileType::CoalOre] && "diamond should be rarer than coal");
    assert(counts[TileType::GoldOre] < counts[TileType::IronOre] || counts[TileType::GoldOre] < counts[TileType::CoalOre]);
    assert(counts[TileType::Wood] > 0 && "expected trees to generate");
    printf("\nAll distribution sanity checks passed.\n");

    // ---- Render full map to PNG (1 px per tile, no lighting) ----
    std::vector<uint8_t> img(static_cast<size_t>(world.width()) * world.height() * 3);
    for (int y = 0; y < world.height(); y++) {
        for (int x = 0; x < world.width(); x++) {
            Color c = getTileInfo(world.getTile(x, y)).color;
            if (world.getTile(x, y) == TileType::Air) {
                c = (y < world.surfaceHeight(x)) ? Color(135, 195, 245) : Color(25, 22, 30);
            }
            size_t i = (static_cast<size_t>(y) * world.width() + x) * 3;
            img[i] = c.r; img[i+1] = c.g; img[i+2] = c.b;
        }
    }
    writePPM("/tmp/viz/world_base.ppm", world.width(), world.height(), img);

    // ---- Render WITH lighting applied ----
    for (int y = 0; y < world.height(); y++) {
        for (int x = 0; x < world.width(); x++) {
            Color c = world.getTileRenderColor(x, y);
            if (world.getTile(x, y) == TileType::Air) {
                c = world.getBackgroundColor(x, y, 1.0f); // daytime
            }
            size_t i = (static_cast<size_t>(y) * world.width() + x) * 3;
            img[i] = c.r; img[i+1] = c.g; img[i+2] = c.b;
        }
    }
    writePPM("/tmp/viz/world_lit_day.ppm", world.width(), world.height(), img);

    for (int y = 0; y < world.height(); y++) {
        for (int x = 0; x < world.width(); x++) {
            Color c = world.getTileRenderColor(x, y);
            if (world.getTile(x, y) == TileType::Air) {
                c = world.getBackgroundColor(x, y, 0.0f); // night
            }
            size_t i = (static_cast<size_t>(y) * world.width() + x) * 3;
            img[i] = c.r; img[i+1] = c.g; img[i+2] = c.b;
        }
    }
    writePPM("/tmp/viz/world_lit_night.ppm", world.width(), world.height(), img);

    // ---- Pure light value heatmap (grayscale) ----
    for (int y = 0; y < world.height(); y++) {
        for (int x = 0; x < world.width(); x++) {
            uint8_t g = static_cast<uint8_t>(world.getLight(x, y) * 255);
            size_t i = (static_cast<size_t>(y) * world.width() + x) * 3;
            img[i] = img[i+1] = img[i+2] = g;
        }
    }
    writePPM("/tmp/viz/world_light_heatmap.ppm", world.width(), world.height(), img);

    // ---- Lighting correctness checks ----
    int sx = world.width() / 2;
    int surf = world.surfaceHeight(sx);
    printf("\nSurface at x=%d is row %d, light there = %.3f (expect ~1.0)\n", sx, surf, world.getLight(sx, surf - 1));
    assert(world.getLight(sx, surf - 1) > 0.9f);

    // Find a deep stone tile far from any opening and check it's dark
    int deepY = world.height() - Config::BEDROCK_ROWS - 5;
    float deepLight = world.getLight(sx, deepY);
    printf("Deep tile (%d,%d) light = %.4f (expect close to ambient floor %.4f)\n", sx, deepY, deepLight, Config::AMBIENT_CAVE_LIGHT);

    // Place a torch in a dark spot and confirm light increases locally
    // find a solid stone tile deep down to dig out and place a torch in
    int torchX = sx, torchY = -1;
    for (int y = Config::ZONE_CAVERNS_END; y < world.height() - Config::BEDROCK_ROWS; y++) {
        if (world.getTile(torchX, y) != TileType::Air) { torchY = y; break; }
    }
    if (torchY > 0) {
        float before = world.getLight(torchX, torchY);
        // can't place a torch on solid ground tile directly (torch is non-solid, occupies an air cell) -
        // simulate player digging then placing a torch in the resulting space
        world.setTile(torchX, torchY, TileType::Air);
        world.recomputeLightingRegion(torchX, torchY, Config::LIGHT_UPDATE_RADIUS);
        float afterDig = world.getLight(torchX, torchY);
        world.setTile(torchX, torchY, TileType::Torch);
        world.recomputeLightingRegion(torchX, torchY, Config::LIGHT_UPDATE_RADIUS);
        float afterTorch = world.getLight(torchX, torchY);
        printf("\nTorch test at (%d,%d): before=%.4f afterDig=%.4f afterTorch=%.4f\n", torchX, torchY, before, afterDig, afterTorch);
        assert(afterTorch > afterDig);
        assert(afterTorch > 0.9f);
        // neighbor a few tiles away should also brighten somewhat
        float neighborBefore = world.getLight(torchX + 5, torchY);
        printf("Neighbor 5 tiles away light = %.4f\n", neighborBefore);
    } else {
        printf("\n(no solid tile found for torch test in expected range - skipping)\n");
    }

    // ---- Save / load round trip ----
    {
        std::ostringstream oss(std::ios::binary);
        bool wrote = world.writeTo(oss);
        assert(wrote);
        std::string data = oss.str();
        printf("\nSerialized world size: %zu bytes\n", data.size());

        World loaded(1, 1, 0); // dummy dims, readFrom will resize
        std::istringstream iss(data, std::ios::binary);
        bool ok = loaded.readFrom(iss);
        assert(ok);
        assert(loaded.width() == world.width());
        assert(loaded.height() == world.height());

        int mismatches = 0;
        for (int y = 0; y < world.height(); y++)
            for (int x = 0; x < world.width(); x++)
                if (loaded.getTile(x, y) != world.getTile(x, y)) mismatches++;
        printf("Tile mismatches after round-trip: %d (expect 0)\n", mismatches);
        assert(mismatches == 0);
        assert(loaded.spawnX() == world.spawnX() && loaded.spawnY() == world.spawnY());
    }

    // ---- Zone boundaries sanity ----
    printf("\nZone at row 10: %s\n", world.zoneName(world.zoneAt(10)));
    printf("Zone at row 70: %s\n", world.zoneName(world.zoneAt(70)));
    printf("Zone at row 110: %s\n", world.zoneName(world.zoneAt(110)));
    printf("Zone at row 150: %s\n", world.zoneName(world.zoneAt(150)));

    printf("\nALL WORLD TESTS PASSED\n");
    return 0;
}
