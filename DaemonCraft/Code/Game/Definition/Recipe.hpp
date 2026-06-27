//----------------------------------------------------------------------------------------------------
// Recipe.hpp
// Crafting recipe system for SimpleMiner (Assignment 7 - A7-Core)
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <array>
#include <cstdint>
#include <vector>

//----------------------------------------------------------------------------------------------------
/// @brief Recipe type classification for crafting system behavior
enum class eRecipeType : uint8_t
{
	SHAPED,    // Pattern-based crafting (e.g., pickaxe requires specific arrangement)
	SHAPELESS  // Ingredient-based crafting (e.g., planks can be in any position)
};

//----------------------------------------------------------------------------------------------------
/// @brief Crafting recipe definition for 2×2 crafting grid
/// @details Each recipe defines inputs (pattern or ingredients) and output (item + quantity).
///          Shaped recipes require specific 2×2 arrangements and check all rotations/mirrors.
///          Shapeless recipes only count ingredients regardless of position.
///
/// Pattern Layout (2×2 grid):
///   [0] [1]
///   [2] [3]
///
/// Usage:
///   Recipe const* recipe = RecipeRegistry::GetInstance().Get(5);
///   std::array<uint16_t, 4> grid = {plankID, plankID, plankID, plankID};
///   if (recipe->Matches(grid)) {
///       uint16_t outputID = recipe->GetOutputItemID();
///       uint8_t count = recipe->GetOutputQuantity();
///   }
///
class Recipe
{
public:
	// Constructor for initialization
	Recipe() = default;
	~Recipe() = default;

	// Accessors for recipe properties
	uint16_t     GetRecipeID() const { return m_recipeID; }
	eRecipeType  GetType() const { return m_type; }
	uint16_t     GetOutputItemID() const { return m_outputItemID; }
	uint8_t      GetOutputQuantity() const { return m_outputQuantity; }

	// Pattern matching for crafting grid
	bool Matches(std::array<uint16_t, 4> const& craftingGrid) const;

private:
	// Core properties
	uint16_t    m_recipeID       = 0;       // Unique recipe identifier
	eRecipeType m_type           = eRecipeType::SHAPELESS;
	uint16_t    m_outputItemID   = 0;       // Item produced by this recipe
	uint8_t     m_outputQuantity = 1;       // Number of items produced

	// Shaped recipe data (2×2 pattern, 0 = empty/any)
	// Pattern layout: [top-left, top-right, bottom-left, bottom-right]
	std::array<uint16_t, 4> m_pattern = {0, 0, 0, 0};

	// Shapeless recipe data (ingredient item IDs, duplicates allowed)
	std::vector<uint16_t> m_ingredients;

	// Allow RecipeRegistry to access private members for JSON loading
	friend class RecipeRegistry;
};
