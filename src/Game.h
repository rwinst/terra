#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include "World.h"
#include "Player.h"
#include "Inventory.h"
#include "Enemy.h"
#include "Particle.h"
#include "Color.h"

struct Toast {
    std::string text;
    float timer;
    float maxTimer;
};

class Game {
public:
    Game();
    ~Game();

    bool init();
    void run();

private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool running = false;

    World world;
    Player player;
    Inventory inventory;
    EnemyManager enemyManager;
    ParticleSystem particles;

    float dayTime = 0.0f;
    float camX = 0.0f, camY = 0.0f;

    bool inventoryOpen = false;
    bool helpOpen = true;

    bool mouseLeftDown = false;
    bool mouseRightDown = false;
    int mouseX = 0, mouseY = 0;
    bool prevJumpKeyHeld = false;

    float attackCooldown = 0.0f;
    float placeCooldown  = 0.0f;

    std::vector<Toast> toasts;
    Zone lastZone = Zone::Surface;
    bool wasNight = false;

    static const std::string kSavePath;

    float dayPhase01() const; // 1 = full daylight, 0 = full night

    // ---- setup ----
    void newWorld();
    void loadOrNewWorld();

    // ---- main loop phases ----
    void handleEvents();
    void handleKeyDown(const SDL_Keysym& keysym);
    void handleMouseDown(int button, int mx, int my);
    void handleMouseUp(int button);
    void update(float dt);
    void updateCamera(float dt);
    void render();

    // ---- world/entity rendering (camera space) ----
    void renderWorldTiles();
    void renderMiningProgress();
    void renderParticles();
    void renderEnemies();
    void renderPlayer();

    // ---- UI rendering (screen space) ----
    void renderUI();
    void renderHotbar();
    void renderHealthBar();
    void renderToasts();
    void renderHelpOverlay();
    void renderInventoryPanel();
    void renderZoneLabel();

    // ---- drawing primitives ----
    void fillRect(int x, int y, int w, int h, Color c);
    void drawRectOutline(int x, int y, int w, int h, Color c);
    void drawText(const std::string& s, int x, int y, int scale, Color c);
    void drawItemIcon(ItemId id, int x, int y, int size);

    // ---- helpers ----
    int worldToScreenX(float wx) const { return static_cast<int>(wx - camX); }
    int worldToScreenY(float wy) const { return static_cast<int>(wy - camY); }
    void getTileUnderMouse(int& tx, int& ty) const;
    void pushToast(const std::string& text);
    void doSave();
    bool tryCraftAtPanelPosition(int mx, int my);
};
