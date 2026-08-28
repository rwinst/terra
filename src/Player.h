#pragma once
#include "TileType.h"

class World;
class Inventory; // fwd-declared; full Inventory.h only needed in Player.cpp

struct AABB {
    float x, y, w, h;
    bool intersects(const AABB& o) const {
        return x < o.x + o.w && x + w > o.x && y < o.y + o.h && y + h > o.y;
    }
};

// Edge-triggered + held input for one frame, filled in by the platform layer.
struct PlayerInput {
    bool left = false;
    bool right = false;
    bool jumpPressed = false; // true only on the frame the key/button went down
    bool jumpHeld = false;
};

class Player {
public:
    float x = 0, y = 0;   // top-left of the AABB, pixels
    float vx = 0, vy = 0; // px/s
    bool onGround = false;
    bool facingRight = true;

    float health;
    float maxHealth;
    float invulnTimer = 0.0f;

    // Mining state, exposed so the UI can draw a progress indicator.
    int targetTileX = -1, targetTileY = -1;
    float miningProgress = 0.0f;

    Player();

    void spawnAt(float px, float py);
    void update(float dt, const World& world, const PlayerInput& input);

    AABB getAABB() const { return { x, y, kWidth, kHeight }; }

    bool inReach(int tileX, int tileY) const;
    // Call every frame the mouse is held over a tile. Returns true the
    // instant the tile actually breaks (caller should add the dropped
    // item to inventory's tile and spawn break particles then).
    bool tryMine(World& world, Inventory& inv, int tileX, int tileY, float dt);
    bool tryPlace(World& world, Inventory& inv, int tileX, int tileY);

    void takeDamage(float amount);
    bool isDead() const { return health <= 0.0f; }
    bool isInvulnerable() const { return invulnTimer > 0.0f; }

private:
    static constexpr float kWidth = 22.0f;
    static constexpr float kHeight = 46.0f;

    float coyoteTimer = 0.0f;
    float jumpBufferTimer = 0.0f;

    void moveAxis(float delta, bool horizontal, const World& world);
};
