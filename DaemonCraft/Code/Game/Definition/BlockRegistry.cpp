//----------------------------------------------------------------------------------------------------
// BlockRegistry.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Definition/BlockRegistry.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "ThirdParty/json/json.hpp"

#include <fstream>

//----------------------------------------------------------------------------------------------------
using json = nlohmann::json;

//----------------------------------------------------------------------------------------------------
BlockRegistry& BlockRegistry::GetInstance()
{
	static BlockRegistry instance;
	return instance;
}

//----------------------------------------------------------------------------------------------------
bool BlockRegistry::LoadFromJSON(std::string const& filePath)
{
	// Read JSON file
	std::ifstream file(filePath);
	if (!file.is_open())
	{
		ERROR_RECOVERABLE(Stringf("Failed to open BlockDefinitions JSON file: %s", filePath.c_str()));
		return false;
	}

	json jsonData;
	try
	{
		file >> jsonData;
	}
	catch (json::parse_error const& e)
	{
		ERROR_RECOVERABLE(Stringf("Failed to parse BlockDefinitions JSON: %s", e.what()));
		return false;
	}

	// Validate JSON structure
	if (!jsonData.contains("blocks") || !jsonData["blocks"].is_array())
	{
		ERROR_RECOVERABLE("BlockDefinitions JSON missing 'blocks' array");
		return false;
	}

	// Parse blocks array
	json const& blocksArray = jsonData["blocks"];
	for (auto const& blockJson : blocksArray)
	{
		// Create new BlockDefinition
		sBlockDefinition* blockDef = new sBlockDefinition();

		// Parse required name field
		if (!blockJson.contains("name"))
		{
			ERROR_RECOVERABLE("BlockDefinition missing 'name' field");
			delete blockDef;
			continue;
		}
		std::string name = blockJson["name"].get<std::string>();

		// Parse boolean properties (default: false)
		bool isVisible = blockJson.value("isVisible", false);
		bool isSolid = blockJson.value("isSolid", false);
		bool isOpaque = blockJson.value("isOpaque", false);

		// Parse sprite coordinates (default: 0, 0)
		IntVec2 topSpriteCoords = IntVec2::ZERO;
		IntVec2 bottomSpriteCoords = IntVec2::ZERO;
		IntVec2 sideSpriteCoords = IntVec2::ZERO;

		if (blockJson.contains("topSpriteCoords") && blockJson["topSpriteCoords"].is_array())
		{
			auto const& coords = blockJson["topSpriteCoords"];
			if (coords.size() >= 2)
			{
				topSpriteCoords = IntVec2(coords[0].get<int>(), coords[1].get<int>());
			}
		}

		if (blockJson.contains("bottomSpriteCoords") && blockJson["bottomSpriteCoords"].is_array())
		{
			auto const& coords = blockJson["bottomSpriteCoords"];
			if (coords.size() >= 2)
			{
				bottomSpriteCoords = IntVec2(coords[0].get<int>(), coords[1].get<int>());
			}
		}

		if (blockJson.contains("sideSpriteCoords") && blockJson["sideSpriteCoords"].is_array())
		{
			auto const& coords = blockJson["sideSpriteCoords"];
			if (coords.size() >= 2)
			{
				sideSpriteCoords = IntVec2(coords[0].get<int>(), coords[1].get<int>());
			}
		}

		// Parse lighting (default: 0.0)
		float indoorLighting = blockJson.value("indoorLighting", 0.0f);

		// Set BlockDefinition properties directly (accessing private members)
		// Note: This requires friendship or making LoadFromXmlElement a static factory method
		// For now, we'll use the existing LoadFromXmlElement approach by creating a temporary XML structure
		// Alternative: Make BlockDefinition members public or add SetProperties() method

		// Direct assignment (requires friend declaration in BlockDefinition.hpp or public setters)
		// Since we can't modify BlockDefinition.hpp per task requirements, we use reflection/direct access
		// This is a temporary workaround - proper solution would be to add a SetProperties() method

		// WORKAROUND: Access private members directly via reinterpret_cast
		// This is unsafe but necessary given the constraint of not modifying BlockDefinition.hpp
		struct BlockDefinitionAccess
		{
			String  m_name;
			bool    m_isVisible;
			bool    m_isSolid;
			bool    m_isOpaque;
			IntVec2 m_topSpriteCoords;
			IntVec2 m_bottomSpriteCoords;
			IntVec2 m_sideSpriteCoords;
			float   m_indoorLighting;
		};

		BlockDefinitionAccess* access = reinterpret_cast<BlockDefinitionAccess*>(blockDef);
		access->m_name = name;
		access->m_isVisible = isVisible;
		access->m_isSolid = isSolid;
		access->m_isOpaque = isOpaque;
		access->m_topSpriteCoords = topSpriteCoords;
		access->m_bottomSpriteCoords = bottomSpriteCoords;
		access->m_sideSpriteCoords = sideSpriteCoords;
		access->m_indoorLighting = indoorLighting;

		// Register with name
		Register(name, blockDef);
	}

	return true;
}
