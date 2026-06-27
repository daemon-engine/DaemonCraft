//----------------------------------------------------------------------------------------------------
// ItemDefinition.hpp
// Data-driven item configuration for SimpleMiner inventory system (Assignment 7 - A7-Core)
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Math/IntVec2.hpp"

#include <cstdint>

//----------------------------------------------------------------------------------------------------
/// @brief Item type classification for inventory system behavior
enum class eItemType : uint8_t
{
	RESOURCE,   // Raw materials (sticks, coal, etc.)
	TOOL,       // Mining/placement tools with durability (pickaxes, shovels)
	BLOCK,      // Placeable blocks that reference BlockDefinition
	CONSUMABLE  // Future: Food, potions, etc.
};

//----------------------------------------------------------------------------------------------------
/// @brief Data-driven item definition for SimpleMiner inventory system
/// @details Each item type has one ItemDefinition instance describing its properties.
///          Items can be resources, tools, blocks (placeable), or consumables.
///          Block items reference BlockRegistry for placement behavior.
///          Tool items have mining speed multipliers and durability limits.
///
/// Usage:
///   ItemDefinition const* pickaxe = ItemRegistry::GetInstance().Get("wooden_pickaxe");
///   if (pickaxe->GetType() == eItemType::TOOL) {
///       float speed = pickaxe->GetMiningSpeed();
///   }
///
struct sItemDefinition
{
	// Constructor for initialization
	sItemDefinition() = default;
	~sItemDefinition() = default;

	// Accessors for item properties
	String const& GetName() const { return m_name; }
	String const& GetDisplayName() const { return m_displayName; }
	eItemType     GetType() const { return m_type; }
	IntVec2       GetSpriteCoords() const { return m_spriteCoords; }
	uint8_t       GetMaxStackSize() const { return m_maxStackSize; }

	// Block item properties (valid when m_type == eItemType::BLOCK)
	bool     IsBlock() const { return m_type == eItemType::BLOCK; }
	uint16_t GetBlockTypeID() const { return m_blockTypeID; }

	// Tool item properties (valid when m_type == eItemType::TOOL)
	bool     IsTool() const { return m_type == eItemType::TOOL; }
	float    GetMiningSpeed() const { return m_miningSpeed; }
	uint16_t GetMaxDurability() const { return m_maxDurability; }

private:
	// Core properties (all items)
	String    m_name         = "UNKNOWN_ITEM";  // Internal identifier (e.g., "wooden_pickaxe")
	String    m_displayName  = "Unknown Item";  // User-facing name (e.g., "Wooden Pickaxe")
	eItemType m_type         = eItemType::RESOURCE;
	IntVec2   m_spriteCoords = IntVec2::ZERO;   // UV coordinates in item sprite sheet
	uint8_t   m_maxStackSize = 64;              // Max items per stack (1-64)

	// Block-specific properties (m_type == BLOCK)
	uint16_t m_blockTypeID = 0;  // References BlockRegistry for placement

	// Tool-specific properties (m_type == TOOL)
	float    m_miningSpeed   = 1.0f;  // Mining speed multiplier (1.0 = default)
	uint16_t m_maxDurability = 0;     // Tool durability (0 = infinite for testing)

	// Allow ItemRegistry to access private members for JSON loading
	friend class ItemRegistry;
};
