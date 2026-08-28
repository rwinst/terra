#include "SaveSystem.h"
#include "World.h"
#include "Player.h"
#include "Inventory.h"
#include <fstream>
#include <cstring>
#include <cstdint>

namespace SaveSystem {

bool saveGame(const std::string& path, const World& world, const Player& player,
              const Inventory& inv, float dayTime) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    const char magic[8] = {'T', 'E', 'R', 'R', 'A', 'S', 'A', 'V'};
    out.write(magic, 8);
    int32_t version = 1;
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));

    if (!world.writeTo(out)) return false;

    float px = player.x, py = player.y, phealth = player.health, pmax = player.maxHealth;
    out.write(reinterpret_cast<const char*>(&px), sizeof(px));
    out.write(reinterpret_cast<const char*>(&py), sizeof(py));
    out.write(reinterpret_cast<const char*>(&phealth), sizeof(phealth));
    out.write(reinterpret_cast<const char*>(&pmax), sizeof(pmax));
    out.write(reinterpret_cast<const char*>(&dayTime), sizeof(dayTime));

    if (!inv.writeTo(out)) return false;

    out.flush();
    return out.good();
}

bool loadGame(const std::string& path, World& world, Player& player,
              Inventory& inv, float& dayTime) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    char magic[8];
    in.read(magic, 8);
    if (!in.good() || std::memcmp(magic, "TERRASAV", 8) != 0) return false;

    int32_t version = 0;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!in.good() || version != 1) return false;

    if (!world.readFrom(in)) return false;

    float px = 0, py = 0, phealth = 0, pmax = 0;
    in.read(reinterpret_cast<char*>(&px), sizeof(px));
    in.read(reinterpret_cast<char*>(&py), sizeof(py));
    in.read(reinterpret_cast<char*>(&phealth), sizeof(phealth));
    in.read(reinterpret_cast<char*>(&pmax), sizeof(pmax));
    in.read(reinterpret_cast<char*>(&dayTime), sizeof(dayTime));
    if (!in.good()) return false;

    player.x = px;
    player.y = py;
    player.maxHealth = pmax;
    player.health = phealth;
    player.vx = 0.0f;
    player.vy = 0.0f;
    player.onGround = false;

    if (!inv.readFrom(in)) return false;

    return true;
}

}
