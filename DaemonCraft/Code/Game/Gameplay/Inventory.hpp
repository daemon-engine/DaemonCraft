//----------------------------------------------------------------------------------------------------
// Inventory.hpp
//----------------------------------------------------------------------------------------------------
#pragma once

#include "Engine/Core/EngineCommon.hpp"
#include "Game/Gameplay/ItemStack.hpp"
#include "ThirdParty/json/json.hpp"

#include <array>

//----------------------------------------------------------------------------------------------------
// Inventory - 36-slot Minecraft-style inventory system
//
// Layout: [0-26] = Main inventory (27 slots), [27-35] = Hotbar (9 slots)
// Hotbar is bottom row in UI, main inventory is grid above it
//----------------------------------------------------------------------------------------------------
class Inventory
{
public:
	// Slot configuration
	static constexpr int MAIN_SLOT_COUNT   = 27;
	static constexpr int HOTBAR_SLOT_COUNT = 9;
	static constexpr int TOTAL_SLOT_COUNT  = 36;

	//------------------------------------------------------------------------------------------------
	// Constructor / Destructor
	//------------------------------------------------------------------------------------------------
	Inventory();
	~Inventory() = default;

	//------------------------------------------------------------------------------------------------
	// Slot Access
	//------------------------------------------------------------------------------------------------
	ItemStack&       GetSlot(int slotIndex);              // 0-35 (entire inventory)
	ItemStack const& GetSlot(int slotIndex) const;
	ItemStack&       GetMainSlot(int index);              // 0-26 (main inventory)
	ItemStack const& GetMainSlot(int index) const;
	ItemStack&       GetHotbarSlot(int index);            // 0-8 (hotbar)
	ItemStack const& GetHotbarSlot(int index) const;

	//------------------------------------------------------------------------------------------------
	// Hotbar Selection
	//------------------------------------------------------------------------------------------------
	int        GetSelectedHotbarSlot() const { return m_selectedHotbarSlot; }
	void       SetSelectedHotbarSlot(int slot); // Clamps to 0-8
	ItemStack& GetSelectedHotbarItemStack();
	ItemStack const& GetSelectedHotbarItemStack() const;

	//------------------------------------------------------------------------------------------------
	// Item Management
	//------------------------------------------------------------------------------------------------
	bool AddItem(uint16_t itemID, uint8_t quantity);  // Minecraft stacking behavior
	bool RemoveItem(uint16_t itemID, uint8_t quantity); // Remove from any slot(s)
	void SwapSlots(int slotA, int slotB);              // Exchange two slots
	bool MergeSlots(int sourceSlot, int targetSlot);   // Merge source into target

	//------------------------------------------------------------------------------------------------
	// Utility
	//------------------------------------------------------------------------------------------------
	void Clear(); // Clear all slots
	int  CountItem(uint16_t itemID) const; // Total quantity across all slots

	//------------------------------------------------------------------------------------------------
	// Serialization
	//------------------------------------------------------------------------------------------------
	nlohmann::json SaveToJSON() const;
	void           LoadFromJSON(nlohmann::json const& json);

private:
	std::array<ItemStack, TOTAL_SLOT_COUNT> m_slots;
	int m_selectedHotbarSlot = 0; // 0-8, index into hotbar slots [27-35]
};
