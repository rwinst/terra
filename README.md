# Terra — A Terraria-Style 2D Platformer

A complete Terraria-inspired 2D sandbox platformer written in C++17 with SDL2.  
Mine, build, craft, fight, and explore a procedurally generated world — all the way  
from the grassy surface down to diamond-rich deep caverns.

---

## Features

| System | Details |
|---|---|
| **Procedural world** | 400×160 tile world; Perlin-noise terrain, rolling hills, layered geology |
| **Caves** | Large connected cave networks that grow denser with depth |
| **Ores** | 4 ore tiers (Coal → Iron → Gold → Diamond) gated by depth |
| **Lighting** | BFS flood-fill lighting; open sky, torch propagation, ambient cave floor |
| **Day / night cycle** | Smooth sinusoidal 3-minute cycle; enemies spawn at night and in dark caves |
| **Physics** | Sub-stepped AABB collision, coyote time, jump buffering |
| **Mining** | Hold LMB; tool-tier gates (bare hands can't mine stone) |
| **Building** | RMB to place any item flagged as placeable |
| **Crafting** | 9 recipes (pickaxes, swords, torches) — all-or-nothing atomic crafting |
| **Enemies** | Slimes (surface night) and Cave Crawlers (underground dark) with AI chase/patrol |
| **Combat** | Click near an enemy to swing; sword does more damage than bare hands |
| **Inventory** | 10-slot hotbar + 20-slot backpack, full stacking |
| **Save / load** | Single save file; F5 to save, auto-loaded on next run |
| **Font** | Built-in 5×7 bitmap font — zero external font dependencies |

---

## Requirements

- **Compiler**: g++ with C++17 support  
- **Library**: SDL2 (core only — no SDL_image, no SDL_ttf)

### Install SDL2

```bash
# Ubuntu / Debian
sudo apt install libsdl2-dev

# Fedora / RHEL
sudo dnf install SDL2-devel

# macOS (Homebrew)
brew install sdl2

# Windows (MSYS2 / MinGW-w64)
pacman -S mingw-w64-x86_64-SDL2
```

---

## Build & Run

```bash
git clone <repo>
cd terra

# Build optimised release
make

# Run
./terra

# Or build + run in one step
make run

# Debug build (AddressSanitizer + UBSan)
make debug && ./terra_debug

# Run all logic-layer unit tests (no SDL required)
make tests

# Clean build artefacts
make clean
```

> **Windows (Visual Studio)**: Create a new project, add all `src/*.cpp` files,  
> add the SDL2 include/lib paths, and link `SDL2.lib` + `SDL2main.lib`.  
> Change `#include <SDL2/SDL.h>` to `#include <SDL.h>` in `Game.h` if needed.

---

## Controls

| Key / Mouse | Action |
|---|---|
| `A` / `←` | Move left |
| `D` / `→` | Move right |
| `W` / `Space` / `↑` | Jump |
| **LMB (hold)** | Mine tile under cursor |
| **LMB (click air)** | Attack enemy near cursor |
| **RMB** | Place held block |
| `Scroll` | Cycle hotbar selection |
| `1` – `0` | Select hotbar slot |
| `E` | Open / close inventory & crafting |
| `F1` | Toggle help overlay |
| `F5` | Save game |
| `ESC` | Quit |

---

## Gameplay Guide

### Getting started

You spawn on the surface with a **Stone Pickaxe**, **Wood Sword**, some **Wood**, and **Torches**.

1. **Punch trees** (LMB) for Wood — no tool required for Leaves/Wood  
2. Press **E** to open the inventory panel and craft better gear  
3. Mine **Coal** (black ore in stone) — combine with Wood to make more Torches  
4. Dig down to find **Iron**, then **Gold**, then **Diamond**  

### Crafting recipes

| Result | Ingredients |
|---|---|
| Wood Pickaxe | 6× Wood |
| Wood Sword | 4× Wood |
| Stone Pickaxe | 2× Wood + 6× Stone |
| Stone Sword | 2× Wood + 4× Stone |
| Iron Pickaxe | 2× Wood + 6× Iron |
| Iron Sword | 2× Wood + 4× Iron |
| Gold Pickaxe | 2× Wood + 6× Gold |
| Gold Sword | 2× Wood + 4× Gold |
| Torch (×4) | 1× Wood + 1× Coal |

### Tool tiers

| Pickaxe | Mines |
|---|---|
| Bare hands | Dirt, Grass, Leaves, Wood |
| Wood Pickaxe | + Stone, Coal |
| Stone Pickaxe | + Iron Ore |
| Iron Pickaxe | + Gold Ore |
| Gold Pickaxe | + Diamond Ore |

### Ore depth guide

```
Surface  (rows 0–60)  : Coal only
Underground (60–95)   : Coal + Iron
Caverns   (95–130)    : + Gold
The Deep  (130–157)   : + Diamond (rare)
```

### Enemies

| Enemy | Spawns when | Behaviour |
|---|---|---|
| **Slime** | Night, surface | Hops toward player, gentle damage |
| **Cave Crawler** | Underground, dark (light < 30%) | Walks/chases, higher damage |

**Tip:** Place Torches in caves — they prevent Cave Crawler spawns and let you see ores.

### Lighting

- Open sky = full brightness  
- Each solid tile the light passes through costs 0.32 brightness  
- Each air tile costs 0.06  
- Torches emit full (1.0) brightness and re-illuminate a 40-tile radius on placement  
- Caves without torches sit at a dim ambient floor (barely visible — bring torches!)

---

## Architecture

```
terra/
├── src/
│   ├── Config.h          — All tunable constants (tile size, physics, timing…)
│   ├── Color.h           — Dependency-free RGBA struct
│   ├── TileType.h/.cpp   — Tile enum + per-tile property table
│   ├── Item.h/.cpp       — Item enum, properties, crafting recipes
│   ├── Noise.h/.cpp      — Perlin noise + fBm (world generation)
│   ├── World.h/.cpp      — Tile grid, procedural gen, BFS lighting, serialization
│   ├── Player.h/.cpp     — Physics, AABB collision, mining, placing
│   ├── Inventory.h/.cpp  — Stacking, crafting, serialization
│   ├── Enemy.h/.cpp      — AI (chase/patrol), physics, spawn manager
│   ├── Particle.h/.cpp   — Break/death particle bursts
│   ├── SaveSystem.h/.cpp — Bundles World + Player + Inventory into one file
│   ├── BitmapFont.h/.cpp — Self-contained 5×7 pixel font (no SDL_ttf)
│   ├── Game.h/.cpp       — SDL2 main loop, rendering, UI
│   └── main.cpp          — Entry point
├── tests/                — Headless logic test suites (no SDL required)
│   ├── test_world.cpp
│   ├── test_player.cpp
│   ├── test_inventory.cpp
│   ├── test_enemy.cpp
│   └── test_savesystem.cpp
├── Makefile
└── README.md
```

The game is split into a **graphics-free logic layer** (World, Player, Inventory, Enemy, SaveSystem) and a **thin SDL2 rendering layer** (Game). Every logic module has a corresponding headless test suite that runs without a display — `make tests` runs them all.

---

## Save file format

`terra_save.dat` in the working directory. Binary format:

```
[TERRASAV][version:i32][TWLD world blob][player x,y,hp,maxhp : 4×f32]
[dayTime:f32][TINV inventory blob]
```

Delete `terra_save.dat` to start a fresh world.

---

## Tuning

All gameplay constants live in `src/Config.h`:

```cpp
Config::WORLD_WIDTH / WORLD_HEIGHT   // change world size
Config::DAY_LENGTH_SECONDS           // speed up/slow down day cycle
Config::GRAVITY / JUMP_VELOCITY      // adjust physics feel
Config::REACH_TILES                  // mining/placing range
Config::MAX_ENEMIES_ALIVE            // enemy cap
Config::LIGHT_FALLOFF_SOLID          // how fast light dies through stone
```
