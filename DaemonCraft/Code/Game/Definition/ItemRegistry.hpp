//----------------------------------------------------------------------------------------------------
// ItemRegistry.hpp
// Singleton registry for ItemDefinition objects (Assignment 7 - A7-Core)
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include "Game/Definition/ItemDefinition.hpp"
#include "Game/Definition/Registry.hpp"

#include <string>

//----------------------------------------------------------------------------------------------------
/// @brief Singleton registry for managing ItemDefinition objects
/// @details Extends Registry<sItemDefinition> with JSON loading capability. Items can reference
///          BlockRegistry for block placement (BLOCK type items require valid blockType name).
///          Must be loaded AFTER BlockRegistry for blockType name → ID resolution.
///
/// Usage:
///   ItemRegistry::GetInstance().LoadFromJSON("Data/Definitions/ItemDefinitions.json");
///   sItemDefinition* pickaxe = ItemRegistry::GetInstance().Get("wooden_pickaxe");
///   sItemDefinition* dirt = ItemRegistry::GetInstance().Get(5); // By ID
///
class ItemRegistry : public Registry<sItemDefinition>
{
public:
	// Singleton access
	static ItemRegistry& GetInstance();

	// JSON loading (call AFTER BlockRegistry::LoadFromJSON)
	bool LoadFromJSON(std::string const& filePath);

	// Assignment 7: Block-to-Item mapping (for item drops when mining blocks)
	uint16_t GetItemIDByBlockType(uint16_t blockTypeID) const;

private:
	// Singleton pattern: private constructor
	ItemRegistry() = default;
	~ItemRegistry() = default;

	// Delete copy/move constructors and assignment operators
	ItemRegistry(ItemRegistry const&)            = delete;
	ItemRegistry(ItemRegistry&&)                 = delete;
	ItemRegistry& operator=(ItemRegistry const&) = delete;
	ItemRegistry& operator=(ItemRegistry&&)      = delete;
};
