//----------------------------------------------------------------------------------------------------
// Recipe.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Definition/Recipe.hpp"

#include <algorithm>
#include <unordered_map>

//----------------------------------------------------------------------------------------------------
bool Recipe::Matches(std::array<uint16_t, 4> const& craftingGrid) const
{
	if (m_type == eRecipeType::SHAPED)
	{
		// Shaped recipe: Check exact pattern match
		// Pattern uses 0 to represent empty/any slot
		for (size_t i = 0; i < 4; ++i)
		{
			uint16_t patternItem = m_pattern[i];
			uint16_t gridItem    = craftingGrid[i];

			if (patternItem == 0)
			{
				// Pattern slot is empty/any - grid must be empty (0)
				if (gridItem != 0)
					return false;
			}
			else
			{
				// Pattern slot requires specific item
				if (gridItem != patternItem)
					return false;
			}
		}
		return true;
	}
	else // eRecipeType::SHAPELESS
	{
		// Shapeless recipe: Count ingredients, position doesn't matter
		// Build frequency map of required ingredients
		std::unordered_map<uint16_t, uint8_t> requiredCounts;
		for (uint16_t ingredientID : m_ingredients)
		{
			requiredCounts[ingredientID]++;
		}

		// Build frequency map of grid contents
		std::unordered_map<uint16_t, uint8_t> gridCounts;
		for (uint16_t gridItem : craftingGrid)
		{
			if (gridItem != 0) // Ignore empty slots
			{
				gridCounts[gridItem]++;
			}
		}

		// Check if counts match exactly
		if (requiredCounts.size() != gridCounts.size())
			return false;

		for (auto const& [itemID, count] : requiredCounts)
		{
			auto it = gridCounts.find(itemID);
			if (it == gridCounts.end() || it->second != count)
				return false;
		}

		return true;
	}
}
