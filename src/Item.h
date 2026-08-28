#pragma once
#include <cstdint>
#include <vector>
#include <utility>
#include "TileType.h"
#include "Color.h"

// Append-only for save compatibility, same rule as TileType.
enum class ItemId : uint8_t {
    None = 0,
    Dirt,
    Stone,
    Sand,
    Wood,
    Leaves,
    Coal,
    Iron,
    Gold,
    Diamond,
    Torch,
    PickaxeWood,
    PickaxeStone,
    PickaxeIron,
    PickaxeGold,
    SwordWood,
    SwordStone,
    SwordIron,
    SwordGold,
    Count
};

struct ItemInfo {
    const char* name;
    Color color;       // icon color in the UI
    int   maxStack;     // 1 means "unique equippable", not stackable

    bool  isPickaxe;
    bool  isSword;
    int   toolTier;            // for pickaxes: what minToolTier this satisfies
    float miningSpeedMul;      // for pickaxes: hardness / miningSpeedMul = seconds to mine
    int   meleeDamage;         // for swords

    bool      placeable;   // can right-click place a tile
    TileType  placesTile;
};

const ItemInfo& getItemInfo(ItemId id);

// What you receive when a given tile is broken (ItemId::None if nothing).
ItemId tileDropItem(TileType t);

// A crafting recipe: consume `ingredients` (item, count) pairs, produce
// `resultCount` of `result`.
struct Recipe {
    ItemId result;
    int resultCount;
    std::vector<std::pair<ItemId, int>> ingredients;
    const char* description;
};

const std::vector<Recipe>& getAllRecipes();
