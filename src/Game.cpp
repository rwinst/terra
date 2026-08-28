#include "Game.h"
#include "BitmapFont.h"
#include "SaveSystem.h"
#include "Config.h"
#include "Item.h"
#include "TileType.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

const std::string Game::kSavePath = "terra_save.dat";

// Crafting panel constants — must match between renderInventoryPanel and tryCraftAtPanelPosition.
static constexpr int CRAFT_X    = 650;
static constexpr int CRAFT_Y0   = 80;
static constexpr int RECIPE_H   = 60;

static SDL_Rect makeSDLRect(int x, int y, int w, int h) {
    SDL_Rect r{ x, y, std::max(1, w), std::max(1, h) };
    return r;
}

// ================================================================
//  Constructor / Destructor
// ================================================================

Game::Game() : world(Config::WORLD_WIDTH, Config::WORLD_HEIGHT, 42u) {}

Game::~Game() {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
    SDL_Quit();
}

// ================================================================
//  init() — SDL window + renderer
// ================================================================

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow(
        "Terra  –  A Terraria-Style Platformer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }
    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, 0); // fallback software
        if (!renderer) {
            fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
            return false;
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    return true;
}

// ================================================================
//  Day / night phase
// ================================================================

float Game::dayPhase01() const {
    // Returns 1 at noon, 0 at night. Smoothly transitions.
    float t = dayTime / Config::DAY_LENGTH_SECONDS;
    // Shift so t=0 is pre-dawn (dark), t=0.5 is noon (bright), t=1 is back to night.
    float raw = std::sin(2.0f * 3.14159265358979f * (t - 0.25f));
    return std::clamp(raw * 0.7f + 0.5f, 0.0f, 1.0f);
}

// ================================================================
//  World creation
// ================================================================

void Game::newWorld() {
    uint32_t seed = static_cast<uint32_t>(SDL_GetTicks64() ^ 0xDEADBEEFu);
    world = World(Config::WORLD_WIDTH, Config::WORLD_HEIGHT, seed);
    world.generate();

    player.spawnAt(static_cast<float>(world.spawnX()),
                   static_cast<float>(world.spawnY()));
    inventory = Inventory();
    inventory.addItem(ItemId::PickaxeStone, 1);
    inventory.addItem(ItemId::SwordWood,    1);
    inventory.addItem(ItemId::Wood,         8);
    inventory.addItem(ItemId::Torch,        6);
    inventory.selectSlot(0);

    dayTime = 0.0f;
    camX = player.x - Config::WINDOW_WIDTH  / 2.0f;
    camY = player.y - Config::WINDOW_HEIGHT / 2.0f;
    toasts.clear();
    enemyManager.enemies.clear();
    lastZone = Zone::Surface;
    wasNight  = false;
    pushToast("NEW WORLD — PRESS F1 FOR HELP");
}

void Game::loadOrNewWorld() {
    World    tempWorld(Config::WORLD_WIDTH, Config::WORLD_HEIGHT, 42u);
    Player   tempPlayer;
    Inventory tempInv;
    float    savedDay = 0.0f;

    if (SaveSystem::loadGame(kSavePath, tempWorld, tempPlayer, tempInv, savedDay)) {
        world     = std::move(tempWorld);
        player    = tempPlayer;
        inventory = tempInv;
        dayTime   = savedDay;
        camX = player.x - Config::WINDOW_WIDTH  / 2.0f;
        camY = player.y - Config::WINDOW_HEIGHT / 2.0f;
        int pty = static_cast<int>((player.y + Config::PLAYER_HEIGHT / 2.0f) / Config::TILE_SIZE);
        lastZone = world.zoneAt(pty);
        wasNight = dayPhase01() < (1.0f - Config::NIGHT_FRACTION);
        pushToast("SAVE LOADED");
    } else {
        newWorld();
    }
}

// ================================================================
//  Main loop
// ================================================================

void Game::run() {
    loadOrNewWorld();
    running = true;
    Uint64 lastTime = SDL_GetTicks64();

    while (running) {
        Uint64 now = SDL_GetTicks64();
        float dt = static_cast<float>(now - lastTime) / 1000.0f;
        lastTime = now;
        dt = std::min(dt, Config::MAX_FRAME_TIME);

        handleEvents();
        if (!running) break;
        update(dt);
        render();
    }
}

// ================================================================
//  Event handling
// ================================================================

void Game::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT: running = false; break;
            case SDL_KEYDOWN: handleKeyDown(e.key.keysym); break;
            case SDL_MOUSEBUTTONDOWN:
                handleMouseDown(e.button.button, e.button.x, e.button.y); break;
            case SDL_MOUSEBUTTONUP:
                handleMouseUp(e.button.button); break;
            case SDL_MOUSEWHEEL:
                if (!inventoryOpen) inventory.cycleSelected(-e.wheel.y);
                break;
            default: break;
        }
    }
    SDL_GetMouseState(&mouseX, &mouseY);
}

void Game::handleKeyDown(const SDL_Keysym& keysym) {
    switch (keysym.scancode) {
        case SDL_SCANCODE_ESCAPE: running = false;                    break;
        case SDL_SCANCODE_E:      inventoryOpen = !inventoryOpen;     break;
        case SDL_SCANCODE_F1:     helpOpen = !helpOpen;               break;
        case SDL_SCANCODE_F5:     doSave();                           break;
        case SDL_SCANCODE_1:  inventory.selectSlot(0); break;
        case SDL_SCANCODE_2:  inventory.selectSlot(1); break;
        case SDL_SCANCODE_3:  inventory.selectSlot(2); break;
        case SDL_SCANCODE_4:  inventory.selectSlot(3); break;
        case SDL_SCANCODE_5:  inventory.selectSlot(4); break;
        case SDL_SCANCODE_6:  inventory.selectSlot(5); break;
        case SDL_SCANCODE_7:  inventory.selectSlot(6); break;
        case SDL_SCANCODE_8:  inventory.selectSlot(7); break;
        case SDL_SCANCODE_9:  inventory.selectSlot(8); break;
        case SDL_SCANCODE_0:  inventory.selectSlot(9); break;
        default: break;
    }
}

void Game::handleMouseDown(int button, int mx, int my) {
    if (button == SDL_BUTTON_LEFT) {
        mouseLeftDown = true;
        if (inventoryOpen) tryCraftAtPanelPosition(mx, my);
    }
    if (button == SDL_BUTTON_RIGHT) mouseRightDown = true;
}

void Game::handleMouseUp(int button) {
    if (button == SDL_BUTTON_LEFT)  mouseLeftDown  = false;
    if (button == SDL_BUTTON_RIGHT) mouseRightDown = false;
}

// ================================================================
//  Update
// ================================================================

void Game::update(float dt) {
    // ---- Day/night clock ----
    dayTime += dt;
    if (dayTime >= Config::DAY_LENGTH_SECONDS) dayTime -= Config::DAY_LENGTH_SECONDS;

    bool isNight = dayPhase01() < (1.0f - Config::NIGHT_FRACTION);
    if (isNight != wasNight) {
        wasNight = isNight;
        pushToast(isNight ? "NIGHT FALLS..." : "A NEW DAY DAWNS");
    }

    // ---- Zone change notification ----
    int pty = static_cast<int>((player.y + Config::PLAYER_HEIGHT / 2.0f) / Config::TILE_SIZE);
    Zone curZone = world.zoneAt(pty);
    if (curZone != lastZone) {
        lastZone = curZone;
        pushToast(world.zoneName(curZone));
    }

    // ---- Toast timers ----
    for (auto& t : toasts) t.timer -= dt;
    toasts.erase(std::remove_if(toasts.begin(), toasts.end(),
        [](const Toast& t) { return t.timer <= 0.0f; }), toasts.end());

    // ---- Death + respawn ----
    if (player.isDead()) {
        player.spawnAt(static_cast<float>(world.spawnX()),
                       static_cast<float>(world.spawnY()));
        player.health = player.maxHealth;
        inventory = Inventory();
        inventory.addItem(ItemId::PickaxeStone, 1);
        inventory.addItem(ItemId::SwordWood,    1);
        inventory.addItem(ItemId::Wood,         5);
        inventory.addItem(ItemId::Torch,        4);
        pushToast("YOU DIED — RESPAWNED");
        enemyManager.enemies.clear();
    }

    // ---- Player input ----
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    PlayerInput input;
    input.left  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
    input.right = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
    bool jumpNow       = keys[SDL_SCANCODE_W]   ||
                         keys[SDL_SCANCODE_UP]   ||
                         keys[SDL_SCANCODE_SPACE];
    input.jumpPressed  = jumpNow && !prevJumpKeyHeld;
    input.jumpHeld     = jumpNow;
    prevJumpKeyHeld    = jumpNow;

    player.update(dt, world, input);

    // ---- World interaction (only when inventory closed) ----
    if (!inventoryOpen) {
        int tx, ty;
        getTileUnderMouse(tx, ty);

        if (mouseLeftDown) {
            // Capture tile color before it might be removed
            TileType prevTile   = world.getTile(tx, ty);
            Color    prevColor  = getTileInfo(prevTile).color;

            bool mined = player.tryMine(world, inventory, tx, ty, dt);
            if (mined) {
                particles.spawnBurst(
                    tx * Config::TILE_SIZE + Config::TILE_SIZE / 2.0f,
                    ty * Config::TILE_SIZE + Config::TILE_SIZE / 2.0f,
                    prevColor, 8);
            }

            // If the mouse isn't on a minable tile, try a melee swing
            if (player.targetTileX == -1 && attackCooldown <= 0.0f) {
                float cursorWX = mouseX + camX;
                float cursorWY = mouseY + camY;
                float pcx = player.x + Config::PLAYER_WIDTH  / 2.0f;
                float pcy = player.y + Config::PLAYER_HEIGHT / 2.0f;
                float reachPx = Config::REACH_TILES * Config::TILE_SIZE;

                ItemId sel = inventory.selectedItem();
                int dmg = 3; // bare hands
                if (sel != ItemId::None) {
                    const ItemInfo& ii = getItemInfo(sel);
                    if (ii.isSword) dmg = ii.meleeDamage;
                }

                for (auto& enemy : enemyManager.enemies) {
                    if (enemy.isDead()) continue;
                    AABB eb  = enemy.getAABB();
                    float ex = eb.x + eb.w / 2.0f;
                    float ey = eb.y + eb.h / 2.0f;
                    float cursorDistSq = (cursorWX - ex) * (cursorWX - ex) +
                                         (cursorWY - ey) * (cursorWY - ey);
                    float playerDistSq = (pcx - ex) * (pcx - ex) +
                                         (pcy - ey) * (pcy - ey);
                    float cs = reachPx * 0.65f;
                    if (cursorDistSq <= cs * cs && playerDistSq <= reachPx * reachPx) {
                        enemy.takeDamage(static_cast<float>(dmg));
                        float kbDir = (ex > pcx) ? 1.0f : -1.0f;
                        enemy.vx += kbDir * 200.0f;
                        enemy.vy -= 130.0f;
                        attackCooldown = (dmg > 3) ? 0.40f : 0.65f;
                        if (enemy.isDead()) {
                            particles.spawnBurst(ex, ey, enemy.renderColor(), 12);
                            pushToast("ENEMY SLAIN!");
                        }
                        break;
                    }
                }
            }
        }

        if (mouseRightDown) {
            placeCooldown -= dt;
            if (placeCooldown <= 0.0f) {
                if (player.tryPlace(world, inventory, tx, ty)) {
                    placeCooldown = 0.18f;
                }
            }
        }
    }

    attackCooldown = std::max(0.0f, attackCooldown - dt);
    placeCooldown  = std::max(0.0f, placeCooldown - dt);

    enemyManager.update(dt, world, player, dayPhase01());
    particles.update(dt);
    updateCamera(dt);
}

void Game::updateCamera(float dt) {
    float targetX = player.x + Config::PLAYER_WIDTH  / 2.0f - Config::WINDOW_WIDTH  / 2.0f;
    float targetY = player.y + Config::PLAYER_HEIGHT / 2.0f - Config::WINDOW_HEIGHT / 2.0f;
    float s = 7.0f * dt;
    camX += (targetX - camX) * s;
    camY += (targetY - camY) * s;
    float maxCX = world.width()  * Config::TILE_SIZE - Config::WINDOW_WIDTH;
    float maxCY = world.height() * Config::TILE_SIZE - Config::WINDOW_HEIGHT;
    camX = std::clamp(camX, 0.0f, maxCX);
    camY = std::clamp(camY, -static_cast<float>(Config::WINDOW_HEIGHT) * 0.3f, maxCY);
}

// ================================================================
//  Top-level render
// ================================================================

void Game::render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    renderWorldTiles();
    renderMiningProgress();
    renderParticles();
    renderEnemies();
    renderPlayer();
    renderUI();

    SDL_RenderPresent(renderer);
}

// ================================================================
//  World tile rendering
// ================================================================

void Game::renderWorldTiles() {
    const int ts  = Config::TILE_SIZE;
    float     day = dayPhase01();

    int minTX = std::max(0, static_cast<int>(camX / ts) - 1);
    int maxTX = std::min(world.width()  - 1, static_cast<int>((camX + Config::WINDOW_WIDTH)  / ts) + 1);
    int minTY = std::max(0, static_cast<int>(camY / ts) - 1);
    int maxTY = std::min(world.height() - 1, static_cast<int>((camY + Config::WINDOW_HEIGHT) / ts) + 1);

    for (int ty = minTY; ty <= maxTY; ty++) {
        for (int tx = minTX; tx <= maxTX; tx++) {
            int sx = worldToScreenX(tx * ts);
            int sy = worldToScreenY(ty * ts);
            TileType t = world.getTile(tx, ty);

            if (t == TileType::Air) {
                Color bg = world.getBackgroundColor(tx, ty, day);
                // Stars: occasionally in dark sky tiles
                if (ty < world.surfaceHeight(tx) && day < 0.35f) {
                    uint32_t h = tileHash(tx, ty);
                    if ((h % 22) == 0) {
                        float brightness = (1.0f - day / 0.35f);
                        uint8_t b = static_cast<uint8_t>(160 + (h % 80) * brightness);
                        int sx2 = sx + static_cast<int>((h >> 8)  % static_cast<uint32_t>(ts));
                        int sy2 = sy + static_cast<int>((h >> 16) % static_cast<uint32_t>(ts));
                        fillRect(sx2, sy2, 2, 2, Color(b, b, static_cast<uint8_t>(b + 20)));
                    }
                }
                fillRect(sx, sy, ts, ts, bg);
            } else {
                Color c = world.getTileRenderColor(tx, ty);
                fillRect(sx, sy, ts, ts, c);
                // Subtle grid line between solid tiles (purely visual texture)
                drawRectOutline(sx, sy, ts, ts, Color(0, 0, 0, 25));
            }
        }
    }

    // Tile cursor — highlights block under mouse
    if (!inventoryOpen) {
        int tx, ty;
        getTileUnderMouse(tx, ty);
        int sx = worldToScreenX(tx * ts);
        int sy = worldToScreenY(ty * ts);
        bool reach = player.inReach(tx, ty);
        Color cursorC = reach ? Color(255, 255, 255, 200) : Color(200, 60, 60, 140);
        drawRectOutline(sx,     sy,     ts, ts, cursorC);
        drawRectOutline(sx + 1, sy + 1, ts - 2, ts - 2, Color(0, 0, 0, 80));
    }
}

// ================================================================
//  Mining progress overlay
// ================================================================

void Game::renderMiningProgress() {
    if (player.targetTileX < 0) return;
    TileType tt = world.getTile(player.targetTileX, player.targetTileY);
    if (tt == TileType::Air) return;

    float hardness = getTileInfo(tt).hardness;
    float pct = (hardness > 0.0f) ? std::clamp(player.miningProgress / hardness, 0.0f, 1.0f) : 0.0f;
    const int ts = Config::TILE_SIZE;
    int sx = worldToScreenX(player.targetTileX * ts);
    int sy = worldToScreenY(player.targetTileY * ts);

    // Darkening crack overlay
    fillRect(sx, sy, ts, ts, Color(0, 0, 0, static_cast<uint8_t>(pct * 170)));

    // X crack marks
    uint8_t crackAlpha = static_cast<uint8_t>(pct * 220);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, crackAlpha);
    SDL_RenderDrawLine(renderer, sx + 4,    sy + 4,    sx + ts - 4, sy + ts - 4);
    SDL_RenderDrawLine(renderer, sx + ts-4, sy + 4,    sx + 4,      sy + ts - 4);

    // Progress bar below the tile
    int bx = sx + 2, by = sy + ts + 3, bw = ts - 4, bh = 5;
    fillRect(bx, by, bw, bh, Color(30, 30, 30, 200));
    Color barC = (pct < 0.5f) ? Color(255, 200, 40) : Color(40, 220, 80);
    fillRect(bx, by, static_cast<int>(bw * pct), bh, barC);
}

// ================================================================
//  Particles
// ================================================================

void Game::renderParticles() {
    for (const auto& p : particles.particles()) {
        float alpha = p.life / p.maxLife;
        Color c(p.color.r, p.color.g, p.color.b, static_cast<uint8_t>(p.color.a * alpha));
        int sz = std::max(1, static_cast<int>(p.size * alpha));
        fillRect(worldToScreenX(static_cast<int>(p.x)) - sz / 2,
                 worldToScreenY(static_cast<int>(p.y)) - sz / 2,
                 sz, sz, c);
    }
}

// ================================================================
//  Enemy rendering
// ================================================================

void Game::renderEnemies() {
    for (const auto& e : enemyManager.enemies) {
        if (e.isDead()) continue;
        AABB eb  = e.getAABB();
        int  ex  = worldToScreenX(static_cast<int>(eb.x));
        int  ey  = worldToScreenY(static_cast<int>(eb.y));
        int  ew  = static_cast<int>(eb.w);
        int  eh  = static_cast<int>(eb.h);

        if (ex + ew < 0 || ex > Config::WINDOW_WIDTH  ||
            ey + eh < 0 || ey > Config::WINDOW_HEIGHT) continue;

        Color c = e.renderColor();

        if (e.kind == EnemyKind::Slime) {
            // Wide squished blob
            fillRect(ex, ey + eh / 4, ew, eh - eh / 4, c);
            fillRect(ex + 2, ey, ew - 4, eh / 2, c); // rounded top
            drawRectOutline(ex, ey, ew, eh, Color(0, 0, 0, 140));
            // Eyes
            fillRect(ex + ew / 4 - 1, ey + eh / 2, 4, 3, Color(20, 20, 20));
            fillRect(ex + 3*ew/4 - 2, ey + eh / 2, 4, 3, Color(20, 20, 20));
        } else {
            // Tall cave crawler
            fillRect(ex, ey, ew, eh, c);
            fillRect(ex + 3, ey + 3, ew - 6, eh / 3, Color(c.r + 30, c.g, c.b + 20)); // head highlight
            drawRectOutline(ex, ey, ew, eh, Color(0, 0, 0, 150));
            // Angry eyes
            int eyeX = e.facingRight ? (ex + ew - 10) : (ex + 3);
            fillRect(eyeX, ey + eh / 4, 5, 3, Color(255, 50, 50));
        }

        // Health bar above enemy when hurt
        if (e.health < e.maxHealth) {
            int bx = ex, by = ey - 8, bw = ew, bh = 4;
            fillRect(bx, by, bw, bh, Color(80, 0, 0, 200));
            fillRect(bx, by,
                     static_cast<int>(bw * e.health / e.maxHealth), bh,
                     Color(220, 50, 50, 220));
        }
    }
}

// ================================================================
//  Player rendering
// ================================================================

void Game::renderPlayer() {
    // Invulnerability flicker
    if (player.isInvulnerable() && (SDL_GetTicks64() / 55) % 2 == 0) return;

    int px = worldToScreenX(static_cast<int>(player.x));
    int py = worldToScreenY(static_cast<int>(player.y));
    int pw = static_cast<int>(Config::PLAYER_WIDTH);
    int ph = static_cast<int>(Config::PLAYER_HEIGHT);

    bool hurt = player.isInvulnerable();

    // Head
    int headH = ph * 36 / 100;
    Color skinC = hurt ? Color(255, 180, 180) : Color(238, 195, 150);
    fillRect(px + 4, py, pw - 8, headH, skinC);
    // Eyes
    Color eyeC(55, 35, 25);
    int eyeY = py + headH / 3;
    if (player.facingRight) fillRect(px + pw - 11, eyeY, 4, 3, eyeC);
    else                     fillRect(px + 7,       eyeY, 4, 3, eyeC);

    // Shirt / torso
    int bodyY = py + headH;
    int bodyH = ph * 38 / 100;
    Color shirtC = hurt ? Color(255, 130, 130) : Color(65, 125, 200);
    fillRect(px + 2, bodyY, pw - 4, bodyH, shirtC);

    // Arms (thin rects on sides)
    fillRect(px,        bodyY, 4, bodyH, skinC);
    fillRect(px + pw - 4, bodyY, 4, bodyH, skinC);

    // Legs
    int legY  = bodyY + bodyH;
    int legH  = ph - headH - bodyH;
    int legW  = pw / 2 - 3;
    Color pantsC = hurt ? Color(255, 130, 130) : Color(55, 75, 140);
    // Simple walk animation: alternate leg lower slightly when moving horizontally
    bool moving = std::abs(player.vx) > 10.0f;
    int  step   = (moving && player.onGround) ? static_cast<int>(SDL_GetTicks64() / 150) % 2 : 0;
    fillRect(px + 2,          legY + (step ? 2 : 0),     legW, legH - (step ? 2 : 0), pantsC);
    fillRect(px + pw - 2 - legW, legY + (step ? 0 : 2), legW, legH - (step ? 0 : 2), pantsC);

    drawRectOutline(px, py, pw, ph, Color(0, 0, 0, 90));

    // Held item indicator: small rectangle extending from the "hand" side
    ItemId sel = inventory.selectedItem();
    if (sel != ItemId::None) {
        const ItemInfo& ii = getItemInfo(sel);
        if (ii.isPickaxe || ii.isSword || ii.placeable) {
            int handX = player.facingRight ? (px + pw)     : (px - 8);
            int handY = bodyY + bodyH / 4;
            Color itemC = ii.color;
            fillRect(handX, handY, 8, 8, itemC);
            drawRectOutline(handX, handY, 8, 8, Color(0, 0, 0, 150));
        }
    }
}

// ================================================================
//  UI orchestrator
// ================================================================

void Game::renderUI() {
    renderHotbar();
    renderHealthBar();
    renderZoneLabel();
    renderToasts();

    // Day/night indicator — top-right corner
    float day = dayPhase01();
    bool  isDay = (day > 0.05f);
    Color celestialC = isDay ? Color(255, 230, 55) : Color(210, 215, 245);
    std::string timeStr = isDay ? "DAY" : "NIGHT";
    int tw  = BitmapFont::textWidth(timeStr, 2);
    int indX = Config::WINDOW_WIDTH - tw - 10;
    fillRect(indX - 4, 5, tw + 8, 18, Color(0, 0, 0, 120));
    drawText(timeStr, indX, 7, 2, celestialC);

    // Seed / zone tiny label near bottom right
    {
        std::string zoneStr = world.zoneName(lastZone);
        int zw = BitmapFont::textWidth(zoneStr, 1);
        int zx = Config::WINDOW_WIDTH - zw - 8;
        int zy = Config::WINDOW_HEIGHT - 22;
        fillRect(zx - 3, zy - 2, zw + 6, 11, Color(0, 0, 0, 110));
        drawText(zoneStr, zx, zy, 1, Color(180, 180, 200));
    }

    if (helpOpen)      renderHelpOverlay();
    if (inventoryOpen) renderInventoryPanel();
}

void Game::renderHotbar() {
    const int SLOT  = 50;
    const int GAP   = 4;
    const int STRIDE = SLOT + GAP;
    const int n     = Inventory::hotbarSlots();
    int totalW = n * STRIDE - GAP;
    int startX = Config::WINDOW_WIDTH  / 2 - totalW / 2;
    int startY = Config::WINDOW_HEIGHT - SLOT - 8;

    // Background strip
    fillRect(startX - 6, startY - 4, totalW + 12, SLOT + 8, Color(0, 0, 0, 140));

    for (int i = 0; i < n; i++) {
        int sx = startX + i * STRIDE;
        int sy = startY;
        bool sel = (i == inventory.selectedSlot());

        Color bgC = sel ? Color(255, 210, 50, 200) : Color(55, 55, 68, 200);
        Color brC = sel ? Color(255, 255, 255, 255) : Color(110, 110, 125, 180);
        fillRect(sx, sy, SLOT, SLOT, bgC);
        drawRectOutline(sx, sy, SLOT, SLOT, brC);

        const Slot& slot = inventory.slotAt(i);
        if (slot.item != ItemId::None) {
            drawItemIcon(slot.item, sx + 4, sy + 4, SLOT - 8);
            if (slot.count > 1) {
                std::string cnt = std::to_string(slot.count);
                int cw = BitmapFont::textWidth(cnt, 1);
                drawText(cnt, sx + SLOT - cw - 2, sy + SLOT - 9, 1, Color(240, 240, 240));
            }
        }
        // Slot number hint
        std::string num = std::to_string((i + 1) % 10);
        drawText(num, sx + 2, sy + 2, 1, sel ? Color(30, 30, 30) : Color(160, 160, 165));
    }

    // Name of selected item above the hotbar
    ItemId selId = inventory.selectedItem();
    if (selId != ItemId::None) {
        const std::string name = getItemInfo(selId).name;
        int nw = BitmapFont::textWidth(name, 2);
        int nx = Config::WINDOW_WIDTH / 2 - nw / 2;
        int ny = startY - BitmapFont::textHeight(2) - 6;
        fillRect(nx - 5, ny - 2, nw + 10, BitmapFont::textHeight(2) + 4, Color(0, 0, 0, 120));
        drawText(name, nx, ny, 2, Color(255, 255, 200));
    }
}

void Game::renderHealthBar() {
    float pct = player.health / player.maxHealth;
    Color barC = (pct > 0.60f) ? Color(50, 210, 80)  :
                 (pct > 0.30f) ? Color(240, 200, 40) :
                                  Color(220, 55, 55);
    int bx = 8, by = 8, bw = 180, bh = 14;
    fillRect(bx, by, bw, bh, Color(30, 30, 30, 180));
    fillRect(bx, by, static_cast<int>(bw * pct), bh, barC);
    drawRectOutline(bx, by, bw, bh, Color(200, 200, 200, 160));

    std::ostringstream oss;
    oss << "HP " << static_cast<int>(player.health)
        << "/" << static_cast<int>(player.maxHealth);
    drawText(oss.str(), bx + 4, by + bh + 4, 1, Color(230, 230, 230));
}

void Game::renderZoneLabel() {
    // Zone name is pushed as a toast when zone changes; nothing extra needed.
}

void Game::renderToasts() {
    int baseY = Config::WINDOW_HEIGHT - 80; // above hotbar
    int count = 0;
    for (int i = static_cast<int>(toasts.size()) - 1; i >= 0 && count < 4; i--, count++) {
        const Toast& t = toasts[static_cast<size_t>(i)];
        float alpha01 = std::min(1.0f, t.timer / 0.5f);
        int tw = BitmapFont::textWidth(t.text, 2);
        int tx = Config::WINDOW_WIDTH / 2 - tw / 2;
        int ty = baseY - count * 22;
        fillRect(tx - 6, ty - 2, tw + 12, BitmapFont::textHeight(2) + 4,
                 Color(0, 0, 0, static_cast<uint8_t>(130 * alpha01)));
        drawText(t.text, tx, ty, 2,
                 Color(255, 255, 200, static_cast<uint8_t>(240 * alpha01)));
    }
}

void Game::renderHelpOverlay() {
    // Semi-transparent background
    fillRect(0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT, Color(0, 0, 0, 175));

    const int COL1 = 40, COL2 = 500;
    int y = 40;
    auto line = [&](const char* label, const char* value) {
        drawText(label, COL1, y, 2, Color(255, 220, 100));
        if (value && value[0]) {
            drawText(value, COL1 + BitmapFont::textWidth(label, 2) + 14, y, 2, Color(210, 210, 210));
        }
        y += 22;
    };
    auto sectionTitle = [&](const char* t) {
        drawText(t, COL1, y, 2, Color(120, 210, 255));
        y += 26;
    };

    drawText("TERRA  —  CONTROLS  (F1 TO CLOSE)", COL1, 10, 2, Color(255, 255, 255));
    y = 50;

    sectionTitle("MOVEMENT");
    line("A / LEFT",     "Move left");
    line("D / RIGHT",    "Move right");
    line("W / SPACE / UP",  "Jump (coyote-time + buffer)");

    y += 6; sectionTitle("WORLD INTERACTION");
    line("LMB (hold)",   "Mine block");
    line("LMB (click)",  "Attack enemy near cursor");
    line("RMB",          "Place held block");
    line("SCROLL",       "Cycle hotbar");
    line("1 – 0",        "Select hotbar slot");

    y += 6; sectionTitle("MENUS / SYSTEM");
    line("E",            "Open / close inventory + crafting");
    line("F1",           "Toggle this help");
    line("F5",           "Save game");
    line("ESC",          "Quit");

    // Right column: tips
    y = 50;
    int rc = COL2;
    drawText("TIPS", rc, y, 2, Color(120, 210, 255)); y += 26;
    auto tip = [&](const char* t) {
        drawText(t, rc, y, 1, Color(200, 200, 200));
        y += 14;
    };
    tip("Craft a pickaxe before mining stone.");
    tip("Coal + Wood makes 4 torches (E to craft).");
    tip("Enemies spawn at night and in dark caves.");
    tip("Place torches (RMB) to push back darkness.");
    tip("Better pickaxes unlock deeper ore tiers.");
    tip("Gold and Diamond are deep underground.");
    tip("A sword increases your melee damage.");
    tip("F5 saves. Your world reloads on next run.");
    y += 6;
    tip("Ore rarity (most → least):");
    tip("  Coal > Iron > Gold > Diamond");
    tip("Depth zones:");
    tip("  Surface > Underground > Caverns > The Deep");

    // Current game info
    y += 14;
    std::ostringstream ss;
    ss << "World seed: " << world.getSeed();
    drawText(ss.str(), rc, y, 1, Color(140, 140, 160)); y += 14;
    ss.str(""); ss << "Current zone: " << world.zoneName(lastZone);
    drawText(ss.str(), rc, y, 1, Color(140, 140, 160)); y += 14;
    ss.str(""); ss << "Time: " << (dayPhase01() > 0.05f ? "Day" : "Night");
    drawText(ss.str(), rc, y, 1, Color(140, 140, 160));
}

void Game::renderInventoryPanel() {
    // ---- Full-screen dark backdrop ----
    fillRect(0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT, Color(0, 0, 0, 190));

    // ================================================================
    //  LEFT PANEL — Inventory
    // ================================================================
    const int SLOT   = 56;
    const int GAP    = 4;
    const int STRIDE = SLOT + GAP;
    const int COLS   = Inventory::hotbarSlots(); // 10
    const int ROWS   = Inventory::totalSlots() / COLS; // 3
    const int INV_X  = 20;
    const int INV_Y  = 80;

    drawText("INVENTORY", INV_X, 40, 2, Color(120, 210, 255));

    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            int slotIndex = row * COLS + col;
            int sx = INV_X + col * STRIDE;
            int sy = INV_Y + row * STRIDE;
            const Slot& s = inventory.slotAt(slotIndex);

            // Hotbar row gets a gold tint
            bool isHotbarRow = (row == 0);
            bool isSel       = (slotIndex == inventory.selectedSlot());
            Color bgC  = isSel      ? Color(255, 215, 50, 190) :
                         isHotbarRow ? Color(70, 65, 45, 180)   :
                                       Color(45, 45, 55, 180);
            Color borC = isSel      ? Color(255, 255, 255) :
                         isHotbarRow ? Color(160, 140, 60)  :
                                       Color(90, 90, 105);

            fillRect(sx, sy, SLOT, SLOT, bgC);
            drawRectOutline(sx, sy, SLOT, SLOT, borC);

            if (s.item != ItemId::None) {
                drawItemIcon(s.item, sx + 4, sy + 4, SLOT - 8);
                if (s.count > 1) {
                    std::string cnt = std::to_string(s.count);
                    drawText(cnt,
                             sx + SLOT - BitmapFont::textWidth(cnt, 1) - 2,
                             sy + SLOT - 9, 1, Color(240, 240, 240));
                }
            }
        }
    }

    // Row labels
    drawText("HOTBAR",  INV_X + COLS * STRIDE + 8, INV_Y,              1, Color(160, 140, 60));
    drawText("BACKPACK",INV_X + COLS * STRIDE + 8, INV_Y + STRIDE,     1, Color(130, 130, 150));
    drawText("",        INV_X + COLS * STRIDE + 8, INV_Y + STRIDE * 2, 1, Color(130, 130, 150));

    // ================================================================
    //  RIGHT PANEL — Crafting
    // ================================================================
    int cy = CRAFT_Y0;
    drawText("CRAFTING  (CLICK TO CRAFT)", CRAFT_X, 40, 2, Color(120, 210, 255));

    const auto& recipes = getAllRecipes();
    for (int ri = 0; ri < static_cast<int>(recipes.size()); ri++) {
        const Recipe& r  = recipes[static_cast<size_t>(ri)];
        bool canCraft    = inventory.canCraft(r);
        int  ry = cy + ri * RECIPE_H;

        Color rowBg  = canCraft ? Color(40, 80, 40, 200) : Color(40, 40, 50, 160);
        Color rowBor = canCraft ? Color(80, 180, 80, 200) : Color(60, 60, 75, 160);
        fillRect(CRAFT_X, ry, 600, RECIPE_H - 3, rowBg);
        drawRectOutline(CRAFT_X, ry, 600, RECIPE_H - 3, rowBor);

        // Result icon
        drawItemIcon(r.result, CRAFT_X + 4, ry + 5, RECIPE_H - 14);

        // Name + count
        std::string nameStr = getItemInfo(r.result).name;
        if (r.resultCount > 1) nameStr += " x" + std::to_string(r.resultCount);
        Color nameC = canCraft ? Color(200, 255, 180) : Color(140, 140, 150);
        drawText(nameStr, CRAFT_X + RECIPE_H - 6, ry + 5, 2, nameC);

        // Ingredients
        std::string ingStr;
        for (int ii = 0; ii < static_cast<int>(r.ingredients.size()); ii++) {
            if (ii > 0) ingStr += "  ";
            ingStr += getItemInfo(r.ingredients[static_cast<size_t>(ii)].first).name;
            ingStr += " x";
            ingStr += std::to_string(r.ingredients[static_cast<size_t>(ii)].second);
            int have = inventory.countItem(r.ingredients[static_cast<size_t>(ii)].first);
            ingStr += " (" + std::to_string(have) + ")";
        }
        Color ingC = canCraft ? Color(180, 220, 180) : Color(110, 110, 120);
        drawText(ingStr, CRAFT_X + RECIPE_H - 6, ry + 22, 1, ingC);

        if (canCraft) {
            int hintX = CRAFT_X + 600 - BitmapFont::textWidth("CLICK", 1) - 8;
            drawText("CLICK", hintX, ry + RECIPE_H / 2 - 4, 1, Color(150, 255, 150));
        }
    }

    // Footer
    int footY = Config::WINDOW_HEIGHT - 20;
    drawText("[E] CLOSE INVENTORY", Config::WINDOW_WIDTH / 2 - 100, footY, 1, Color(160, 160, 180));
}

// ================================================================
//  Drawing primitives — thin wrappers over SDL2
// ================================================================

void Game::fillRect(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r = makeSDLRect(x, y, w, h);
    SDL_RenderFillRect(renderer, &r);
}

void Game::drawRectOutline(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r = makeSDLRect(x, y, w, h);
    SDL_RenderDrawRect(renderer, &r);
}

void Game::drawText(const std::string& s, int x, int y, int scale, Color c) {
    BitmapFont::drawText(s, x, y, scale, c,
        [this](int rx, int ry, int rw, int rh, Color rc) {
            fillRect(rx, ry, rw, rh, rc);
        });
}

void Game::drawItemIcon(ItemId id, int x, int y, int size) {
    if (id == ItemId::None) return;
    const ItemInfo& info = getItemInfo(id);
    Color c = info.color;

    fillRect(x, y, size, size, c);
    drawRectOutline(x, y, size, size, Color(0, 0, 0, 100));

    if (info.isPickaxe) {
        // Pickaxe head
        int hw = size * 55 / 100, hh = size * 28 / 100;
        int hx = x + (size - hw) / 2, hy = y + size / 8;
        fillRect(hx, hy, hw, hh, Color(210, 215, 225));
        // Handle
        fillRect(x + size / 2 - 1, hy + hh, 3, size / 2, Color(160, 115, 65));
    } else if (info.isSword) {
        // Blade diagonal
        int bx1 = x + size / 5,      by1 = y + size * 4 / 5;
        int bx2 = x + size * 4 / 5, by2 = y + size / 5;
        SDL_SetRenderDrawColor(renderer, 215, 220, 235, 230);
        SDL_RenderDrawLine(renderer, bx1, by1, bx2, by2);
        SDL_RenderDrawLine(renderer, bx1 + 1, by1, bx2 + 1, by2);
        // Cross-guard
        int gx = x + size / 2 - size / 4;
        int gy = y + size / 2;
        fillRect(gx, gy, size / 2, 2, Color(180, 150, 80));
    } else if (id == ItemId::Torch) {
        // Flame
        fillRect(x + size / 3, y + size / 5,  size / 3, size * 3/5, Color(250, 150, 30));
        fillRect(x + size / 3, y + size / 5,  size / 3, size / 4,   Color(255, 245, 100));
    }
    // Shine dot (top-left corner)
    fillRect(x + 2, y + 2, 2, 2, Color(255, 255, 255, 60));
}

// ================================================================
//  Helpers
// ================================================================

void Game::getTileUnderMouse(int& tx, int& ty) const {
    float wx = mouseX + camX;
    float wy = mouseY + camY;
    tx = std::clamp(static_cast<int>(std::floor(wx / Config::TILE_SIZE)), 0, world.width()  - 1);
    ty = std::clamp(static_cast<int>(std::floor(wy / Config::TILE_SIZE)), 0, world.height() - 1);
}

void Game::pushToast(const std::string& text) {
    if (toasts.size() >= 6) toasts.erase(toasts.begin());
    toasts.push_back({ text, 3.2f, 3.2f });
}

void Game::doSave() {
    if (SaveSystem::saveGame(kSavePath, world, player, inventory, dayTime)) {
        pushToast("GAME SAVED  (F5)");
    } else {
        pushToast("SAVE FAILED");
    }
}

bool Game::tryCraftAtPanelPosition(int mx, int my) {
    if (!inventoryOpen) return false;
    if (mx < CRAFT_X || mx > CRAFT_X + 600) return false;
    int row = (my - CRAFT_Y0) / RECIPE_H;
    if (row < 0) return false;
    const auto& recipes = getAllRecipes();
    if (row >= static_cast<int>(recipes.size())) return false;

    const Recipe& r = recipes[static_cast<size_t>(row)];
    if (!inventory.canCraft(r)) {
        pushToast("NEED MORE MATERIALS");
        return false;
    }
    if (inventory.craft(r)) {
        pushToast(std::string("CRAFTED: ") + getItemInfo(r.result).name);
        return true;
    }
    pushToast("INVENTORY FULL");
    return false;
}
