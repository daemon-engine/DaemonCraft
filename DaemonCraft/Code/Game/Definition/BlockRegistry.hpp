//----------------------------------------------------------------------------------------------------
// BlockRegistry.hpp
// Singleton registry for BlockDefinition objects (Assignment 7 - A7-Core)
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include "Game/Definition/BlockDefinition.hpp"
#include "Game/Definition/Registry.hpp"

#include <string>

//----------------------------------------------------------------------------------------------------
/// @brief Singleton registry for managing BlockDefinition objects
/// @details Extends Registry<sBlockDefinition> with JSON loading capability. Maintains block ID
///          order from existing XML definitions to preserve chunk save compatibility.
///
/// Usage:
///   BlockRegistry::GetInstance().LoadFromJSON("Data/Definitions/BlockDefinitions.json");
///   sBlockDefinition* grassDef = BlockRegistry::GetInstance().Get("Grass");
///   sBlockDefinition* stoneDef = BlockRegistry::GetInstance().Get(5); // By ID
///
class BlockRegistry : public Registry<sBlockDefinition>
{
public:
	// Singleton access
	static BlockRegistry& GetInstance();

	// JSON loading
	bool LoadFromJSON(std::string const& filePath);

private:
	// Singleton pattern: private constructor
	BlockRegistry() = default;
	~BlockRegistry() = default;

	// Delete copy/move constructors and assignment operators
	BlockRegistry(BlockRegistry const&) = delete;
	BlockRegistry(BlockRegistry&&) = delete;
	BlockRegistry& operator=(BlockRegistry const&) = delete;
	BlockRegistry& operator=(BlockRegistry&&) = delete;
};
