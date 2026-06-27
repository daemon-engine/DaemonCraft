//----------------------------------------------------------------------------------------------------
// ItemStack.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/ItemStack.hpp"

#include "Game/Definition/ItemRegistry.hpp"

//----------------------------------------------------------------------------------------------------
bool ItemStack::IsFull() const
{
	// Empty slots are not full
	if (IsEmpty())
		return false;

	// Query ItemRegistry for max stack size
	sItemDefinition* itemDef = ItemRegistry::GetInstance().Get(itemID);
	if (!itemDef)
		return true; // Invalid item, consider full to prevent overflow

	return quantity >= itemDef->GetMaxStackSize();
}
