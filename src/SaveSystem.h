#pragma once
#include <string>

class World;
class Player;
class Inventory;

// A single save file = world tiles + player position/health + inventory +
// the day/night clock. Enemies are intentionally NOT persisted - they
// respawn fresh, which keeps the format simple and is a fine trade-off for
// a sandbox/demo game.
namespace SaveSystem {
    bool saveGame(const std::string& path, const World& world, const Player& player,
                  const Inventory& inv, float dayTime);
    bool loadGame(const std::string& path, World& world, Player& player,
                  Inventory& inv, float& dayTime);
}
