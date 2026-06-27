//----------------------------------------------------------------------------------------------------
// Inventory.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/Inventory.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Game/Definition/ItemRegistry.hpp"
#include "ThirdParty/json/json.hpp"

//----------------------------------------------------------------------------------------------------
using json = nlohmann::json;

//----------------------------------------------------------------------------------------------------
Inventory::Inventory()
{
	// Slots initialized to empty by ItemStack default constructor
}

//----------------------------------------------------------------------------------------------------
// Slot Access
//----------------------------------------------------------------------------------------------------
ItemStack& Inventory::GetSlot(int slotIndex)
{
	GUARANTEE_OR_DIE(slotIndex >= 0 && slotIndex < TOTAL_SLOT_COUNT, "Invalid slot index");
	return m_slots[slotIndex];
}

//----------------------------------------------------------------------------------------------------
ItemStack const& Inventory::GetSlot(int slotIndex) const
{
	GUARANTEE_OR_DIE(slotIndex >= 0 && slotIndex < TOTAL_SLOT_COUNT, "Invalid slot index");
	return m_slots[slotIndex];
}

//----------------------------------------------------------------------------------------------------
ItemStack& Inventory::GetMainSlot(int index)
{
	GUARANTEE_OR_DIE(index >= 0 && index < MAIN_SLOT_COUNT, "Invalid main slot index");
	return m_slots[index];
}

//----------------------------------------------------------------------------------------------------
ItemStack const& Inventory::GetMainSlot(int index) const
{
	GUARANTEE_OR_DIE(index >= 0 && index < MAIN_SLOT_COUNT, "Invalid main slot index");
	return m_slots[index];
}

//----------------------------------------------------------------------------------------------------
ItemStack& Inventory::GetHotbarSlot(int index)
{
	GUARANTEE_OR_DIE(index >= 0 && index < HOTBAR_SLOT_COUNT, "Invalid hotbar slot index");
	return m_slots[MAIN_SLOT_COUNT + index]; // Map [0-8] to [27-35]
}

//----------------------------------------------------------------------------------------------------
ItemStack const& Inventory::GetHotbarSlot(int index) const
{
	GUARANTEE_OR_DIE(index >= 0 && index < HOTBAR_SLOT_COUNT, "Invalid hotbar slot index");
	return m_slots[MAIN_SLOT_COUNT + index]; // Map [0-8] to [27-35]
}

//----------------------------------------------------------------------------------------------------
// Hotbar Selection
//----------------------------------------------------------------------------------------------------
void Inventory::SetSelectedHotbarSlot(int slot)
{
	// Clamp to valid range
	if (slot < 0)
		slot = 0;
	if (slot >= HOTBAR_SLOT_COUNT)
		slot = HOTBAR_SLOT_COUNT - 1;

	m_selectedHotbarSlot = slot;
}

//----------------------------------------------------------------------------------------------------
ItemStack& Inventory::GetSelectedHotbarItemStack()
{
	return GetHotbarSlot(m_selectedHotbarSlot);
}

//----------------------------------------------------------------------------------------------------
ItemStack const& Inventory::GetSelectedHotbarItemStack() const
{
	return GetHotbarSlot(m_selectedHotbarSlot);
}

//----------------------------------------------------------------------------------------------------
// Item Management
//----------------------------------------------------------------------------------------------------
bool Inventory::AddItem(uint16_t itemID, uint8_t quantity)
{
	if (quantity == 0 || itemID == 0)
		return true; // Nothing to add

	// Get item definition to check max stack size
	sItemDefinition* itemDef = ItemRegistry::GetInstance().Get(itemID);
	if (!itemDef)
	{
		DebuggerPrintf("[INVENTORY] AddItem FAILED - Invalid itemID=%u\n", itemID);
		return false; // Invalid item
	}

	DebuggerPrintf("[INVENTORY] AddItem called - itemID=%u, quantity=%u, maxStack=%u\n",
		itemID, quantity, itemDef->GetMaxStackSize());

	uint8_t maxStackSize = itemDef->GetMaxStackSize();
	uint8_t remaining = quantity;

	// Phase 1: Merge with existing non-full stacks in HOTBAR FIRST (Minecraft behavior)
	for (int i = MAIN_SLOT_COUNT; i < TOTAL_SLOT_COUNT && remaining > 0; ++i)
	{
		ItemStack& slot = m_slots[i];

		// Skip if slot is empty or different item
		if (slot.IsEmpty() || slot.itemID != itemID)
			continue;

		// Skip if slot is already full
		if (slot.IsFull())
			continue;

		// Calculate how much we can add to this stack
		uint8_t spaceAvailable = maxStackSize - slot.quantity;
		uint8_t amountToAdd = std::min(remaining, spaceAvailable);

		// Add to existing stack
		slot.Add(amountToAdd);
		remaining -= amountToAdd;
	}

	// Phase 1b: Then merge with existing non-full stacks in MAIN inventory
	for (int i = 0; i < MAIN_SLOT_COUNT && remaining > 0; ++i)
	{
		ItemStack& slot = m_slots[i];

		// Skip if slot is empty or different item
		if (slot.IsEmpty() || slot.itemID != itemID)
			continue;

		// Skip if slot is already full
		if (slot.IsFull())
			continue;

		// Calculate how much we can add to this stack
		uint8_t spaceAvailable = maxStackSize - slot.quantity;
		uint8_t amountToAdd = std::min(remaining, spaceAvailable);

		// Add to existing stack
		slot.Add(amountToAdd);
		remaining -= amountToAdd;
	}

	// Phase 2: Create new stacks in empty HOTBAR slots FIRST
	for (int i = MAIN_SLOT_COUNT; i < TOTAL_SLOT_COUNT && remaining > 0; ++i)
	{
		ItemStack& slot = m_slots[i];

		// Skip non-empty slots
		if (!slot.IsEmpty())
			continue;

		// Calculate how much to put in this new stack
		uint8_t amountToAdd = std::min(remaining, maxStackSize);

		// Create new stack
		slot.itemID = itemID;
		slot.quantity = amountToAdd;
		slot.durability = 0;
		remaining -= amountToAdd;
	}

	// Phase 2b: Then create new stacks in empty MAIN inventory slots
	for (int i = 0; i < MAIN_SLOT_COUNT && remaining > 0; ++i)
	{
		ItemStack& slot = m_slots[i];

		// Skip non-empty slots
		if (!slot.IsEmpty())
			continue;

		// Calculate how much to put in this new stack
		uint8_t amountToAdd = std::min(remaining, maxStackSize);

		// Create new stack
		slot.itemID = itemID;
		slot.quantity = amountToAdd;
		slot.durability = 0;
		remaining -= amountToAdd;
	}

	// Return true if all items were added, false if some remain
	bool success = remaining == 0;
	DebuggerPrintf("[INVENTORY] AddItem %s - remaining=%u\n", success ? "SUCCESS" : "PARTIAL", remaining);
	return success;
}

//----------------------------------------------------------------------------------------------------
bool Inventory::RemoveItem(uint16_t itemID, uint8_t quantity)
{
	if (quantity == 0 || itemID == 0)
		return true; // Nothing to remove

	// Count total available
	int totalAvailable = CountItem(itemID);
	if (totalAvailable < quantity)
		return false; // Not enough items

	uint8_t remaining = quantity;

	// Remove from slots (iterate backwards to prefer removing from end)
	for (int i = TOTAL_SLOT_COUNT - 1; i >= 0 && remaining > 0; --i)
	{
		ItemStack& slot = m_slots[i];

		// Skip if not the item we're looking for
		if (slot.itemID != itemID || slot.IsEmpty())
			continue;

		// Take what we can from this slot
		uint8_t amountToTake = std::min(remaining, slot.quantity);
		slot.Take(amountToTake);
		remaining -= amountToTake;
	}

	return remaining == 0;
}

//----------------------------------------------------------------------------------------------------
void Inventory::SwapSlots(int slotA, int slotB)
{
	GUARANTEE_OR_DIE(slotA >= 0 && slotA < TOTAL_SLOT_COUNT, "Invalid slotA index");
	GUARANTEE_OR_DIE(slotB >= 0 && slotB < TOTAL_SLOT_COUNT, "Invalid slotB index");

	// Simple swap using std::swap
	std::swap(m_slots[slotA], m_slots[slotB]);
}

//----------------------------------------------------------------------------------------------------
bool Inventory::MergeSlots(int sourceSlot, int targetSlot)
{
	GUARANTEE_OR_DIE(sourceSlot >= 0 && sourceSlot < TOTAL_SLOT_COUNT, "Invalid sourceSlot index");
	GUARANTEE_OR_DIE(targetSlot >= 0 && targetSlot < TOTAL_SLOT_COUNT, "Invalid targetSlot index");

	if (sourceSlot == targetSlot)
		return false; // Can't merge with self

	ItemStack& source = m_slots[sourceSlot];
	ItemStack& target = m_slots[targetSlot];

	// Can't merge if source is empty
	if (source.IsEmpty())
		return false;

	// If target is empty, just move the stack
	if (target.IsEmpty())
	{
		target = source;
		source.Clear();
		return true;
	}

	// Can't merge if different items
	if (source.itemID != target.itemID)
		return false;

	// Can't merge if target is already full
	if (target.IsFull())
		return false;

	// Get max stack size
	sItemDefinition* itemDef = ItemRegistry::GetInstance().Get(source.itemID);
	if (!itemDef)
		return false;

	uint8_t maxStackSize = itemDef->GetMaxStackSize();

	// Calculate how much we can transfer
	uint8_t spaceAvailable = maxStackSize - target.quantity;
	uint8_t amountToTransfer = std::min(source.quantity, spaceAvailable);

	// Transfer items
	source.Take(amountToTransfer);
	target.Add(amountToTransfer);

	return true;
}

//----------------------------------------------------------------------------------------------------
// Utility
//----------------------------------------------------------------------------------------------------
void Inventory::Clear()
{
	for (ItemStack& slot : m_slots)
	{
		slot.Clear();
	}
}

//----------------------------------------------------------------------------------------------------
int Inventory::CountItem(uint16_t itemID) const
{
	if (itemID == 0)
		return 0;

	int total = 0;
	for (ItemStack const& slot : m_slots)
	{
		if (slot.itemID == itemID)
		{
			total += slot.quantity;
		}
	}

	return total;
}

//----------------------------------------------------------------------------------------------------
// Serialization
//----------------------------------------------------------------------------------------------------
json Inventory::SaveToJSON() const
{
	json inventoryJson;
	inventoryJson["selectedHotbarSlot"] = m_selectedHotbarSlot;

	// Save slots array
	json slotsArray = json::array();
	for (int i = 0; i < TOTAL_SLOT_COUNT; ++i)
	{
		ItemStack const& slot = m_slots[i];

		// Only save non-empty slots to reduce file size
		if (!slot.IsEmpty())
		{
			json slotJson;
			slotJson["index"] = i;
			slotJson["itemID"] = slot.itemID;
			slotJson["quantity"] = slot.quantity;
			slotJson["durability"] = slot.durability;
			slotsArray.push_back(slotJson);
		}
	}

	inventoryJson["slots"] = slotsArray;
	return inventoryJson;
}

//----------------------------------------------------------------------------------------------------
void Inventory::LoadFromJSON(json const& inventoryJson)
{
	// Clear all slots first
	Clear();

	// Load selected hotbar slot
	if (inventoryJson.contains("selectedHotbarSlot"))
	{
		m_selectedHotbarSlot = inventoryJson["selectedHotbarSlot"].get<int>();
		// Clamp to valid range
		SetSelectedHotbarSlot(m_selectedHotbarSlot);
	}

	// Load slots
	if (inventoryJson.contains("slots") && inventoryJson["slots"].is_array())
	{
		json const& slotsArray = inventoryJson["slots"];
		for (auto const& slotJson : slotsArray)
		{
			int index = slotJson.value("index", -1);
			if (index < 0 || index >= TOTAL_SLOT_COUNT)
				continue; // Invalid index

			ItemStack& slot = m_slots[index];
			slot.itemID = slotJson.value("itemID", static_cast<uint16_t>(0));
			slot.quantity = slotJson.value("quantity", static_cast<uint8_t>(0));
			slot.durability = slotJson.value("durability", static_cast<uint16_t>(0));
		}
	}
}
