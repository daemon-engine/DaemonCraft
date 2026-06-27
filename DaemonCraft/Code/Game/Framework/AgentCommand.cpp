//----------------------------------------------------------------------------------------------------
// AgentCommand.cpp
// Assignment 7-AI: Command implementations for autonomous agent actions
//----------------------------------------------------------------------------------------------------
#include "Game/Framework/AgentCommand.hpp"
//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/Agent.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Game/Gameplay/World.hpp"
#include "Game/Gameplay/Inventory.hpp"
//----------------------------------------------------------------------------------------------------
#include "Game/Framework/Block.hpp"
#include "Game/Framework/Chunk.hpp"
#include "Game/Framework/GameCommon.hpp"
//----------------------------------------------------------------------------------------------------
#include "Game/Definition/BlockDefinition.hpp"
#include "Game/Definition/BlockRegistry.hpp"
#include "Game/Definition/ItemRegistry.hpp"
#include "Game/Definition/ItemDefinition.hpp"
#include "Game/Definition/RecipeRegistry.hpp"
//----------------------------------------------------------------------------------------------------
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/MathUtils.hpp"
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
// MoveCommand - Navigate agent to target position
//----------------------------------------------------------------------------------------------------
MoveCommand::MoveCommand(Vec3 const& targetPosition, float moveSpeed)
	: m_targetPosition(targetPosition)
	, m_moveSpeed(moveSpeed)
{
}

void MoveCommand::Start()
{
	m_status = eCommandStatus::IN_PROGRESS;
}

eCommandStatus MoveCommand::Execute(float deltaSeconds, Agent* agent)
{
	Vec3 currentPos = agent->m_position;  // Entity has public m_position member
	Vec3 toTarget = m_targetPosition - currentPos;
	float distanceRemaining = toTarget.GetLength();

	// Check arrival
	if (distanceRemaining < ARRIVAL_THRESHOLD)
	{
		return eCommandStatus::COMPLETED;
	}

	// Move towards target
	Vec3 moveDirection = toTarget.GetNormalized();
	float moveDistance = m_moveSpeed * deltaSeconds;

	if (moveDistance > distanceRemaining)
	{
		moveDistance = distanceRemaining; // Don't overshoot
	}

	Vec3 newPos = currentPos + (moveDirection * moveDistance);
	agent->m_position = newPos;  // Directly assign to public member

	// NOTE: Collision detection is handled by Entity physics system in Agent::Update()
	// If agent is pushed back by collision, progress will be slower but command won't fail
	// This matches Minecraft behavior where mobs keep trying to pathfind around obstacles

	return eCommandStatus::IN_PROGRESS;
}

//----------------------------------------------------------------------------------------------------
// MineCommand - Progressive block breaking (reuses block breaking logic)
//----------------------------------------------------------------------------------------------------
MineCommand::MineCommand(IntVec3 const& blockCoords)
	: m_blockCoords(blockCoords)
{
}

void MineCommand::Start()
{
	m_status = eCommandStatus::IN_PROGRESS;
	m_miningProgress = 0.0f;
	m_miningDuration = 0.0f; // Will be calculated on first Execute
}

eCommandStatus MineCommand::Execute(float deltaSeconds, Agent* agent)
{
	// Get world reference
	Game* game = agent->m_game;  // Entity has public m_game member
	if (game == nullptr)
	{
		m_failureReason = "Agent has no game reference";
		return eCommandStatus::FAILED;
	}

	World* world = game->GetWorld();
	if (world == nullptr)
	{
		m_failureReason = "No active world";
		return eCommandStatus::FAILED;
	}

	// Check distance (same as Player mining distance)
	Vec3 blockCenter = Vec3(static_cast<float>(m_blockCoords.x) + 0.5f,
	                        static_cast<float>(m_blockCoords.y) + 0.5f,
	                        static_cast<float>(m_blockCoords.z) + 0.5f);
	float distance = GetDistance3D(agent->m_position, blockCenter);  // Entity has public m_position member
	if (distance > MAX_MINING_DISTANCE)
	{
		m_failureReason = "Block out of range";
		return eCommandStatus::FAILED;
	}

	// Get block type at target coords
	uint8_t blockType = world->GetBlockTypeAtGlobalCoords(m_blockCoords);
	if (blockType == 0)  // 0 = AIR
	{
		m_failureReason = "Block already mined";
		return eCommandStatus::COMPLETED; // Treat as success (block is gone either way)
	}

	// Calculate mining duration on first frame (from block definition)
	if (m_miningDuration == 0.0f)
	{
		sBlockDefinition const* blockDef = BlockRegistry::GetInstance().Get(blockType);
		if (blockDef == nullptr)
		{
			m_failureReason = "Invalid block definition";
			return eCommandStatus::FAILED;
		}

		// Minecraft-style hardness calculation
		// Solid blocks (stone, dirt): 1.5 seconds
		// Non-solid blocks (leaves, grass): 0.5 seconds
		float hardness = blockDef->IsSolid() ? 1.5f : 0.5f;
		float toolEffectiveness = 1.0f; // Hand mining (TODO: Use agent inventory tool when implemented)
		m_miningDuration = hardness / toolEffectiveness;
	}

	// Update mining progress
	m_miningProgress += deltaSeconds / m_miningDuration;

	// Check if block is broken
	if (m_miningProgress >= 1.0f)
	{
		// Get block definition for item drop
		sBlockDefinition const* blockDef = BlockRegistry::GetInstance().Get(blockType);

		// Destroy block
		world->SetBlockAtGlobalCoords(m_blockCoords, 0); // 0 = AIR

		// Spawn ItemEntity at block center (same pattern as Player::BreakBlock)
		if (blockDef != nullptr)
		{
			// Get corresponding item ID from ItemRegistry
			uint16_t itemID = ItemRegistry::GetInstance().GetItemIDByBlockType(blockType);
			if (itemID != 0)  // 0 = invalid item
			{
				ItemStack droppedItem(itemID, 1); // Drop 1 block item

				// Spawn ItemEntity slightly above block center (prevents clipping into ground)
				Vec3 spawnPosition = blockCenter + Vec3(0.0f, 0.0f, 0.3f);
				world->SpawnItemEntity(spawnPosition, droppedItem);
			}
		}

		return eCommandStatus::COMPLETED;
	}

	return eCommandStatus::IN_PROGRESS;
}

//----------------------------------------------------------------------------------------------------
// PlaceCommand - Block placement from inventory
//----------------------------------------------------------------------------------------------------
PlaceCommand::PlaceCommand(IntVec3 const& blockCoords, uint16_t itemID)
	: m_blockCoords(blockCoords)
	, m_itemID(itemID)
{
}

void PlaceCommand::Start()
{
	m_status = eCommandStatus::IN_PROGRESS;
}

eCommandStatus PlaceCommand::Execute(float deltaSeconds, Agent* agent)
{
	UNUSED(deltaSeconds);

	// Get world reference
	Game* game = agent->m_game;  // Entity has public m_game member
	if (game == nullptr)
	{
		m_failureReason = "Agent has no game reference";
		return eCommandStatus::FAILED;
	}

	World* world = game->GetWorld();
	if (world == nullptr)
	{
		m_failureReason = "No active world";
		return eCommandStatus::FAILED;
	}

	// Check distance
	Vec3 blockCenter = Vec3(static_cast<float>(m_blockCoords.x) + 0.5f,
	                        static_cast<float>(m_blockCoords.y) + 0.5f,
	                        static_cast<float>(m_blockCoords.z) + 0.5f);
	float distance = GetDistance3D(agent->m_position, blockCenter);  // Entity has public m_position member
	if (distance > MAX_PLACEMENT_DISTANCE)
	{
		m_failureReason = "Block position out of range";
		return eCommandStatus::FAILED;
	}

	// Check if block position is already occupied
	uint8_t existingBlockType = world->GetBlockTypeAtGlobalCoords(m_blockCoords);
	if (existingBlockType != 0)  // 0 = AIR
	{
		m_failureReason = "Block position already occupied";
		return eCommandStatus::FAILED;
	}

	// Check agent has the item in inventory
	Inventory& inv = agent->GetInventory();
	int itemCount = inv.CountItem(m_itemID);
	if (itemCount == 0)
	{
		m_failureReason = "Item not in inventory";
		return eCommandStatus::FAILED;
	}

	// Get corresponding block type from ItemRegistry
	sItemDefinition const* itemDef = ItemRegistry::GetInstance().Get(m_itemID);
	if (itemDef == nullptr)
	{
		m_failureReason = "Invalid item ID";
		return eCommandStatus::FAILED;
	}

	uint8_t blockType = itemDef->GetBlockTypeID();
	if (blockType == 0)
	{
		m_failureReason = "Item is not a placeable block";
		return eCommandStatus::FAILED;
	}

	// Attempt block placement
	bool placed = world->SetBlockAtGlobalCoords(m_blockCoords, blockType);
	if (!placed)
	{
		m_failureReason = "Block placement failed (invalid chunk or coordinates)";
		return eCommandStatus::FAILED;
	}

	// Remove 1 item from inventory
	bool removed = inv.RemoveItem(m_itemID, 1);
	if (!removed)
	{
		// This shouldn't happen (we checked CountItem above), but handle gracefully
		m_failureReason = "Failed to remove item from inventory";
		// Note: Block was already placed, so this creates 1 duplicate block (acceptable edge case)
		return eCommandStatus::FAILED;
	}

	return eCommandStatus::COMPLETED;
}

//----------------------------------------------------------------------------------------------------
// CraftCommand - Recipe execution (instant crafting)
//----------------------------------------------------------------------------------------------------
CraftCommand::CraftCommand(uint16_t recipeID)
	: m_recipeID(recipeID)
{
}

void CraftCommand::Start()
{
	m_status = eCommandStatus::IN_PROGRESS;
}

eCommandStatus CraftCommand::Execute(float deltaSeconds, Agent* agent)
{
	UNUSED(deltaSeconds);

	// Get recipe from registry
	Recipe const* recipe = RecipeRegistry::GetInstance().Get(m_recipeID);
	if (recipe == nullptr)
	{
		m_failureReason = "Invalid recipe ID";
		return eCommandStatus::FAILED;
	}

	// NOTE: Current Recipe/Inventory system doesn't have HasIngredients/RemoveIngredients methods
	// The Recipe class uses Matches() with a 2x2 crafting grid
	// For AI agents, we'll implement a simplified ingredient check and removal

	Inventory& inv = agent->GetInventory();

	// For shapeless recipes, we can check if agent has all ingredients
	// For shaped recipes, we'd need a crafting grid system (not implemented yet for agents)
	// For now, we'll return FAILED and document this as a TODO

	// TODO (Future Task): Implement agent crafting grid support
	// Requires either:
	// 1. Adding HasIngredients/RemoveIngredients to Inventory class
	// 2. Creating temporary 2x2 grid from agent inventory and using Recipe::Matches()
	// 3. Adding simplified crafting interface specifically for agents

	m_failureReason = "Agent crafting not yet implemented (requires crafting grid system)";
	return eCommandStatus::FAILED;

	// FUTURE IMPLEMENTATION (when Inventory has ingredient checking):
	/*
	// Check if agent has required ingredients
	if (!inv.HasIngredients(recipe->GetIngredients()))
	{
		m_failureReason = "Missing ingredients";
		return eCommandStatus::FAILED;
	}

	// Remove ingredients from inventory
	inv.RemoveIngredients(recipe->GetIngredients());

	// Get crafting result
	uint16_t outputItemID = recipe->GetOutputItemID();
	uint8_t outputQuantity = recipe->GetOutputQuantity();

	// Add result to inventory
	bool added = inv.AddItem(outputItemID, outputQuantity);
	if (!added)
	{
		m_failureReason = "Inventory full (cannot add crafting result)";
		// TODO: Should we return ingredients? (Edge case handling)
		return eCommandStatus::FAILED;
	}

	return eCommandStatus::COMPLETED;
	*/
}

//----------------------------------------------------------------------------------------------------
// WaitCommand - Timer delay (pauses agent for specified duration)
//----------------------------------------------------------------------------------------------------
WaitCommand::WaitCommand(float duration)
	: m_duration(duration)
{
}

void WaitCommand::Start()
{
	m_status = eCommandStatus::IN_PROGRESS;
	m_elapsedTime = 0.0f;
}

eCommandStatus WaitCommand::Execute(float deltaSeconds, Agent* agent)
{
	UNUSED(agent);

	m_elapsedTime += deltaSeconds;

	if (m_elapsedTime >= m_duration)
	{
		return eCommandStatus::COMPLETED;
	}

	return eCommandStatus::IN_PROGRESS;
}
