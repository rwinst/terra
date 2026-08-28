#include "SaveSystem.h"
#include "World.h"
#include "Player.h"
#include "Inventory.h"
#include "Config.h"
#include <cstdio>
#include <cstdlib>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* path = "/tmp/viz/integration_test.sav";

    World world(Config::WORLD_WIDTH, Config::WORLD_HEIGHT, 42u);
    world.generate();
    Player player;
    player.spawnAt(world.spawnX(), world.spawnY());
    player.health = 67.0f;
    Inventory inv;
    inv.addItem(ItemId::Wood, 23);
    inv.addItem(ItemId::PickaxeStone, 1);
    inv.selectSlot(1);

    // mutate the world a bit so we're not just round-tripping pristine gen output
    world.setTile(world.spawnX() / Config::TILE_SIZE, world.spawnY() / Config::TILE_SIZE + 3, TileType::Torch);
    world.recomputeLightingRegion(world.spawnX() / Config::TILE_SIZE, world.spawnY() / Config::TILE_SIZE + 3, 30);

    float dayTime = 123.45f;
    bool saved = SaveSystem::saveGame(path, world, player, inv, dayTime);
    CHECK(saved, "saveGame should succeed");

    World loadedWorld(1, 1, 0);
    Player loadedPlayer;
    Inventory loadedInv;
    float loadedDayTime = 0.0f;
    bool loaded = SaveSystem::loadGame(path, loadedWorld, loadedPlayer, loadedInv, loadedDayTime);
    CHECK(loaded, "loadGame should succeed");

    CHECK(loadedWorld.width() == world.width() && loadedWorld.height() == world.height(), "world dimensions should round-trip");
    int mismatches = 0;
    for (int y = 0; y < world.height(); y++)
        for (int x = 0; x < world.width(); x++)
            if (loadedWorld.getTile(x, y) != world.getTile(x, y)) mismatches++;
    printf("  tile mismatches: %d\n", mismatches);
    CHECK(mismatches == 0, "every tile should round-trip exactly, including the manually placed torch");

    CHECK(std::abs(loadedPlayer.x - player.x) < 0.01f, "player x should round-trip");
    CHECK(std::abs(loadedPlayer.y - player.y) < 0.01f, "player y should round-trip");
    CHECK(std::abs(loadedPlayer.health - 67.0f) < 0.01f, "player health should round-trip");

    CHECK(loadedInv.countItem(ItemId::Wood) == 23, "inventory wood count should round-trip");
    CHECK(loadedInv.countItem(ItemId::PickaxeStone) == 1, "inventory pickaxe should round-trip");
    CHECK(loadedInv.selectedSlot() == 1, "selected hotbar slot should round-trip");

    CHECK(std::abs(loadedDayTime - 123.45f) < 0.01f, "day/night clock should round-trip");

    // corrupted/missing file handling
    bool loadedMissing = SaveSystem::loadGame("/tmp/viz/does_not_exist.sav", loadedWorld, loadedPlayer, loadedInv, loadedDayTime);
    CHECK(!loadedMissing, "loading a nonexistent file should fail gracefully, not crash");

    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
