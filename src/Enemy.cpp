#include "Enemy.h"
#include "World.h"
#include "Config.h"
#include <cmath>
#include <algorithm>

namespace {
    uint32_t xorshift32(uint32_t& state) {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }
    float rand01(uint32_t& state) {
        return (xorshift32(state) & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
    }
    uint32_t hashSeed(float a, float b) {
        uint32_t h = static_cast<uint32_t>(a * 7919) ^ (static_cast<uint32_t>(b * 104729) << 1);
        return h == 0 ? 0x9e3779b9u : h;
    }
}

Enemy::Enemy(EnemyKind k, float spawnX, float spawnY)
    : kind(k), x(spawnX), y(spawnY), rngState(hashSeed(spawnX, spawnY))
{
    if (kind == EnemyKind::Slime) { maxHealth = 30.0f; }
    else { maxHealth = 50.0f; }
    health = maxHealth;
}

float Enemy::nextRand01() { return rand01(rngState); }

float Enemy::width() const  { return (kind == EnemyKind::Slime) ? 26.0f : 30.0f; }
float Enemy::height() const { return (kind == EnemyKind::Slime) ? 22.0f : 28.0f; }

AABB Enemy::getAABB() const { return { x, y, width(), height() }; }

Color Enemy::renderColor() const {
    return (kind == EnemyKind::Slime) ? Color(90, 200, 110) : Color(150, 60, 130);
}

int Enemy::contactDamage() const { return (kind == EnemyKind::Slime) ? 8 : 14; }

void Enemy::moveAxis(float delta, bool horizontal, const World& world) {
    if (delta == 0.0f) return;
    if (horizontal) x += delta; else y += delta;

    AABB box = getAABB();
    const int ts = Config::TILE_SIZE;
    const float eps = 0.01f;
    int minTX = static_cast<int>(std::floor(box.x / ts));
    int maxTX = static_cast<int>(std::floor((box.x + box.w - eps) / ts));
    int minTY = static_cast<int>(std::floor(box.y / ts));
    int maxTY = static_cast<int>(std::floor((box.y + box.h - eps) / ts));

    bool collided = false;
    float corrected = horizontal ? x : y;
    for (int ty = minTY; ty <= maxTY; ty++) {
        for (int tx = minTX; tx <= maxTX; tx++) {
            if (!world.isSolid(tx, ty)) continue;
            collided = true;
            if (horizontal) {
                if (delta > 0.0f) corrected = std::min(corrected, tx * static_cast<float>(ts) - box.w);
                else               corrected = std::max(corrected, (tx + 1) * static_cast<float>(ts));
            } else {
                if (delta > 0.0f) corrected = std::min(corrected, ty * static_cast<float>(ts) - box.h);
                else               corrected = std::max(corrected, (ty + 1) * static_cast<float>(ts));
            }
        }
    }
    if (collided) {
        if (horizontal) { x = corrected; vx = 0.0f; }
        else {
            y = corrected;
            vy = 0.0f;
            if (delta > 0.0f) onGround = true;
        }
    }
}

void Enemy::update(float dt, const World& world, float playerCenterX, float playerCenterY) {
    AABB box = getAABB();
    float centerX = box.x + box.w / 2.0f;
    float centerY = box.y + box.h / 2.0f;
    float dx = playerCenterX - centerX;
    float dy = playerCenterY - centerY;
    float dist = std::sqrt(dx * dx + dy * dy);

    float aggroRange = (kind == EnemyKind::Slime) ? 260.0f : 320.0f;
    float speed = (kind == EnemyKind::Slime) ? 70.0f : 115.0f;

    int desiredDir;
    if (dist < aggroRange) {
        desiredDir = (dx >= 0.0f) ? 1 : -1;
    } else {
        aiTimer -= dt;
        if (aiTimer <= 0.0f) {
            patrolDir = -patrolDir;
            aiTimer = 1.5f + nextRand01() * 2.5f;
        }
        desiredDir = (patrolDir > 0.0f) ? 1 : -1;
    }
    facingRight = desiredDir > 0;
    vx = desiredDir * speed;

    if (kind == EnemyKind::Slime && onGround) {
        hopTimer -= dt;
        if (hopTimer <= 0.0f) {
            vy = -340.0f;
            hopTimer = 0.6f + nextRand01() * 1.2f;
        }
    }

    vy += Config::GRAVITY * dt;
    vy = std::min(vy, Config::MAX_FALL_SPEED);

    onGround = false;
    float remaining = std::min(dt, Config::MAX_FRAME_TIME);
    while (remaining > 1e-6f) {
        float step = std::min(remaining, Config::PHYSICS_SUBSTEP);
        moveAxis(vx * step, true, world);
        moveAxis(vy * step, false, world);
        remaining -= step;
    }

    damageCooldown = std::max(0.0f, damageCooldown - dt);
}

// =================================================================
// EnemyManager
// =================================================================

float EnemyManager::nextRand01() { return rand01(rngState); }

void EnemyManager::trySpawn(const World& world, const Player& player, float dayPhase01) {
    if (static_cast<int>(enemies.size()) >= Config::MAX_ENEMIES_ALIVE) return;

    AABB pBox = player.getAABB();
    int playerTileX = static_cast<int>((pBox.x + pBox.w / 2.0f) / Config::TILE_SIZE);
    int playerTileY = static_cast<int>((pBox.y + pBox.h / 2.0f) / Config::TILE_SIZE);

    bool playerUnderground = world.zoneAt(playerTileY) != Zone::Surface;
    bool isNight = dayPhase01 < (1.0f - Config::NIGHT_FRACTION);

    if (playerUnderground) {
        // Try a handful of candidate dark spots near the player's depth.
        for (int attempt = 0; attempt < 8; attempt++) {
            float sign = (nextRand01() < 0.5f) ? -1.0f : 1.0f;
            float distTiles = Config::ENEMY_SPAWN_MIN_DIST + nextRand01() * (Config::ENEMY_SPAWN_MAX_DIST - Config::ENEMY_SPAWN_MIN_DIST);
            int cx = playerTileX + static_cast<int>(sign * distTiles);
            int cy = playerTileY + static_cast<int>((nextRand01() - 0.5f) * 10.0f);

            if (world.getTile(cx, cy) != TileType::Air) continue;
            if (world.getLight(cx, cy) > 0.30f) continue; // needs to be genuinely dark

            float spawnPx = cx * static_cast<float>(Config::TILE_SIZE);
            float spawnPy = cy * static_cast<float>(Config::TILE_SIZE);
            enemies.emplace_back(EnemyKind::CaveCrawler, spawnPx, spawnPy);
            return;
        }
    } else if (isNight) {
        float sign = (nextRand01() < 0.5f) ? -1.0f : 1.0f;
        float distTiles = Config::ENEMY_SPAWN_MIN_DIST + nextRand01() * (Config::ENEMY_SPAWN_MAX_DIST - Config::ENEMY_SPAWN_MIN_DIST);
        int cx = playerTileX + static_cast<int>(sign * distTiles);
        int surf = world.surfaceHeight(cx);
        float spawnPx = cx * static_cast<float>(Config::TILE_SIZE);
        float spawnPy = (surf - 3) * static_cast<float>(Config::TILE_SIZE);
        enemies.emplace_back(EnemyKind::Slime, spawnPx, spawnPy);
    }
}

void EnemyManager::update(float dt, const World& world, Player& player, float dayPhase01) {
    spawnTimer -= dt;
    if (spawnTimer <= 0.0f) {
        trySpawn(world, player, dayPhase01);
        spawnTimer = Config::ENEMY_SPAWN_INTERVAL_MIN +
            nextRand01() * (Config::ENEMY_SPAWN_INTERVAL_MAX - Config::ENEMY_SPAWN_INTERVAL_MIN);
    }

    AABB pBox = player.getAABB();
    float playerCenterX = pBox.x + pBox.w / 2.0f;
    float playerCenterY = pBox.y + pBox.h / 2.0f;

    for (auto& e : enemies) {
        if (e.isDead()) continue;
        e.update(dt, world, playerCenterX, playerCenterY);

        if (e.damageCooldown <= 0.0f && e.getAABB().intersects(player.getAABB())) {
            player.takeDamage(static_cast<float>(e.contactDamage()));
            e.damageCooldown = Config::ENEMY_CONTACT_DAMAGE_COOLDOWN;
        }
    }

    removeDead();
}

void EnemyManager::removeDead() {
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
        [](const Enemy& e) { return e.isDead(); }), enemies.end());
}
