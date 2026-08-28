#include "Inventory.h"
#include <algorithm>
#include <cstring>
#include <cstdint>

namespace {
    // Shared by addItem and craft()'s "would it fit" simulation.
    int tryAddToSlots(std::array<Slot, Config::INVENTORY_SIZE>& slots, ItemId id, int count) {
        if (id == ItemId::None || count <= 0) return 0;
        const ItemInfo& info = getItemInfo(id);
        int maxStack = info.maxStack;
        int remaining = count;

        for (auto& s : slots) {
            if (remaining <= 0) break;
            if (s.item == id && s.count < maxStack) {
                int canAdd = std::min(remaining, maxStack - s.count);
                s.count += canAdd;
                remaining -= canAdd;
            }
        }
        for (auto& s : slots) {
            if (remaining <= 0) break;
            if (s.item == ItemId::None) {
                int canAdd = std::min(remaining, maxStack);
                s.item = id;
                s.count = canAdd;
                remaining -= canAdd;
            }
        }
        return remaining;
    }
}

Inventory::Inventory() {
    slots.fill(Slot{});
}

int Inventory::addItem(ItemId id, int count) {
    return tryAddToSlots(slots, id, count);
}

bool Inventory::removeItem(ItemId id, int count) {
    if (id == ItemId::None || count <= 0) return true;
    if (countItem(id) < count) return false;

    int remaining = count;
    for (auto& s : slots) {
        if (remaining <= 0) break;
        if (s.item == id) {
            int take = std::min(remaining, s.count);
            s.count -= take;
            remaining -= take;
            if (s.count <= 0) { s.item = ItemId::None; s.count = 0; }
        }
    }
    return true;
}

int Inventory::countItem(ItemId id) const {
    int total = 0;
    for (auto& s : slots) if (s.item == id) total += s.count;
    return total;
}

bool Inventory::canCraft(const Recipe& r) const {
    for (auto& ing : r.ingredients) {
        if (countItem(ing.first) < ing.second) return false;
    }
    return true;
}

bool Inventory::craft(const Recipe& r) {
    if (!canCraft(r)) return false;

    // Simulate the output drop first so a full inventory fails cleanly
    // instead of consuming materials for a result that vanishes.
    auto sim = slots;
    if (tryAddToSlots(sim, r.result, r.resultCount) > 0) return false;

    for (auto& ing : r.ingredients) removeItem(ing.first, ing.second);
    tryAddToSlots(slots, r.result, r.resultCount);
    return true;
}

void Inventory::selectSlot(int index) {
    if (index < 0) index = 0;
    if (index >= hotbarSlots()) index = hotbarSlots() - 1;
    selected = index;
}

void Inventory::cycleSelected(int delta) {
    int n = hotbarSlots();
    selected = ((selected + delta) % n + n) % n;
}

bool Inventory::writeTo(std::ostream& os) const {
    const char magic[4] = {'T', 'I', 'N', 'V'};
    os.write(magic, 4);
    int32_t n = static_cast<int32_t>(slots.size());
    os.write(reinterpret_cast<const char*>(&n), sizeof(n));
    for (auto& s : slots) {
        uint8_t itemByte = static_cast<uint8_t>(s.item);
        int32_t count = s.count;
        os.write(reinterpret_cast<const char*>(&itemByte), sizeof(itemByte));
        os.write(reinterpret_cast<const char*>(&count), sizeof(count));
    }
    int32_t sel = selected;
    os.write(reinterpret_cast<const char*>(&sel), sizeof(sel));
    return os.good();
}

bool Inventory::readFrom(std::istream& is) {
    char magic[4];
    is.read(magic, 4);
    if (!is.good() || std::memcmp(magic, "TINV", 4) != 0) return false;

    int32_t n = 0;
    is.read(reinterpret_cast<char*>(&n), sizeof(n));
    if (!is.good() || n != static_cast<int32_t>(slots.size())) return false;

    for (int i = 0; i < n; i++) {
        uint8_t itemByte = 0;
        int32_t count = 0;
        is.read(reinterpret_cast<char*>(&itemByte), sizeof(itemByte));
        is.read(reinterpret_cast<char*>(&count), sizeof(count));
        if (!is.good()) return false;
        if (itemByte >= static_cast<uint8_t>(ItemId::Count)) return false;
        slots[static_cast<size_t>(i)].item = static_cast<ItemId>(itemByte);
        slots[static_cast<size_t>(i)].count = count;
    }

    int32_t sel = 0;
    is.read(reinterpret_cast<char*>(&sel), sizeof(sel));
    if (!is.good()) return false;
    selected = std::clamp(sel, 0, Config::HOTBAR_SIZE - 1);
    return true;
}
