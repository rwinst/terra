#pragma once

// All the numbers that shape how the game feels live here so they're easy
// to find and tweak without hunting through gameplay code.
namespace Config {
    // ---- Window / rendering ----
    constexpr int WINDOW_WIDTH  = 1280;
    constexpr int WINDOW_HEIGHT = 720;
    constexpr int TILE_SIZE     = 32; // pixels per tile, world and screen space both use this

    // ---- World generation ----
    constexpr int WORLD_WIDTH   = 400; // tiles
    constexpr int WORLD_HEIGHT  = 160; // tiles
    constexpr int SURFACE_BASE_HEIGHT = 42; // average row index of the ground surface
    constexpr int SURFACE_AMPLITUDE   = 14; // max +/- variation from noise

    // Zone boundaries (row index, depth increases downward) - purely cosmetic
    // labels/flavor but also loosely line up with where resources get rarer.
    constexpr int ZONE_SURFACE_END     = 60;
    constexpr int ZONE_UNDERGROUND_END = 95;
    constexpr int ZONE_CAVERNS_END     = 130;
    // anything below ZONE_CAVERNS_END is "The Deep"

    constexpr int BEDROCK_ROWS = 3; // unbreakable rows at the very bottom

    // ---- Physics ----
    constexpr float GRAVITY            = 1800.0f;  // px/s^2
    constexpr float MAX_FALL_SPEED     = 1300.0f;  // px/s terminal velocity
    constexpr float MOVE_SPEED         = 230.0f;   // px/s max horizontal speed
    constexpr float GROUND_ACCEL       = 2200.0f;  // px/s^2
    constexpr float AIR_ACCEL          = 1100.0f;  // px/s^2 (less control mid-air)
    constexpr float GROUND_FRICTION    = 1900.0f;  // px/s^2 deceleration when no input
    constexpr float JUMP_VELOCITY      = -700.0f;  // px/s, negative = up
    constexpr float COYOTE_TIME        = 0.10f;    // seconds you can still jump after leaving ground
    constexpr float JUMP_BUFFER_TIME   = 0.12f;    // seconds a jump press is remembered before landing

    constexpr float PLAYER_WIDTH  = 22.0f;
    constexpr float PLAYER_HEIGHT = 46.0f;

    constexpr float PHYSICS_SUBSTEP = 1.0f / 180.0f; // fixed sub-step to avoid tunneling at low fps
    constexpr float MAX_FRAME_TIME  = 0.05f;          // clamp huge dt spikes (e.g. window unfocus)

    // ---- Player / interaction ----
    constexpr float REACH_TILES        = 5.5f;
    constexpr float PLAYER_MAX_HEALTH  = 100.0f;
    constexpr float PLAYER_INVULN_TIME = 0.6f; // seconds of invulnerability after taking damage

    // ---- Inventory ----
    constexpr int HOTBAR_SIZE   = 10;
    constexpr int BACKPACK_SIZE = 20;
    constexpr int INVENTORY_SIZE = HOTBAR_SIZE + BACKPACK_SIZE;
    constexpr int MAX_STACK     = 999;

    // ---- Day / night ----
    // Real Terraria runs ~24 real-world minutes per day; that's far too
    // slow for a quick demo session, so this is intentionally sped up.
    // Tune this up if you want a slower, calmer cycle.
    constexpr float DAY_LENGTH_SECONDS = 180.0f;
    constexpr float NIGHT_FRACTION     = 0.45f; // fraction of the cycle that is night

    // ---- Enemies ----
    constexpr float ENEMY_SPAWN_INTERVAL_MIN = 4.0f;
    constexpr float ENEMY_SPAWN_INTERVAL_MAX = 9.0f;
    constexpr int   MAX_ENEMIES_ALIVE        = 12;
    constexpr float ENEMY_SPAWN_MIN_DIST     = 8.0f;  // tiles from player
    constexpr float ENEMY_SPAWN_MAX_DIST     = 22.0f; // tiles from player
    constexpr float ENEMY_CONTACT_DAMAGE_COOLDOWN = 0.5f;

    // ---- Lighting ----
    constexpr float LIGHT_FALLOFF_AIR   = 0.06f; // light lost per tile travelled through open air
    constexpr float LIGHT_FALLOFF_SOLID = 0.32f; // light lost per tile travelled through solid blocks
    constexpr float AMBIENT_CAVE_LIGHT  = 0.045f; // floor so caves are dim, not pure black
    constexpr float TORCH_LIGHT         = 1.0f;
    constexpr int   LIGHT_UPDATE_RADIUS = 40; // tiles, local recompute window around an edit
}
