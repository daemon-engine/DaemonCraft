//----------------------------------------------------------------------------------------------------
// RecipeRegistry.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Definition/RecipeRegistry.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Game/Definition/ItemRegistry.hpp"
#include "ThirdParty/json/json.hpp"

#include <fstream>

//----------------------------------------------------------------------------------------------------
using json = nlohmann::json;

//----------------------------------------------------------------------------------------------------
RecipeRegistry& RecipeRegistry::GetInstance()
{
	static RecipeRegistry instance;
	return instance;
}

//----------------------------------------------------------------------------------------------------
bool RecipeRegistry::LoadFromJSON(std::string const& filePath)
{
	// Read JSON file
	std::ifstream file(filePath);
	if (!file.is_open())
	{
		ERROR_RECOVERABLE(Stringf("Failed to open Recipes JSON file: %s", filePath.c_str()));
		return false;
	}

	json jsonData;
	try
	{
		file >> jsonData;
	}
	catch (json::parse_error const& e)
	{
		ERROR_RECOVERABLE(Stringf("Failed to parse Recipes JSON: %s", e.what()));
		return false;
	}

	// Validate JSON structure
	if (!jsonData.contains("recipes") || !jsonData["recipes"].is_array())
	{
		ERROR_RECOVERABLE("Recipes JSON missing 'recipes' array");
		return false;
	}

	// Parse recipes array
	json const& recipesArray = jsonData["recipes"];
	for (auto const& recipeJson : recipesArray)
	{
		// Create new Recipe
		Recipe* recipe = new Recipe();

		// Parse required name field (for registry lookup)
		if (!recipeJson.contains("name"))
		{
			ERROR_RECOVERABLE("Recipe missing 'name' field");
			delete recipe;
			continue;
		}
		std::string name = recipeJson["name"].get<std::string>();

		// Parse recipe type (default: SHAPELESS)
		std::string typeStr = recipeJson.value("type", "shapeless");
		eRecipeType type    = eRecipeType::SHAPELESS;
		if (typeStr == "shaped")
			type = eRecipeType::SHAPED;

		// Parse output (result)
		if (!recipeJson.contains("result"))
		{
			ERROR_RECOVERABLE(Stringf("Recipe '%s' missing 'result' field", name.c_str()));
			delete recipe;
			continue;
		}

		json const& resultJson    = recipeJson["result"];
		std::string outputItemName = resultJson.value("item", "");
		uint8_t     outputQuantity = static_cast<uint8_t>(resultJson.value("count", 1));

		// Resolve output item name to ItemRegistry ID
		sItemDefinition* outputItem = ItemRegistry::GetInstance().Get(outputItemName);
		if (!outputItem)
		{
			ERROR_RECOVERABLE(Stringf("Recipe '%s': Unknown output item '%s'", name.c_str(), outputItemName.c_str()));
			delete recipe;
			continue;
		}
		uint16_t outputItemID = ItemRegistry::GetInstance().GetID(outputItemName);

		// Parse type-specific recipe data
		std::array<uint16_t, 4> pattern     = {0, 0, 0, 0};
		std::vector<uint16_t>   ingredients;

		if (type == eRecipeType::SHAPED)
		{
			// Parse 2×2 pattern array
			if (!recipeJson.contains("pattern") || !recipeJson["pattern"].is_array())
			{
				ERROR_RECOVERABLE(Stringf("Shaped recipe '%s' missing 'pattern' array", name.c_str()));
				delete recipe;
				continue;
			}

			json const& patternJson = recipeJson["pattern"];
			if (patternJson.size() != 2)
			{
				ERROR_RECOVERABLE(Stringf("Shaped recipe '%s' pattern must have 2 rows", name.c_str()));
				delete recipe;
				continue;
			}

			// Parse pattern rows (2×2 grid)
			for (size_t row = 0; row < 2; ++row)
			{
				if (!patternJson[row].is_array() || patternJson[row].size() != 2)
				{
					ERROR_RECOVERABLE(Stringf("Shaped recipe '%s' pattern row %zu must have 2 columns", name.c_str(), row));
					delete recipe;
					continue;
				}

				for (size_t col = 0; col < 2; ++col)
				{
					std::string itemName = patternJson[row][col].get<std::string>();
					size_t      index    = row * 2 + col;

					if (itemName.empty() || itemName == "air" || itemName == "empty")
					{
						pattern[index] = 0; // Empty slot
					}
					else
					{
						// Resolve item name to ItemRegistry ID
						sItemDefinition* item = ItemRegistry::GetInstance().Get(itemName);
						if (!item)
						{
							ERROR_RECOVERABLE(Stringf("Recipe '%s': Unknown ingredient '%s'", name.c_str(), itemName.c_str()));
							pattern[index] = 0;
						}
						else
						{
							pattern[index] = ItemRegistry::GetInstance().GetID(itemName);
						}
					}
				}
			}
		}
		else // eRecipeType::SHAPELESS
		{
			// Parse ingredients array
			if (!recipeJson.contains("ingredients") || !recipeJson["ingredients"].is_array())
			{
				ERROR_RECOVERABLE(Stringf("Shapeless recipe '%s' missing 'ingredients' array", name.c_str()));
				delete recipe;
				continue;
			}

			json const& ingredientsJson = recipeJson["ingredients"];
			for (auto const& ingredientName : ingredientsJson)
			{
				std::string itemName = ingredientName.get<std::string>();

				// Resolve item name to ItemRegistry ID
				sItemDefinition* item = ItemRegistry::GetInstance().Get(itemName);
				if (!item)
				{
					ERROR_RECOVERABLE(Stringf("Recipe '%s': Unknown ingredient '%s'", name.c_str(), itemName.c_str()));
					continue;
				}

				uint16_t itemID = ItemRegistry::GetInstance().GetID(itemName);
				ingredients.push_back(itemID);
			}
		}

		// Set Recipe properties via friend access
		recipe->m_recipeID       = static_cast<uint16_t>(GetCount()); // Auto-assign sequential ID
		recipe->m_type           = type;
		recipe->m_outputItemID   = outputItemID;
		recipe->m_outputQuantity = outputQuantity;
		recipe->m_pattern        = pattern;
		recipe->m_ingredients    = ingredients;

		// Register with name
		Register(name, recipe);
	}

	return true;
}
