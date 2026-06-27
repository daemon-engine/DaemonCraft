//----------------------------------------------------------------------------------------------------
// RecipeRegistry.hpp
// Singleton registry for Recipe objects (Assignment 7 - A7-Core)
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include "Game/Definition/Recipe.hpp"
#include "Game/Definition/Registry.hpp"

#include <string>

//----------------------------------------------------------------------------------------------------
/// @brief Singleton registry for managing Recipe objects
/// @details Extends Registry<Recipe> with JSON loading capability. Recipes reference
///          ItemRegistry for ingredient/output name → ID resolution.
///          Must be loaded AFTER ItemRegistry for item name → ID mapping.
///
/// Usage:
///   RecipeRegistry::GetInstance().LoadFromJSON("Data/Definitions/Recipes.json");
///   Recipe* plankRecipe = RecipeRegistry::GetInstance().Get("oak_planks");
///   Recipe* recipe = RecipeRegistry::GetInstance().Get(3); // By ID
///
class RecipeRegistry : public Registry<Recipe>
{
public:
	// Singleton access
	static RecipeRegistry& GetInstance();

	// JSON loading (call AFTER ItemRegistry::LoadFromJSON)
	bool LoadFromJSON(std::string const& filePath);

private:
	// Singleton pattern: private constructor
	RecipeRegistry() = default;
	~RecipeRegistry() = default;

	// Delete copy/move constructors and assignment operators
	RecipeRegistry(RecipeRegistry const&)            = delete;
	RecipeRegistry(RecipeRegistry&&)                 = delete;
	RecipeRegistry& operator=(RecipeRegistry const&) = delete;
	RecipeRegistry& operator=(RecipeRegistry&&)      = delete;
};
