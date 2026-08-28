#pragma once
#include "Player.h"
#include "Color.h"
#include <vector>
#include <cstdint>

class World;

enum class EnemyKind { Slime, CaveCrawler };

class Enemy {
public:
    EnemyKind kind;
    float x, y;
    float vx = 0, vy = 0;
    float health, maxHealth;
    bool onGround = false;
    bool facingRight = true;

    Enemy(EnemyKind k, float spawnX, float spawnY);

    void update(float dt, const World& world, float playerCenterX, float playerCenterY);
    AABB getAABB() const;
    void takeDamage(float dmg) { health -= dmg; if (health < 0.0f) health = 0.0f; }
    bool isDead() const { return health <= 0.0f; }

    Color renderColor() const;
    int contactDamage() const;
    float width() const;
    float height() const;

    float damageCooldown = 0.0f; // managed by EnemyManager's contact-damage check

private:
    float patrolDir = 1.0f;
    float aiTimer = 1.5f;
    float hopTimer = 1.0f;
    uint32_t rngState;

    float nextRand01();
    void moveAxis(float delta, bool horizontal, const World& world);
};

// Owns the enemy list, spawn timer, and the day/depth based spawn rules.
class EnemyManager {
public:
    std::vector<Enemy> enemies;

    // dayPhase01: 1 = full day, 0 = full night (matches World::getBackgroundColor's convention)
    void update(float dt, const World& world, Player& player, float dayPhase01);

    void removeDead();

private:
    float spawnTimer = 3.0f;
    uint32_t rngState = 0x9e3779b9u; // simple xorshift, no <random> needed for spawn rolls
    float nextRand01();
    void trySpawn(const World& world, const Player& player, float dayPhase01);
};
