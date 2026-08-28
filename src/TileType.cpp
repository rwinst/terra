#include "TileType.h"
#include <array>
#include <stdexcept>

namespace {
    // Indexed by static_cast<size_t>(TileType). Keep in exact enum order.
    const std::array<TileInfo, static_cast<size_t>(TileType::Count)> kTileTable = {{
        /* Air        */ { "Air",        Color(0,0,0,0),         false, false, false, 0.0f,  TOOL_TIER_UNMINEABLE },
        /* Grass      */ { "Grass",      Color(86,158,64),       true,  true,  false, 0.5f,  0 },
        /* Dirt       */ { "Dirt",       Color(121,85,58),       true,  true,  false, 0.5f,  0 },
        /* Stone      */ { "Stone",      Color(130,130,135),     true,  true,  false, 1.0f,  1 },
        /* Sand       */ { "Sand",       Color(214,198,140),     true,  true,  false, 0.45f, 0 },
        /* Wood       */ { "Wood",       Color(133,94,66),       true,  true,  false, 0.8f,  0 },
        /* Leaves     */ { "Leaves",     Color(62,112,52),       false, true,  false, 0.15f, 0 },
        /* CoalOre    */ { "Coal Ore",   Color(92,90,96),        true,  true,  false, 1.1f,  1 },
        /* IronOre    */ { "Iron Ore",   Color(186,132,99),      true,  true,  false, 1.4f,  2 },
        /* GoldOre    */ { "Gold Ore",   Color(221,186,79),      true,  true,  false, 1.8f,  3 },
        /* DiamondOre */ { "Diamond Ore",Color(140,228,224),     true,  true,  false, 2.4f,  4 },
        /* Bedrock    */ { "Bedrock",    Color(42,40,46),        true,  false, false, 0.0f,  TOOL_TIER_UNMINEABLE },
        /* Water      */ { "Water",      Color(64,123,201,170),  false, false, false, 0.0f,  TOOL_TIER_UNMINEABLE },
        /* Torch      */ { "Torch",      Color(255,191,94),      false, true,  true,  0.1f,  0 },
    }};
}

const TileInfo& getTileInfo(TileType t) {
    size_t idx = static_cast<size_t>(t);
    if (idx >= kTileTable.size()) {
        throw std::out_of_range("getTileInfo: invalid TileType");
    }
    return kTileTable[idx];
}
