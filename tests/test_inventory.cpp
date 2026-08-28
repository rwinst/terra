#include "Inventory.h"
#include "Item.h"
#include <cstdio>
#include <sstream>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

void test_stacking_overflow() {
    printf("\n[test_stacking_overflow]\n");
    Inventory inv;
    int leftover = inv.addItem(ItemId::Stone, 2500); // way more than one stack (999)
    printf("  leftover after adding 2500 stone=%d, total counted=%d\n", leftover, inv.countItem(ItemId::Stone));
    CHECK(leftover == 0, "2500 stone should fit across multiple slots in a 30-slot inventory with no leftover");
    CHECK(inv.countItem(ItemId::Stone) == 2500, "total stone counted should equal what was added");

    // Fill the entire inventory completely with stone (30 slots * 999) then try to add 1 more of a NEW item type
    Inventory full;
    int totalCap = Inventory::totalSlots() * 999;
    int leftoverFull = full.addItem(ItemId::Stone, totalCap);
    CHECK(leftoverFull == 0, "filling every slot to capacity with one item should succeed with no leftover");
    int leftoverExtra = full.addItem(ItemId::Wood, 1); // no empty slots, no existing wood stack to merge into
    printf("  leftover trying to add 1 wood to a totally full inventory=%d\n", leftoverExtra);
    CHECK(leftoverExtra == 1, "a fully-stacked inventory should reject a new item type entirely");
}

void test_remove_all_or_nothing() {
    printf("\n[test_remove_all_or_nothing]\n");
    Inventory inv;
    inv.addItem(ItemId::Wood, 5);
    bool ok = inv.removeItem(ItemId::Wood, 10); // more than available
    CHECK(!ok, "removing more than available should fail");
    CHECK(inv.countItem(ItemId::Wood) == 5, "a failed removal must not partially consume the stack");

    bool ok2 = inv.removeItem(ItemId::Wood, 5);
    CHECK(ok2, "removing exactly the available amount should succeed");
    CHECK(inv.countItem(ItemId::Wood) == 0, "wood count should be zero after removing all of it");
}

void test_craft_success_and_consumption() {
    printf("\n[test_craft_success_and_consumption]\n");
    Inventory inv;
    inv.addItem(ItemId::Wood, 10);

    const Recipe* woodPick = nullptr;
    for (auto& r : getAllRecipes()) if (r.result == ItemId::PickaxeWood) woodPick = &r;
    CHECK(woodPick != nullptr, "wood pickaxe recipe should exist");

    bool crafted = inv.craft(*woodPick);
    printf("  crafted=%d wood left=%d pickaxes=%d\n", crafted, inv.countItem(ItemId::Wood), inv.countItem(ItemId::PickaxeWood));
    CHECK(crafted, "crafting a wood pickaxe with 10 wood (needs 6) should succeed");
    CHECK(inv.countItem(ItemId::Wood) == 4, "exactly 6 wood should be consumed");
    CHECK(inv.countItem(ItemId::PickaxeWood) == 1, "exactly 1 wood pickaxe should be produced");
}

void test_craft_fails_atomically_when_missing_ingredients() {
    printf("\n[test_craft_fails_atomically_when_missing_ingredients]\n");
    Inventory inv;
    inv.addItem(ItemId::Wood, 1); // need 6 for wood pickaxe, only have 1
    const Recipe* woodPick = nullptr;
    for (auto& r : getAllRecipes()) if (r.result == ItemId::PickaxeWood) woodPick = &r;

    bool crafted = inv.craft(*woodPick);
    printf("  crafted=%d wood remaining=%d\n", crafted, inv.countItem(ItemId::Wood));
    CHECK(!crafted, "crafting should fail when ingredients are insufficient");
    CHECK(inv.countItem(ItemId::Wood) == 1, "a failed craft must not consume any materials at all");
    CHECK(inv.countItem(ItemId::PickaxeWood) == 0, "a failed craft must not produce a result");
}

void test_craft_fails_atomically_when_inventory_full() {
    printf("\n[test_craft_fails_atomically_when_inventory_full]\n");
    Inventory inv;
    // give exactly the ingredients for a torch (1 wood + 1 coal -> 4 torches)
    inv.addItem(ItemId::Wood, 1);
    inv.addItem(ItemId::Coal, 1);
    // now fill every remaining slot with stone so there's no room for a NEW item type (torch)
    // inventory has 30 slots; 2 are used (wood, coal), fill the other 28 to capacity
    int filled = inv.addItem(ItemId::Stone, 28 * 999);
    CHECK(filled == 0, "setup: should be able to fill the remaining 28 slots with stone");

    const Recipe* torchRecipe = nullptr;
    for (auto& r : getAllRecipes()) if (r.result == ItemId::Torch) torchRecipe = &r;
    CHECK(torchRecipe != nullptr, "torch recipe should exist");

    int woodBefore = inv.countItem(ItemId::Wood);
    int coalBefore = inv.countItem(ItemId::Coal);
    bool crafted = inv.craft(*torchRecipe);
    printf("  crafted=%d wood before/after=%d/%d coal before/after=%d/%d\n",
        crafted, woodBefore, inv.countItem(ItemId::Wood), coalBefore, inv.countItem(ItemId::Coal));
    CHECK(!crafted, "crafting should fail when there's no room for the new result item");
    CHECK(inv.countItem(ItemId::Wood) == woodBefore, "materials must be untouched when the craft is rejected for space");
    CHECK(inv.countItem(ItemId::Coal) == coalBefore, "materials must be untouched when the craft is rejected for space");
}

void test_hotbar_selection() {
    printf("\n[test_hotbar_selection]\n");
    Inventory inv;
    inv.selectSlot(3);
    CHECK(inv.selectedSlot() == 3, "selectSlot should set the selected index");
    inv.selectSlot(999); // out of range, should clamp
    CHECK(inv.selectedSlot() == Inventory::hotbarSlots() - 1, "selectSlot should clamp to the last hotbar slot");
    inv.selectSlot(0);
    inv.cycleSelected(-1); // wrap backward from 0
    CHECK(inv.selectedSlot() == Inventory::hotbarSlots() - 1, "cycling backward from slot 0 should wrap to the last hotbar slot");
    inv.cycleSelected(1);
    CHECK(inv.selectedSlot() == 0, "cycling forward should wrap back to slot 0");
}

void test_serialization_roundtrip() {
    printf("\n[test_serialization_roundtrip]\n");
    Inventory inv;
    inv.addItem(ItemId::Wood, 17);
    inv.addItem(ItemId::Diamond, 3);
    inv.addItem(ItemId::PickaxeGold, 1);
    inv.selectSlot(2);

    std::ostringstream oss(std::ios::binary);
    bool wrote = inv.writeTo(oss);
    CHECK(wrote, "writeTo should succeed");

    Inventory loaded;
    std::istringstream iss(oss.str(), std::ios::binary);
    bool ok = loaded.readFrom(iss);
    CHECK(ok, "readFrom should succeed on data just written");
    CHECK(loaded.countItem(ItemId::Wood) == 17, "wood count should round-trip");
    CHECK(loaded.countItem(ItemId::Diamond) == 3, "diamond count should round-trip");
    CHECK(loaded.countItem(ItemId::PickaxeGold) == 1, "pickaxe count should round-trip");
    CHECK(loaded.selectedSlot() == 2, "selected slot should round-trip");
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    test_stacking_overflow();
    test_remove_all_or_nothing();
    test_craft_success_and_consumption();
    test_craft_fails_atomically_when_missing_ingredients();
    test_craft_fails_atomically_when_inventory_full();
    test_hotbar_selection();
    test_serialization_roundtrip();
    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
