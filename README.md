Terra

Terra is a Terraria-inspired 2D sandbox platformer written in C++17 with SDL2. You mine, build, craft, fight, and explore a procedurally generated world that stretches from the grassy surface all the way down to diamond-rich deep caverns.

Features

The world is procedurally generated at 400 by 160 tiles, using Perlin noise to create rolling hills and layered geology. Beneath the surface are large connected cave networks that grow denser the deeper you go. Four tiers of ore (coal, iron, gold, and diamond) are gated by depth, so you have to dig further to find the good stuff.

Lighting is handled with a BFS flood-fill system that covers open sky, torch propagation, and a dim ambient floor in the caves. A smooth day and night cycle runs on a three minute sinusoidal loop, and enemies spawn at night and in dark caves. Movement uses sub-stepped AABB collision with coyote time and jump buffering, so it feels responsive rather than stiff.

Mining is done by holding the left mouse button, and tool tiers matter (bare hands can't break stone, for example). You build by placing any item flagged as placeable with the right mouse button. There are nine crafting recipes covering pickaxes, swords, and torches, and crafting is atomic, meaning it either happens in full or not at all.

For combat, you'll face slimes on the surface at night and cave crawlers underground in the dark, both with chase and patrol AI. Click near an enemy to swing at it, and a sword does more damage than your bare hands. Your inventory is a 10-slot hotbar plus a 20-slot backpack with full stacking. The game saves to a single file (press F5 to save, and it loads automatically on your next run). The font is a built-in 5 by 7 bitmap font, so there are no external font dependencies to worry about.

# SETUP
You'll need g++ with C++17 support and the SDL2 library (core only, so no SDL_image or SDL_ttf). Install SDL2 with the command that matches your system:

bash
# Ubuntu / Debian
sudo apt install libsdl2-dev

# Fedora / RHEL
sudo dnf install SDL2-devel

# macOS (Homebrew)
brew install sdl2

# Windows (MSYS2 / MinGW-w64)
pacman -S mingw-w64-x86_64-SDL2
Build and Run

Clone the repo, move into the folder, and build the optimised release:

bash
git clone <repo>
cd terra
make
./terra

You can also build and run in one step with make run. For a debug build with AddressSanitizer and UBSan, use make debug && ./terra_debug. To run all the logic-layer unit tests (which don't need SDL), use make tests, and clean up build artefacts with make clean.

If you're on Windows with Visual Studio, create a new project, add all the src/*.cpp files, add the SDL2 include and lib paths, and link SDL2.lib and SDL2main.lib. You may also need to change #include <SDL2/SDL.h> to #include <SDL.h> in Game.h.

Controls

Move left with A or the left arrow, and right with D or the right arrow. Jump with W, Space, or the up arrow. Hold the left mouse button to mine the tile under your cursor, or click it in open air to attack a nearby enemy. The right mouse button places your held block.

Scroll to cycle through the hotbar, or press keys 1 through 0 to select a slot directly. Press E to open and close the inventory and crafting panel, F1 to toggle the help overlay, F5 to save, and ESC to quit.

Gameplay Guide

You spawn on the surface with a stone pickaxe, a wood sword, some wood, and a few torches. Start by punching trees with the left mouse button to gather wood, which needs no tool. Press E to open the inventory panel and craft better gear. Mine coal (the black ore in stone) and combine it with wood to make more torches. From there, dig down to find iron, then gold, then diamond.

Crafting recipes

A wood pickaxe takes 6 wood, and a wood sword takes 4 wood. A stone pickaxe needs 2 wood and 6 stone, while a stone sword needs 2 wood and 4 stone. The iron and gold tiers follow the same pattern: the pickaxe is 2 wood plus 6 of the ore, and the sword is 2 wood plus 4 of the ore. Torches come 4 at a time from 1 wood and 1 coal.

Tool tiers

Bare hands can mine dirt, grass, leaves, and wood. A wood pickaxe adds stone and coal. A stone pickaxe adds iron ore, an iron pickaxe adds gold ore, and a gold pickaxe adds diamond ore.

Ore depth guide

Coal appears everywhere from the surface down. Iron starts showing up in the underground layer around rows 60 to 95. Gold joins in through the caverns from rows 95 to 130, and diamond appears (rarely) in the deep from rows 130 to 157.

Enemies

Slimes spawn at night on the surface and hop toward you for gentle damage. Cave crawlers spawn underground wherever light drops below about 30 percent, and they walk and chase for higher damage. A good tip is to place torches in caves, since they prevent cave crawler spawns and let you see the ore around you.

Lighting

Open sky gives full brightness. Every solid tile the light passes through costs 0.32 brightness, and every air tile costs 0.06. Torches emit full brightness and re-illuminate a 40-tile radius when placed. Caves without torches sit at a dim ambient floor that's barely visible, so bring torches.

Architecture

The project lives under src/, with the entry point in main.cpp. Tunable constants sit in Config.h, and Color.h holds a dependency-free RGBA struct. Tiles and items are defined in TileType.h/.cpp and Item.h/.cpp, the latter also holding the crafting recipes. World generation uses Perlin noise and fBm from Noise.h/.cpp, and World.h/.cpp manages the tile grid, procedural generation, BFS lighting, and serialization.

The player logic (physics, AABB collision, mining, and placing) is in Player.h/.cpp, and the inventory (stacking, crafting, serialization) is in Inventory.h/.cpp. Enemy AI, physics, and spawning live in Enemy.h/.cpp, while Particle.h/.cpp handles break and death particle bursts. The save system in SaveSystem.h/.cpp bundles the world, player, and inventory into one file, and BitmapFont.h/.cpp provides the self-contained pixel font. Finally, Game.h/.cpp holds the SDL2 main loop, rendering, and UI.

The tests/ folder holds headless logic test suites for the world, player, inventory, enemy, and save system, none of which need SDL to run. The design splits the game into a graphics-free logic layer (World, Player, Inventory, Enemy, SaveSystem) and a thin SDL2 rendering layer (Game). Every logic module has a matching headless test suite, and make tests runs them all.

Save file format

The game writes to terra_save.dat in the working directory. It's a binary format that starts with a TERRASAV header and version number, followed by the world blob, the player's position, health, and max health, the current day time, and finally the inventory blob. Delete terra_save.dat to start a fresh world.

Tuning

All the gameplay constants live in src/Config.h. You can change the world size with WORLD_WIDTH and WORLD_HEIGHT, speed up or slow down the day with DAY_LENGTH_SECONDS, and adjust how the physics feel with GRAVITY and JUMP_VELOCITY. REACH_TILES sets your mining and placing range, MAX_ENEMIES_ALIVE caps the enemy count, and LIGHT_FALLOFF_SOLID controls how fast light dies through stone.
