//----------------------------------------------------------------------------------------------------
// ItemRegistry.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Definition/ItemRegistry.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Game/Definition/BlockRegistry.hpp"
#include "ThirdParty/json/json.hpp"

#include <fstream>

//----------------------------------------------------------------------------------------------------
using json = nlohmann::json;

//----------------------------------------------------------------------------------------------------
ItemRegistry& ItemRegistry::GetInstance()
{
	static ItemRegistry instance;
	return instance;
}

//----------------------------------------------------------------------------------------------------
bool ItemRegistry::LoadFromJSON(std::string const& filePath)
{
	// Read JSON file
	std::ifstream file(filePath);
	if (!file.is_open())
	{
		ERROR_RECOVERABLE(Stringf("Failed to open ItemDefinitions JSON file: %s", filePath.c_str()));
		return false;
	}

	json jsonData;
	try
	{
		file >> jsonData;
	}
	catch (json::parse_error const& e)
	{
		ERROR_RECOVERABLE(Stringf("Failed to parse ItemDefinitions JSON: %s", e.what()));
		return false;
	}

	// Validate JSON structure
	if (!jsonData.contains("items") || !jsonData["items"].is_array())
	{
		ERROR_RECOVERABLE("ItemDefinitions JSON missing 'items' array");
		return false;
	}

	// Parse items array
	json const& itemsArray = jsonData["items"];
	for (auto const& itemJson : itemsArray)
	{
		// Create new ItemDefinition
		sItemDefinition* itemDef = new sItemDefinition();

		// Parse required name field
		if (!itemJson.contains("name"))
		{
			ERROR_RECOVERABLE("ItemDefinition missing 'name' field");
			delete itemDef;
			continue;
		}
		std::string name = itemJson["name"].get<std::string>();

		// Parse display name (default: name with capital first letter)
		std::string displayName = itemJson.value("displayName", name);

		// Parse item type (default: RESOURCE)
		std::string typeStr = itemJson.value("type", "RESOURCE");
		eItemType   type    = eItemType::RESOURCE;
		if (typeStr == "TOOL")
			type = eItemType::TOOL;
		else if (typeStr == "BLOCK")
			type = eItemType::BLOCK;
		else if (typeStr == "CONSUMABLE")
			type = eItemType::CONSUMABLE;

		// Parse sprite coordinates (default: 0, 0)
		IntVec2 spriteCoords = IntVec2::ZERO;
		if (itemJson.contains("spriteCoords") && itemJson["spriteCoords"].is_array())
		{
			auto const& coords = itemJson["spriteCoords"];
			if (coords.size() >= 2)
			{
				spriteCoords = IntVec2(coords[0].get<int>(), coords[1].get<int>());
			}
		}

		// Parse max stack size (default: 64, tools: 1)
		uint8_t maxStackSize = itemJson.value("maxStackSize", 64);
		if (type == eItemType::TOOL && !itemJson.contains("maxStackSize"))
		{
			maxStackSize = 1; // Tools default to non-stackable
		}

		// Parse block-specific properties
		uint16_t blockTypeID = 0;
		if (type == eItemType::BLOCK && itemJson.contains("blockType"))
		{
			std::string blockTypeName = itemJson["blockType"].get<std::string>();

			// Resolve blockType name to BlockRegistry ID
			sBlockDefinition* blockDef = BlockRegistry::GetInstance().Get(blockTypeName);
			if (blockDef)
			{
				blockTypeID = BlockRegistry::GetInstance().GetID(blockTypeName);
			}
			else
			{
				ERROR_RECOVERABLE(Stringf("ItemDefinition '%s': Unknown blockType '%s'", name.c_str(), blockTypeName.c_str()));
			}
		}

		// Parse tool-specific properties
		float    miningSpeed   = itemJson.value("miningSpeed", 1.0f);
		uint16_t maxDurability = itemJson.value("maxDurability", 0);

		// Set ItemDefinition properties via friend access
		itemDef->m_name         = name;
		itemDef->m_displayName  = displayName;
		itemDef->m_type         = type;
		itemDef->m_spriteCoords = spriteCoords;
		itemDef->m_maxStackSize = maxStackSize;
		itemDef->m_blockTypeID  = blockTypeID;
		itemDef->m_miningSpeed  = miningSpeed;
		itemDef->m_maxDurability = maxDurability;

		// Register with name
		Register(name, itemDef);
	}

	return true;
}

//----------------------------------------------------------------------------------------------------
// Assignment 7: Reverse-lookup item ID by block type ID
// Used for determining item drops when mining blocks
//----------------------------------------------------------------------------------------------------
uint16_t ItemRegistry::GetItemIDByBlockType(uint16_t blockTypeID) const
{
	DebuggerPrintf("[ITEMREGISTRY] GetItemIDByBlockType called - searching for blockTypeID=%u\n", blockTypeID);

	// Iterate through all registered items
	std::vector<sItemDefinition*> const& allItems = GetAll();
	for (size_t i = 0; i < allItems.size(); ++i)
	{
		sItemDefinition* itemDef = allItems[i];
		if (itemDef != nullptr && itemDef->GetType() == eItemType::BLOCK)
		{
			uint16_t storedBlockTypeID = itemDef->GetBlockTypeID();
			DebuggerPrintf("[ITEMREGISTRY]   Item[%d] '%s' (type=BLOCK) has blockTypeID=%u\n",
				static_cast<int>(i), itemDef->GetName().c_str(), storedBlockTypeID);

			// Check if this BLOCK-type item matches the requested block type
			if (storedBlockTypeID == blockTypeID)
			{
				DebuggerPrintf("[ITEMREGISTRY] MATCH FOUND! Returning itemID=%d\n", static_cast<int>(i));
				return static_cast<uint16_t>(i);  // Return item ID (index in registry)
			}
		}
	}

	// No item found for this block type
	DebuggerPrintf("[ITEMREGISTRY] NO MATCH FOUND for blockTypeID=%u, returning -1\n", blockTypeID);
	return static_cast<uint16_t>(-1);  // Invalid ID (0xFFFF)
}
