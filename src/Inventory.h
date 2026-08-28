#pragma once
#include "Item.h"
#include "Config.h"
#include <array>
#include <iostream>

struct Slot {
    ItemId item = ItemId::None;
    int count = 0;
};

class Inventory {
public:
    Inventory();

    // Stacks into existing slots first, then empty slots. Returns the
    // number of items that didn't fit (0 = everything was added).
    int addItem(ItemId id, int count);

    // Removes `count` of `id` from anywhere in the inventory. All-or-nothing:
    // returns false (and changes nothing) if the full amount isn't available.
    bool removeItem(ItemId id, int count);

    int countItem(ItemId id) const;

    bool canCraft(const Recipe& r) const;
    // Refuses (returns false, consumes nothing) if ingredients are missing
    // OR if the crafted result wouldn't fit anywhere - crafting never
    // destroys materials without producing the result.
    bool craft(const Recipe& r);

    int selectedSlot() const { return selected; }
    void selectSlot(int index);
    void cycleSelected(int delta); // mouse wheel; wraps within the hotbar
    ItemId selectedItem() const { return slots[static_cast<size_t>(selected)].item; }

    const Slot& slotAt(int index) const { return slots[static_cast<size_t>(index)]; }
    static constexpr int totalSlots() { return Config::INVENTORY_SIZE; }
    static constexpr int hotbarSlots() { return Config::HOTBAR_SIZE; }

    bool writeTo(std::ostream& os) const;
    bool readFrom(std::istream& is);

private:
    std::array<Slot, Config::INVENTORY_SIZE> slots;
    int selected = 0;
};
