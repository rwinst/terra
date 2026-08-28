#include "Player.h"
#include "World.h"
#include "Inventory.h"
#include "Config.h"
#include <cstdio>
#include <cassert>
#include <cmath>

// Builds a small flat test world: solid floor at row `floorY`, everything
// else air, walls of Stone at the given x columns spanning the given y range.
World makeFlatWorld(int floorY) {
    World w(100, 50, 1u);
    for (int x = 0; x < 100; x++) {
        for (int y = floorY; y < 50; y++) w.setTile(x, y, TileType::Stone);
    }
    return w;
}

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

void test_gravity_and_landing() {
    printf("\n[test_gravity_and_landing]\n");
    World w = makeFlatWorld(20);
    Player p;
    p.spawnAt(50 * Config::TILE_SIZE, 5 * Config::TILE_SIZE);

    PlayerInput input{};
    for (int i = 0; i < 600; i++) p.update(1.0f / 60.0f, w, input); // 10 sim-seconds, plenty to land

    float floorTopPx = 20 * Config::TILE_SIZE;
    printf("  player.y=%.2f vy=%.2f onGround=%d (floor top=%.2f)\n", p.y, p.vy, p.onGround, floorTopPx);
    CHECK(p.onGround, "player should be grounded after falling onto flat ground");
    CHECK(std::fabs((p.y + Config::PLAYER_HEIGHT) - floorTopPx) < 0.5f, "player feet should rest exactly on floor top");
    CHECK(std::fabs(p.vy) < 0.01f, "vertical velocity should be zeroed on landing");
}

void test_wall_collision_no_tunnel() {
    printf("\n[test_wall_collision_no_tunnel]\n");
    World w = makeFlatWorld(20);
    // a wall directly to the right of spawn
    int wallX = 52;
    for (int y = 0; y < 20; y++) w.setTile(wallX, y, TileType::Stone);

    Player p;
    p.spawnAt(50 * Config::TILE_SIZE, 19 * Config::TILE_SIZE); // start standing on the floor already

    PlayerInput input{};
    input.right = true;
    for (int i = 0; i < 300; i++) p.update(1.0f / 60.0f, w, input); // 5 seconds of holding right

    float wallLeftPx = wallX * Config::TILE_SIZE;
    printf("  player.x=%.2f (right edge=%.2f), wall left edge=%.2f\n", p.x, p.x + Config::PLAYER_WIDTH, wallLeftPx);
    CHECK(p.x + Config::PLAYER_WIDTH <= wallLeftPx + 0.5f, "player should not pass through the wall");
    CHECK(p.x + Config::PLAYER_WIDTH > wallLeftPx - 5.0f, "player should reach right up against the wall");
}

void test_jump_arc() {
    printf("\n[test_jump_arc]\n");
    World w = makeFlatWorld(40); // floor far below so the jump has room to arc
    Player p;
    p.spawnAt(50 * Config::TILE_SIZE, 35 * Config::TILE_SIZE);

    PlayerInput input{};
    for (int i = 0; i < 10; i++) p.update(1.0f / 60.0f, w, input); // settle onto ground first... actually floor is far, let's just check jump physics in freefall context instead
    // Re-spawn right on a floor for a clean jump test
    World w2 = makeFlatWorld(20);
    p.spawnAt(50 * Config::TILE_SIZE, 19 * Config::TILE_SIZE);
    for (int i = 0; i < 5; i++) p.update(1.0f / 60.0f, w2, input); // make sure grounded

    CHECK(p.onGround, "should be grounded before jump test");
    float startY = p.y;

    input.jumpPressed = true;
    p.update(1.0f / 60.0f, w2, input);
    input.jumpPressed = false;

    CHECK(p.vy < 0.0f, "vertical velocity should be negative (upward) right after jumping");
    CHECK(!p.onGround, "should leave the ground immediately after jumping");

    float minY = startY;
    for (int i = 0; i < 120; i++) {
        p.update(1.0f / 60.0f, w2, input);
        minY = std::min(minY, p.y);
    }
    printf("  startY=%.2f minY=%.2f (rose %.2f px) landed-again=%d\n", startY, minY, startY - minY, p.onGround);
    CHECK(minY < startY - 10.0f, "player should rise noticeably above the start height");
    CHECK(p.onGround, "player should have landed again after the jump arc completes");
}

void test_tunneling_prevention_under_lag_spike() {
    printf("\n[test_tunneling_prevention_under_lag_spike]\n");
    World w(100, 50, 1u);
    // thin 1-tile floor with open air below it (a void) to make tunneling failure obvious
    int floorY = 25;
    for (int x = 0; x < 100; x++) w.setTile(x, floorY, TileType::Stone);

    Player p;
    p.spawnAt(50 * Config::TILE_SIZE, (floorY - 5) * Config::TILE_SIZE);
    p.vy = 5000.0f; // simulate an extreme pre-existing fall speed

    PlayerInput input{};
    // Even though Player::update clamps dt internally, drive it with a
    // large requested dt (as if a lag spike produced one big frame) to
    // confirm the substep loop still resolves collision correctly.
    p.update(Config::MAX_FRAME_TIME, w, input);

    float floorTopPx = floorY * Config::TILE_SIZE;
    printf("  player.y=%.2f (feet=%.2f) floor top=%.2f\n", p.y, p.y + Config::PLAYER_HEIGHT, floorTopPx);
    CHECK(p.y + Config::PLAYER_HEIGHT <= floorTopPx + 0.5f, "player must not tunnel through a thin floor even at extreme velocity");
}

void test_coyote_time() {
    printf("\n[test_coyote_time]\n");
    World w(100, 50, 1u);
    int floorY = 20;
    for (int x = 0; x < 55; x++) w.setTile(x, floorY, TileType::Stone); // floor ends at x=55 (a ledge)

    Player p;
    p.spawnAt(50 * Config::TILE_SIZE, (floorY - 1) * Config::TILE_SIZE);
    PlayerInput input{};
    for (int i = 0; i < 10; i++) p.update(1.0f / 60.0f, w, input);
    CHECK(p.onGround, "should start grounded before walking off the ledge");

    input.right = true;
    bool leftGround = false;
    int framesAfterLeavingGround = -1;
    for (int i = 0; i < 60; i++) {
        p.update(1.0f / 60.0f, w, input);
        if (!leftGround && !p.onGround) { leftGround = true; framesAfterLeavingGround = 0; }
        else if (leftGround) framesAfterLeavingGround++;
        if (leftGround && framesAfterLeavingGround == 3) {
            // within coyote window (~0.1s = 6 frames at 60fps): jump should still work
            input.jumpPressed = true;
        } else {
            input.jumpPressed = false;
        }
        if (leftGround && framesAfterLeavingGround == 3) {
            float vyBefore = p.vy;
            (void)vyBefore;
        }
    }
    printf("  left ground=%d\n", leftGround);
    CHECK(leftGround, "player should walk off the ledge into the air");

    // Separate, more controlled run: jump exactly 3 frames after leaving ground.
    World w2(100, 50, 1u);
    for (int x = 0; x < 55; x++) w2.setTile(x, floorY, TileType::Stone);
    Player p2;
    p2.spawnAt(50 * Config::TILE_SIZE, (floorY - 1) * Config::TILE_SIZE);
    PlayerInput in2{};
    for (int i = 0; i < 10; i++) p2.update(1.0f / 60.0f, w2, in2);
    in2.right = true;
    int n = 0;
    while (p2.onGround && n < 200) { p2.update(1.0f / 60.0f, w2, in2); n++; }
    CHECK(!p2.onGround, "player2 should have left the ledge");
    for (int i = 0; i < 3; i++) p2.update(1.0f / 60.0f, w2, in2); // a few frames into coyote window
    in2.jumpPressed = true;
    float vyBeforeJump = p2.vy;
    p2.update(1.0f / 60.0f, w2, in2);
    printf("  vy before coyote-jump=%.1f, after=%.1f\n", vyBeforeJump, p2.vy);
    CHECK(p2.vy < -1.0f && p2.vy < vyBeforeJump, "coyote time should allow a jump shortly after leaving the ground");
}

void test_jump_buffer() {
    printf("\n[test_jump_buffer]\n");
    World w(100, 50, 1u);
    int floorY = 20;
    for (int x = 0; x < 100; x++) w.setTile(x, floorY, TileType::Stone);

    Player p;
    // start in the air, just about to land
    p.spawnAt(50 * Config::TILE_SIZE, (floorY - 1) * Config::TILE_SIZE - 2.0f);
    p.vy = 50.0f; // small downward speed, about to touch down

    PlayerInput input{};
    input.jumpPressed = true; // press jump slightly before actually landing
    p.update(1.0f / 60.0f, w, input);
    input.jumpPressed = false;

    bool jumped = false;
    for (int i = 0; i < 10 && !jumped; i++) {
        p.update(1.0f / 60.0f, w, input);
        if (p.vy < -1.0f) jumped = true;
    }
    printf("  jumped within buffer window=%d\n", jumped);
    CHECK(jumped, "a jump press just before landing should still trigger a jump once grounded (buffering)");
}

void test_mining_basic_and_tier_gating() {
    printf("\n[test_mining_basic_and_tier_gating]\n");
    World w(100, 50, 1u);
    int floorY = 20;
    for (int x = 0; x < 100; x++) w.setTile(x, floorY, TileType::Stone);
    w.setTile(52, floorY, TileType::Dirt);   // tier-0 minable
    w.setTile(53, floorY, TileType::IronOre); // needs tier 2

    Player p;
    p.spawnAt(50 * Config::TILE_SIZE, (floorY - 2) * Config::TILE_SIZE);
    Inventory inv;

    // Mining dirt with bare hands should eventually succeed.
    bool broke = false;
    for (int i = 0; i < 600 && !broke; i++) {
        broke = p.tryMine(w, inv, 52, floorY, 1.0f / 60.0f);
    }
    printf("  dirt broke with bare hands=%d, inv dirt count=%d\n", broke, inv.countItem(ItemId::Dirt));
    CHECK(broke, "bare hands should be able to mine dirt");
    CHECK(inv.countItem(ItemId::Dirt) == 1, "breaking dirt should give exactly 1 dirt item");
    CHECK(w.getTile(52, floorY) == TileType::Air, "mined tile should become air");

    // Iron ore should NOT break with bare hands (tier 2 required), even given lots of time.
    bool ironBroke = false;
    for (int i = 0; i < 600 && !ironBroke; i++) {
        ironBroke = p.tryMine(w, inv, 53, floorY, 1.0f / 60.0f);
    }
    CHECK(!ironBroke, "iron ore should NOT be mineable with bare hands");
    CHECK(w.getTile(53, floorY) == TileType::IronOre, "iron ore tile should remain unchanged");

    // Give the player an iron pickaxe (tier 3) and confirm it now works.
    inv.addItem(ItemId::PickaxeIron, 1);
    inv.selectSlot(1); // hotbar slot 1 - need to place the pickaxe there explicitly for the test
    // addItem puts it in the first empty slot, which for a fresh inventory with 1 dirt
    // already in slot 0 will be slot 1 - selecting slot 1 should select the pickaxe.
    CHECK(inv.selectedItem() == ItemId::PickaxeIron, "selected slot should hold the iron pickaxe for this check");

    bool ironBroke2 = false;
    for (int i = 0; i < 600 && !ironBroke2; i++) {
        ironBroke2 = p.tryMine(w, inv, 53, floorY, 1.0f / 60.0f);
    }
    printf("  iron broke with iron pickaxe=%d, inv iron count=%d\n", ironBroke2, inv.countItem(ItemId::Iron));
    CHECK(ironBroke2, "iron pickaxe (tier 3) should be able to mine iron ore (needs tier 2)");
    CHECK(inv.countItem(ItemId::Iron) == 1, "breaking iron ore should give exactly 1 iron item");
}

void test_reach_limit() {
    printf("\n[test_reach_limit]\n");
    World w(100, 50, 1u);
    int floorY = 20;
    for (int x = 0; x < 100; x++) w.setTile(x, floorY, TileType::Stone);
    w.setTile(90, floorY, TileType::Dirt); // far away from spawn at x=50

    Player p;
    p.spawnAt(50 * Config::TILE_SIZE, (floorY - 2) * Config::TILE_SIZE);
    Inventory inv;

    bool broke = false;
    for (int i = 0; i < 600 && !broke; i++) broke = p.tryMine(w, inv, 90, floorY, 1.0f / 60.0f);
    CHECK(!broke, "tile far outside reach distance should never be mineable");
    CHECK(!p.inReach(90, floorY), "inReach() should report false for a tile this far away");
    CHECK(p.inReach(51, floorY), "inReach() should report true for an adjacent tile");
}

void test_place_block() {
    printf("\n[test_place_block]\n");
    World w(100, 50, 1u);
    int floorY = 20;
    for (int x = 0; x < 100; x++) w.setTile(x, floorY, TileType::Stone);

    Player p;
    p.spawnAt(50 * Config::TILE_SIZE, (floorY - 3) * Config::TILE_SIZE);
    Inventory inv;
    inv.addItem(ItemId::Stone, 5);
    inv.selectSlot(0);
    CHECK(inv.selectedItem() == ItemId::Stone, "stone should be selected");

    bool placed = p.tryPlace(w, inv, 51, floorY - 1); // empty air tile near the player, not overlapping
    printf("  placed=%d remaining stone=%d tile now=%s\n", placed, inv.countItem(ItemId::Stone), getTileInfo(w.getTile(51, floorY - 1)).name);
    CHECK(placed, "should be able to place stone into empty reachable air");
    CHECK(inv.countItem(ItemId::Stone) == 4, "placing should consume exactly one stone from inventory");
    CHECK(w.getTile(51, floorY - 1) == TileType::Stone, "target tile should now be stone");

    // can't place on top of the already-solid floor tile
    bool placedOnSolid = p.tryPlace(w, inv, 51, floorY);
    CHECK(!placedOnSolid, "should not be able to place into an already-occupied solid tile");

    // can't place a block overlapping the player's own AABB
    AABB box = p.getAABB();
    int selfTileX = static_cast<int>(box.x / Config::TILE_SIZE);
    int selfTileY = static_cast<int>(box.y / Config::TILE_SIZE);
    bool placedOnSelf = p.tryPlace(w, inv, selfTileX, selfTileY);
    CHECK(!placedOnSelf, "should not be able to wall yourself in by placing on your own tile");
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    test_gravity_and_landing();
    test_wall_collision_no_tunnel();
    test_jump_arc();
    test_tunneling_prevention_under_lag_spike();
    test_coyote_time();
    test_jump_buffer();
    test_mining_basic_and_tier_gating();
    test_reach_limit();
    test_place_block();

    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
