#include "Enemy.h"
#include "Player.h"
#include "World.h"
#include "Config.h"
#include <cstdio>
#include <cmath>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

World makeFlatWorld(int floorY) {
    World w(200, 60, 1u);
    for (int x = 0; x < 200; x++)
        for (int y = floorY; y < 60; y++)
            w.setTile(x, y, TileType::Stone);
    return w;
}

void test_enemy_gravity() {
    printf("\n[test_enemy_gravity]\n");
    World w = makeFlatWorld(30);
    // CaveCrawler has no hop behavior, so it should settle and stay grounded -
    // a cleaner check than Slime, which intentionally bounces periodically.
    Enemy e(EnemyKind::CaveCrawler, 50 * 32.0f, 5 * 32.0f);
    for (int i = 0; i < 300; i++) e.update(1.0f / 60.0f, w, -99999.0f, -99999.0f);
    printf("  e.y=%.2f onGround=%d\n", e.y, e.onGround);
    CHECK(e.onGround, "a non-hopping enemy should land on the floor under gravity and stay there");
}

void test_slime_hops() {
    printf("\n[test_slime_hops]\n");
    World w = makeFlatWorld(30);
    Enemy e(EnemyKind::Slime, 50 * 32.0f, 5 * 32.0f);
    bool everAirborneAfterFirstLanding = false;
    bool everGrounded = false;
    for (int i = 0; i < 600; i++) { // 10 simulated seconds, comfortably more than one hop period
        e.update(1.0f / 60.0f, w, -99999.0f, -99999.0f);
        if (e.onGround) everGrounded = true;
        else if (everGrounded) everAirborneAfterFirstLanding = true; // back in the air after having touched down once
    }
    printf("  ever grounded=%d, airborne again after landing (i.e. hopped)=%d\n", everGrounded, everAirborneAfterFirstLanding);
    CHECK(everGrounded, "slime should land on the ground at least once");
    CHECK(everAirborneAfterFirstLanding, "slime should hop back into the air periodically once grounded (its signature behavior)");
}

void test_enemy_chase_behavior() {
    printf("\n[test_enemy_chase_behavior]\n");
    World w = makeFlatWorld(30);
    Enemy e(EnemyKind::CaveCrawler, 50 * 32.0f, 29 * 32.0f - 28.0f);
    for (int i = 0; i < 10; i++) e.update(1.0f / 60.0f, w, 50 * 32.0f, 29 * 32.0f); // settle
    float startX = e.x;
    float targetX = startX + 2000.0f; // far to the right, within aggro range used in test
    for (int i = 0; i < 60; i++) e.update(1.0f / 60.0f, w, targetX, e.y);
    printf("  startX=%.2f afterX=%.2f (target was to the right)\n", startX, e.x);
    CHECK(e.x > startX, "enemy should move toward a target within aggro range");
}

void test_combat_damage_and_death() {
    printf("\n[test_combat_damage_and_death]\n");
    Enemy e(EnemyKind::Slime, 0, 0);
    float maxHp = e.maxHealth;
    e.takeDamage(10.0f);
    CHECK(std::fabs(e.health - (maxHp - 10.0f)) < 0.01f, "takeDamage should reduce health by the given amount");
    CHECK(!e.isDead(), "enemy with remaining health should not be dead");
    e.takeDamage(10000.0f);
    CHECK(e.health == 0.0f, "health should clamp at zero, not go negative");
    CHECK(e.isDead(), "enemy at zero health should be dead");
}

void test_contact_damage_has_cooldown() {
    printf("\n[test_contact_damage_has_cooldown]\n");
    World w = makeFlatWorld(30);
    Player p;
    p.spawnAt(50 * 32.0f, 29 * 32.0f - Config::PLAYER_HEIGHT);
    EnemyManager mgr;
    // place an enemy directly overlapping the player
    mgr.enemies.emplace_back(EnemyKind::Slime, p.x, p.y);

    float startHealth = p.health;
    // run several frames - even though contact persists every frame, damage should
    // only apply once per cooldown window, not every single frame
    for (int i = 0; i < 30; i++) { // 0.5s at 60fps - cooldown is also 0.5s
        mgr.update(1.0f / 60.0f, w, p, 1.0f);
    }
    float damageTaken = startHealth - p.health;
    printf("  damage taken over 0.5s of continuous contact = %.2f (single hit should be %d)\n", damageTaken, mgr.enemies.empty() ? 0 : 8);
    CHECK(damageTaken > 0.0f, "player should take damage from a touching enemy");
    CHECK(damageTaken < 16.0f, "contact damage should be rate-limited by the cooldown, not applied every frame");
}

void test_spawn_manager_respects_max_cap() {
    printf("\n[test_spawn_manager_respects_max_cap]\n");
    World w = makeFlatWorld(40);
    Player p;
    p.spawnAt(100 * 32.0f, 39 * 32.0f - Config::PLAYER_HEIGHT);
    EnemyManager mgr;

    // run a long simulated time at night (dayPhase01=0) so spawns keep triggering
    for (int i = 0; i < 60 * 200; i++) { // ~200 simulated seconds
        mgr.update(1.0f / 60.0f, w, p, 0.0f);
    }
    printf("  enemies alive after extended night simulation = %zu (cap = %d)\n", mgr.enemies.size(), Config::MAX_ENEMIES_ALIVE);
    CHECK(static_cast<int>(mgr.enemies.size()) <= Config::MAX_ENEMIES_ALIVE, "enemy count should never exceed the configured cap");
}

void test_no_spawns_during_day_on_surface() {
    printf("\n[test_no_spawns_during_day_on_surface]\n");
    World w = makeFlatWorld(40);
    Player p;
    p.spawnAt(100 * 32.0f, 39 * 32.0f - Config::PLAYER_HEIGHT);
    // Force player's tile position to read as "Surface" zone: floorY=40 is
    // below ZONE_SURFACE_END(60) so this world is entirely "Surface" zone by
    // our absolute-row zoning, good for this test.
    EnemyManager mgr;
    for (int i = 0; i < 60 * 60; i++) { // 60 sim-seconds, full daylight throughout
        mgr.update(1.0f / 60.0f, w, p, 1.0f); // dayPhase01=1 means full day, never below night threshold
    }
    printf("  enemies spawned during 60s of full daylight on the surface = %zu\n", mgr.enemies.size());
    CHECK(mgr.enemies.empty(), "no enemies should spawn on the surface during full daylight");
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    test_enemy_gravity();
    test_slime_hops();
    test_enemy_chase_behavior();
    test_combat_damage_and_death();
    test_contact_damage_has_cooldown();
    test_spawn_manager_respects_max_cap();
    test_no_spawns_during_day_on_surface();
    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
