#include "Item.h"
#include <array>
#include <stdexcept>

namespace {
    const std::array<ItemInfo, static_cast<size_t>(ItemId::Count)> kItemTable = {{
        /* None         */ { "None",          Color(0,0,0,0),       0,   false,false, 0,0.f,0, false, TileType::Air },
        /* Dirt         */ { "Dirt",          Color(121,85,58),     999, false,false, 0,0.f,0, true,  TileType::Dirt },
        /* Stone        */ { "Stone",         Color(130,130,135),   999, false,false, 0,0.f,0, true,  TileType::Stone },
        /* Sand         */ { "Sand",          Color(214,198,140),   999, false,false, 0,0.f,0, true,  TileType::Sand },
        /* Wood         */ { "Wood",          Color(133,94,66),     999, false,false, 0,0.f,0, true,  TileType::Wood },
        /* Leaves       */ { "Leaves",        Color(62,112,52),     999, false,false, 0,0.f,0, true,  TileType::Leaves },
        /* Coal         */ { "Coal",          Color(35,35,38),      999, false,false, 0,0.f,0, false, TileType::Air },
        /* Iron         */ { "Iron",          Color(216,141,101),   999, false,false, 0,0.f,0, false, TileType::Air },
        /* Gold         */ { "Gold",          Color(255,210,90),    999, false,false, 0,0.f,0, false, TileType::Air },
        /* Diamond      */ { "Diamond",       Color(158,240,236),   999, false,false, 0,0.f,0, false, TileType::Air },
        /* Torch        */ { "Torch",         Color(255,191,94),    999, false,false, 0,0.f,0, true,  TileType::Torch },
        /* PickaxeWood  */ { "Wood Pickaxe",  Color(180,140,90),    1,   true, false, 1,2.2f,0, false, TileType::Air },
        /* PickaxeStone */ { "Stone Pickaxe", Color(150,150,155),   1,   true, false, 2,3.5f,0, false, TileType::Air },
        /* PickaxeIron  */ { "Iron Pickaxe",  Color(200,200,205),   1,   true, false, 3,5.0f,0, false, TileType::Air },
        /* PickaxeGold  */ { "Gold Pickaxe",  Color(255,215,100),   1,   true, false, 4,7.0f,0, false, TileType::Air },
        /* SwordWood    */ { "Wood Sword",    Color(170,130,85),    1,   false,true,  0,0.f,8,  false, TileType::Air },
        /* SwordStone   */ { "Stone Sword",   Color(160,160,165),   1,   false,true,  0,0.f,14, false, TileType::Air },
        /* SwordIron    */ { "Iron Sword",    Color(210,210,215),   1,   false,true,  0,0.f,22, false, TileType::Air },
        /* SwordGold    */ { "Gold Sword",    Color(255,220,110),   1,   false,true,  0,0.f,34, false, TileType::Air },
    }};

    const std::vector<Recipe> kRecipes = {
        { ItemId::PickaxeWood,  1, {{ItemId::Wood, 6}},                     "Basic pickaxe. Mines stone & coal." },
        { ItemId::SwordWood,    1, {{ItemId::Wood, 4}},                     "Basic sword." },
        { ItemId::PickaxeStone, 1, {{ItemId::Wood, 2}, {ItemId::Stone, 6}}, "Mines iron ore." },
        { ItemId::SwordStone,   1, {{ItemId::Wood, 2}, {ItemId::Stone, 4}}, "Stronger sword." },
        { ItemId::PickaxeIron,  1, {{ItemId::Wood, 2}, {ItemId::Iron, 6}},  "Mines gold ore." },
        { ItemId::SwordIron,    1, {{ItemId::Wood, 2}, {ItemId::Iron, 4}},  "Stronger still." },
        { ItemId::PickaxeGold,  1, {{ItemId::Wood, 2}, {ItemId::Gold, 6}},  "Mines diamond ore." },
        { ItemId::SwordGold,    1, {{ItemId::Wood, 2}, {ItemId::Gold, 4}},  "The best sword." },
        { ItemId::Torch,        4, {{ItemId::Wood, 1}, {ItemId::Coal, 1}},  "Lights up the dark." },
    };
}

const ItemInfo& getItemInfo(ItemId id) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= kItemTable.size()) {
        throw std::out_of_range("getItemInfo: invalid ItemId");
    }
    return kItemTable[idx];
}

ItemId tileDropItem(TileType t) {
    switch (t) {
        case TileType::Grass:      return ItemId::Dirt;
        case TileType::Dirt:       return ItemId::Dirt;
        case TileType::Stone:      return ItemId::Stone;
        case TileType::Sand:       return ItemId::Sand;
        case TileType::Wood:       return ItemId::Wood;
        case TileType::Leaves:     return ItemId::Leaves;
        case TileType::CoalOre:    return ItemId::Coal;
        case TileType::IronOre:    return ItemId::Iron;
        case TileType::GoldOre:    return ItemId::Gold;
        case TileType::DiamondOre: return ItemId::Diamond;
        case TileType::Torch:      return ItemId::Torch;
        default:                   return ItemId::None;
    }
}

const std::vector<Recipe>& getAllRecipes() {
    return kRecipes;
}
