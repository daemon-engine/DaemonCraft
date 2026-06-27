//----------------------------------------------------------------------------------------------------
// ItemStack.hpp
//----------------------------------------------------------------------------------------------------
#pragma once

#define NOMINMAX // Prevent Windows.h from defining min/max macros

#include "Engine/Core/EngineCommon.hpp"

#include <algorithm> // For std::min

//----------------------------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------------------------
struct sItemDefinition;

//----------------------------------------------------------------------------------------------------
// ItemStack - Lightweight 6-byte structure representing a stack of items
//
// Used throughout inventory and crafting systems. Pass by value (6 bytes is efficient).
// itemID = 0 indicates an empty slot.
//----------------------------------------------------------------------------------------------------
struct ItemStack
{
	uint16_t itemID    = 0; // 0 = empty slot
	uint8_t  quantity  = 0; // 1-255 items in stack
	uint16_t durability = 0; // Tool durability (0 = broken, only for TOOL items)

	//------------------------------------------------------------------------------------------------
	// Query Methods
	//------------------------------------------------------------------------------------------------
	inline bool IsEmpty() const
	{
		return itemID == 0 || quantity == 0;
	}

	inline bool CanMergeWith(ItemStack const& other) const
	{
		// Can only merge if both have same item and neither is empty
		if (IsEmpty() || other.IsEmpty())
			return false;

		return itemID == other.itemID;
	}

	bool IsFull() const; // Implemented in .cpp to query ItemRegistry

	//------------------------------------------------------------------------------------------------
	// Mutation Methods
	//------------------------------------------------------------------------------------------------
	inline void Add(uint8_t amount)
	{
		// Add to quantity, clamping to 255 max
		uint32_t newQuantity = static_cast<uint32_t>(quantity) + static_cast<uint32_t>(amount);
		quantity = static_cast<uint8_t>((std::min)(newQuantity, 255U));
	}

	inline uint8_t Take(uint8_t amount)
	{
		// Take up to 'amount' items, return actual amount taken
		uint8_t taken = (std::min)(amount, quantity);
		quantity -= taken;

		// If quantity reaches 0, clear the slot
		if (quantity == 0)
		{
			Clear();
		}

		return taken;
	}

	inline void Clear()
	{
		itemID = 0;
		quantity = 0;
		durability = 0;
	}
};

// Verify size: Must be exactly 6 bytes for efficient pass-by-value
static_assert(sizeof(ItemStack) == 6, "ItemStack must be exactly 6 bytes");
