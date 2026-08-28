#include "Player.h"
#include "World.h"
#include "Inventory.h"
#include "Config.h"
#include <algorithm>
#include <cmath>

Player::Player()
    : health(Config::PLAYER_MAX_HEALTH), maxHealth(Config::PLAYER_MAX_HEALTH)
{
}

void Player::spawnAt(float px, float py) {
    x = px - kWidth / 2.0f;
    y = py;
    vx = 0.0f;
    vy = 0.0f;
    onGround = false;
    health = maxHealth;
    invulnTimer = 0.0f;
    coyoteTimer = 0.0f;
    jumpBufferTimer = 0.0f;
    targetTileX = targetTileY = -1;
    miningProgress = 0.0f;
}

void Player::update(float dt, const World& world, const PlayerInput& input) {
    dt = std::min(dt, Config::MAX_FRAME_TIME);

    // ---- Horizontal acceleration / friction ----
    float dir = 0.0f;
    if (input.left) dir -= 1.0f;
    if (input.right) dir += 1.0f;
    if (dir > 0.0f) facingRight = true;
    else if (dir < 0.0f) facingRight = false;

    float accel = onGround ? Config::GROUND_ACCEL : Config::AIR_ACCEL;
    if (dir != 0.0f) {
        vx += dir * accel * dt;
        vx = std::clamp(vx, -Config::MOVE_SPEED, Config::MOVE_SPEED);
    } else {
        float fric = Config::GROUND_FRICTION * dt;
        if (vx > 0.0f) vx = std::max(0.0f, vx - fric);
        else if (vx < 0.0f) vx = std::min(0.0f, vx + fric);
    }

    // ---- Jump buffering + coyote time (read last frame's onGround first) ----
    jumpBufferTimer = input.jumpPressed ? Config::JUMP_BUFFER_TIME
                                         : std::max(0.0f, jumpBufferTimer - dt);
    coyoteTimer = onGround ? Config::COYOTE_TIME
                            : std::max(0.0f, coyoteTimer - dt);

    if (jumpBufferTimer > 0.0f && coyoteTimer > 0.0f) {
        vy = Config::JUMP_VELOCITY;
        coyoteTimer = 0.0f;
        jumpBufferTimer = 0.0f;
        onGround = false;
    }

    // ---- Gravity ----
    vy += Config::GRAVITY * dt;
    vy = std::min(vy, Config::MAX_FALL_SPEED);

    // ---- Sub-stepped movement + collision (prevents tunneling at low fps) ----
    onGround = false;
    float remaining = dt;
    while (remaining > 1e-6f) {
        float step = std::min(remaining, Config::PHYSICS_SUBSTEP);
        moveAxis(vx * step, /*horizontal=*/true, world);
        moveAxis(vy * step, /*horizontal=*/false, world);
        remaining -= step;
    }

    invulnTimer = std::max(0.0f, invulnTimer - dt);
}

void Player::moveAxis(float delta, bool horizontal, const World& world) {
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
        if (horizontal) {
            x = corrected;
            vx = 0.0f;
        } else {
            y = corrected;
            vy = 0.0f;
            if (delta > 0.0f) onGround = true; // only landing on top counts as grounded
        }
    }
}

bool Player::inReach(int tileX, int tileY) const {
    AABB box = getAABB();
    float cx = box.x + box.w / 2.0f;
    float cy = box.y + box.h / 2.0f;
    float tcx = tileX * static_cast<float>(Config::TILE_SIZE) + Config::TILE_SIZE / 2.0f;
    float tcy = tileY * static_cast<float>(Config::TILE_SIZE) + Config::TILE_SIZE / 2.0f;
    float dx = tcx - cx, dy = tcy - cy;
    float maxDist = Config::REACH_TILES * Config::TILE_SIZE;
    return (dx * dx + dy * dy) <= (maxDist * maxDist);
}

bool Player::tryMine(World& world, Inventory& inv, int tileX, int tileY, float dt) {
    if (!inReach(tileX, tileY)) {
        targetTileX = targetTileY = -1;
        miningProgress = 0.0f;
        return false;
    }

    TileType t = world.getTile(tileX, tileY);
    const TileInfo& info = getTileInfo(t);
    if (!info.minable) {
        targetTileX = targetTileY = -1;
        miningProgress = 0.0f;
        return false;
    }

    int toolTier = 0;
    float speedMul = 1.0f;
    ItemId selected = inv.selectedItem();
    if (selected != ItemId::None) {
        const ItemInfo& ii = getItemInfo(selected);
        if (ii.isPickaxe) {
            toolTier = ii.toolTier;
            speedMul = ii.miningSpeedMul;
        }
    }

    if (info.minToolTier > toolTier) {
        // Can't dent this with the current tool - don't accumulate progress.
        targetTileX = targetTileY = -1;
        miningProgress = 0.0f;
        return false;
    }

    if (tileX != targetTileX || tileY != targetTileY) {
        targetTileX = tileX;
        targetTileY = tileY;
        miningProgress = 0.0f;
    }

    miningProgress += dt * speedMul;
    if (miningProgress >= info.hardness) {
        ItemId drop = tileDropItem(t);
        world.setTile(tileX, tileY, TileType::Air);
        world.recomputeLightingRegion(tileX, tileY, Config::LIGHT_UPDATE_RADIUS);
        if (drop != ItemId::None) inv.addItem(drop, 1);
        targetTileX = targetTileY = -1;
        miningProgress = 0.0f;
        return true;
    }
    return false;
}

bool Player::tryPlace(World& world, Inventory& inv, int tileX, int tileY) {
    if (!inReach(tileX, tileY)) return false;

    ItemId sel = inv.selectedItem();
    if (sel == ItemId::None) return false;

    const ItemInfo& ii = getItemInfo(sel);
    if (!ii.placeable) return false;
    if (world.getTile(tileX, tileY) != TileType::Air) return false;

    float ts = static_cast<float>(Config::TILE_SIZE);
    AABB tileBox{ tileX * ts, tileY * ts, ts, ts };
    if (getAABB().intersects(tileBox)) return false; // don't let the player wall themselves in

    if (!inv.removeItem(sel, 1)) return false;

    world.setTile(tileX, tileY, ii.placesTile);
    world.recomputeLightingRegion(tileX, tileY, Config::LIGHT_UPDATE_RADIUS);
    return true;
}

void Player::takeDamage(float amount) {
    if (invulnTimer > 0.0f) return;
    health = std::max(0.0f, health - amount);
    invulnTimer = Config::PLAYER_INVULN_TIME;
}
