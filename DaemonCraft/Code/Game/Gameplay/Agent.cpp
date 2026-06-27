//----------------------------------------------------------------------------------------------------
// Agent.cpp
// Assignment 7-AI: AI-controlled entity with inventory and command queue
//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/Agent.hpp"
//----------------------------------------------------------------------------------------------------
#include "Game/Framework/AgentCommand.hpp" // Assignment 7-AI: Command execution (Task 3dfa0ec6)
#include "Game/Framework/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Game/Gameplay/World.hpp"
#include "Game/Gameplay/Player.hpp"       // For GetNearbyEntities() vision system
#include "Game/Gameplay/ItemEntity.hpp"   // For GetNearbyEntities() vision system
#include "Game/Definition/BlockRegistry.hpp"
//----------------------------------------------------------------------------------------------------
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Renderer/VertexUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------------------------------
Agent::Agent(Game* owner, std::string const& name, uint64_t agentID, Vec3 const& position)
	: Entity(owner)
	, m_agentName(name)
	, m_agentID(agentID)
	, m_inventory() // 36 slots by default (Inventory constructor)
{
	m_position = position;

	// Agent rendering: 2×2×2 block tall green wireframe cube
	// Physics AABB: 0.8 blocks wide (X/Y), 2.0 blocks tall (Z)
	// Center AABB on entity position
	m_physicsAABB = AABB3(Vec3(-0.4f, -0.4f, 0.0f), Vec3(0.4f, 0.4f, 2.0f));

	// Enable flying physics (no gravity, free movement for AI navigation)
	// AI agents need to navigate to ore blocks freely without getting stuck in terrain
	m_physicsMode = ePhysicsMode::FLYING;
	m_physicsEnabled = true;

	DebuggerPrintf("Agent '%s' (ID: %llu) spawned at (%.1f, %.1f, %.1f)\n",
		m_agentName.c_str(), m_agentID, position.x, position.y, position.z);
}

//----------------------------------------------------------------------------------------------------
// Destructor - Clean up command queue
//----------------------------------------------------------------------------------------------------
Agent::~Agent()
{
	// Delete current command
	if (m_currentCommand != nullptr)
	{
		delete m_currentCommand;
		m_currentCommand = nullptr;
	}

	// Delete all queued commands
	while (!m_commandQueue.empty())
	{
		AgentCommand* command = m_commandQueue.front();
		m_commandQueue.pop();
		delete command;
	}

	DebuggerPrintf("Agent '%s' (ID: %llu) destroyed\n", m_agentName.c_str(), m_agentID);
}

//----------------------------------------------------------------------------------------------------
// Update - Process physics and command queue
//----------------------------------------------------------------------------------------------------
void Agent::Update(float deltaSeconds)
{
	// Step 1: Run Entity physics (gravity, collision, etc.)
	Entity::Update(deltaSeconds);

	// Step 2: Process command queue
	ProcessCommandQueue(deltaSeconds);

	// DEBUG: Log agent state every 60 frames (~1 second at 60fps)
	static int frameCount = 0;
	frameCount++;
	if (frameCount % 60 == 0)
	{
		DebuggerPrintf("Agent '%s' Update(): Position=(%.1f, %.1f, %.1f), Commands=%d, Executing=%s\n",
			m_agentName.c_str(), m_position.x, m_position.y, m_position.z,
			GetCommandQueueSize(), IsExecutingCommand() ? "YES" : "NO");
	}
}

//----------------------------------------------------------------------------------------------------
// Render - Draw green wireframe cube (2×2×2 blocks tall)
//----------------------------------------------------------------------------------------------------
void Agent::Render() const
{
	// Get world-space AABB for rendering
	AABB3 worldAABB = GetWorldAABB();

	// Render green wireframe cube (distinguishes agents from players during development)
	Rgba8 agentColor = Rgba8::GREEN; // Green = Agent, Blue = Player (Player uses Rgba8::BLUE)
	float lineThickness = 0.02f;     // Thin wireframe lines
	float lineDuration = 0.0f;       // Single frame (re-drawn every frame)
	eDebugRenderMode mode = eDebugRenderMode::X_RAY; // Always visible through terrain

	// Draw wireframe box using DebugRenderSystem (same pattern as Player::Render)
	// Bottom face (4 edges at z = mins.z)
	Vec3 bottomBL = Vec3(worldAABB.m_mins.x, worldAABB.m_mins.y, worldAABB.m_mins.z);
	Vec3 bottomBR = Vec3(worldAABB.m_maxs.x, worldAABB.m_mins.y, worldAABB.m_mins.z);
	Vec3 bottomTR = Vec3(worldAABB.m_maxs.x, worldAABB.m_maxs.y, worldAABB.m_mins.z);
	Vec3 bottomTL = Vec3(worldAABB.m_mins.x, worldAABB.m_maxs.y, worldAABB.m_mins.z);

	DebugAddWorldLine(bottomBL, bottomBR, lineThickness, lineDuration, agentColor, agentColor, mode);
	DebugAddWorldLine(bottomBR, bottomTR, lineThickness, lineDuration, agentColor, agentColor, mode);
	DebugAddWorldLine(bottomTR, bottomTL, lineThickness, lineDuration, agentColor, agentColor, mode);
	DebugAddWorldLine(bottomTL, bottomBL, lineThickness, lineDuration, agentColor, agentColor, mode);

	// Top face (4 edges at z = maxs.z)
	Vec3 topBL = Vec3(worldAABB.m_mins.x, worldAABB.m_mins.y, worldAABB.m_maxs.z);
	Vec3 topBR = Vec3(worldAABB.m_maxs.x, worldAABB.m_mins.y, worldAABB.m_maxs.z);
	Vec3 topTR = Vec3(worldAABB.m_maxs.x, worldAABB.m_maxs.y, worldAABB.m_maxs.z);
	Vec3 topTL = Vec3(worldAABB.m_mins.x, worldAABB.m_maxs.y, worldAABB.m_maxs.z);

	DebugAddWorldLine(topBL, topBR, lineThickness, lineDuration, agentColor, agentColor, mode);
	DebugAddWorldLine(topBR, topTR, lineThickness, lineDuration, agentColor, agentColor, mode);
	DebugAddWorldLine(topTR, topTL, lineThickness, lineDuration, agentColor, agentColor, mode);
	DebugAddWorldLine(topTL, topBL, lineThickness, lineDuration, agentColor, agentColor, mode);

	// Vertical edges (4 edges connecting bottom to top)
	DebugAddWorldLine(bottomBL, topBL, lineThickness, lineDuration, agentColor, agentColor, mode);
	DebugAddWorldLine(bottomBR, topBR, lineThickness, lineDuration, agentColor, agentColor, mode);
	DebugAddWorldLine(bottomTR, topTR, lineThickness, lineDuration, agentColor, agentColor, mode);
	DebugAddWorldLine(bottomTL, topTL, lineThickness, lineDuration, agentColor, agentColor, mode);

	// Draw green sphere at agent position (origin marker)
	DebugAddWorldWireSphere(m_position, 0.1f, lineDuration, agentColor, agentColor, mode);
}

//----------------------------------------------------------------------------------------------------
// QueueCommand - Add command to queue (heap-allocated, caller transfers ownership)
//----------------------------------------------------------------------------------------------------
void Agent::QueueCommand(AgentCommand* command)
{
	if (command == nullptr)
	{
		ERROR_RECOVERABLE("Agent::QueueCommand: Attempted to queue nullptr command");
		return;
	}

	m_commandQueue.push(command);

	DebuggerPrintf("Agent '%s': Queued command (queue size: %d)\n",
		m_agentName.c_str(), GetCommandQueueSize());
}

//----------------------------------------------------------------------------------------------------
// GetCurrentCommandType - Return type string of currently executing command
//----------------------------------------------------------------------------------------------------
std::string Agent::GetCurrentCommandType() const
{
	if (m_currentCommand == nullptr)
	{
		return "NONE";
	}
	return m_currentCommand->GetType();
}

//----------------------------------------------------------------------------------------------------
// ClearCommandQueue - Delete all pending commands (emergency stop)
//----------------------------------------------------------------------------------------------------
void Agent::ClearCommandQueue()
{
	// Delete current command
	if (m_currentCommand != nullptr)
	{
		delete m_currentCommand;
		m_currentCommand = nullptr;
	}

	// Delete all queued commands
	while (!m_commandQueue.empty())
	{
		AgentCommand* command = m_commandQueue.front();
		m_commandQueue.pop();
		delete command;
	}

	DebuggerPrintf("Agent '%s': Command queue cleared\n", m_agentName.c_str());
}

//----------------------------------------------------------------------------------------------------
// ProcessCommandQueue - Dequeue next command if idle, execute current command
//----------------------------------------------------------------------------------------------------
void Agent::ProcessCommandQueue(float deltaSeconds)
{
	// If no current command, dequeue next command
	if (m_currentCommand == nullptr && !m_commandQueue.empty())
	{
		m_currentCommand = m_commandQueue.front();
		m_commandQueue.pop();

		// Initialize command (Task 3dfa0ec6)
		m_currentCommand->Start();

		DebuggerPrintf("Agent '%s': Started new command '%s' (queue size: %d)\n",
			m_agentName.c_str(), m_currentCommand->GetType().c_str(), GetCommandQueueSize());
	}

	// Execute current command
	if (m_currentCommand != nullptr)
	{
		ExecuteCurrentCommand(deltaSeconds);
	}
}

//----------------------------------------------------------------------------------------------------
// ExecuteCurrentCommand - Execute current command, handle completion/failure
// Task 3dfa0ec6: Now implemented with AgentCommand::Execute()
//----------------------------------------------------------------------------------------------------
void Agent::ExecuteCurrentCommand(float deltaSeconds)
{
	// Execute command and get status
	eCommandStatus status = m_currentCommand->Execute(deltaSeconds, this);

	// Handle command completion or failure
	if (status == eCommandStatus::COMPLETED)
	{
		DebuggerPrintf("Agent '%s': Command '%s' completed\n",
			m_agentName.c_str(), m_currentCommand->GetType().c_str());
		CompleteCurrentCommand();
	}
	else if (status == eCommandStatus::FAILED)
	{
		DebuggerPrintf("Agent '%s': Command '%s' failed: %s\n",
			m_agentName.c_str(),
			m_currentCommand->GetType().c_str(),
			m_currentCommand->GetFailureReason().c_str());
		CompleteCurrentCommand();
	}
	// Status IN_PROGRESS: Continue executing next frame (do nothing)
}

//----------------------------------------------------------------------------------------------------
// CompleteCurrentCommand - Delete finished command and reset state
//----------------------------------------------------------------------------------------------------
void Agent::CompleteCurrentCommand()
{
	if (m_currentCommand != nullptr)
	{
		delete m_currentCommand;
		m_currentCommand = nullptr;
	}
}

//----------------------------------------------------------------------------------------------------
// GetNearbyBlocks - Query all non-air blocks within radius
// Assignment 7-AI: Vision system for environment perception (Task eb0e3f92)
//----------------------------------------------------------------------------------------------------
std::vector<Agent::BlockInfo> Agent::GetNearbyBlocks(float radius) const
{
	std::vector<BlockInfo> result;

	// Get World reference (need to query blocks)
	if (m_game == nullptr)
	{
		ERROR_RECOVERABLE("Agent::GetNearbyBlocks: m_game is nullptr");
		return result;
	}

	World* world = m_game->GetWorld();
	if (world == nullptr)
	{
		ERROR_RECOVERABLE("Agent::GetNearbyBlocks: World is nullptr");
		return result;
	}

	// Calculate search range in blocks (add 1 to cover partial blocks)
	int radiusBlocks = static_cast<int>(radius) + 1;

	// Triple nested loop: search in cube around agent position
	for (int dx = -radiusBlocks; dx <= radiusBlocks; ++dx)
	{
		for (int dy = -radiusBlocks; dy <= radiusBlocks; ++dy)
		{
			for (int dz = -radiusBlocks; dz <= radiusBlocks; ++dz)
			{
				// Calculate global block coordinates
				IntVec3 blockCoords = IntVec3(
					static_cast<int>(m_position.x) + dx,
					static_cast<int>(m_position.y) + dy,
					static_cast<int>(m_position.z) + dz
				);

				// Calculate block center position
				Vec3 blockCenter = Vec3(
					static_cast<float>(blockCoords.x) + 0.5f,
					static_cast<float>(blockCoords.y) + 0.5f,
					static_cast<float>(blockCoords.z) + 0.5f
				);

				// Check distance (skip blocks outside radius)
				float distance = GetDistance3D(m_position, blockCenter);
				if (distance > radius)
				{
					continue;
				}

				// Get block type at coordinates
				uint8_t blockType = world->GetBlockTypeAtGlobalCoords(blockCoords);

				// Skip air blocks (blockType == 0)
				if (blockType == 0)
				{
					continue;
				}

				// Get block name from BlockDefinition (XML-based legacy system)
				// TODO: Migrate to BlockRegistry JSON system when A7 registry is complete
				sBlockDefinition* blockDef = sBlockDefinition::GetDefinitionByIndex(blockType);
				std::string blockName = blockDef ? std::string(blockDef->GetName()) : ("Unknown_" + std::to_string(blockType));

				// Add to result vector
				BlockInfo info;
				info.blockCoords = blockCoords;
				info.blockID = blockType;
				info.blockName = blockName;
				result.push_back(info);
			}
		}
	}

	return result;
}

//----------------------------------------------------------------------------------------------------
// GetNearbyEntities - Query all entities within radius (excluding self)
// Assignment 7-AI: Vision system for entity detection (Task eb0e3f92)
//----------------------------------------------------------------------------------------------------
std::vector<Entity*> Agent::GetNearbyEntities(float radius) const
{
	std::vector<Entity*> result;

	// Get World reference (need to query entities)
	if (m_game == nullptr)
	{
		ERROR_RECOVERABLE("Agent::GetNearbyEntities: m_game is nullptr");
		return result;
	}

	World* world = m_game->GetWorld();
	if (world == nullptr)
	{
		ERROR_RECOVERABLE("Agent::GetNearbyEntities: World is nullptr");
		return result;
	}

	// Get Player entity (always exists in game mode)
	Player* player = m_game->GetPlayer();
	if (player != nullptr)
	{
		// Player is Entity, so uses public m_position member
		float distance = GetDistance3D(m_position, player->m_position);
		if (distance <= radius)
		{
			result.push_back(player);
		}
	}

	// Get all ItemEntity objects from World
	std::vector<ItemEntity*> itemEntities = world->GetNearbyItemEntities(m_position, radius);
	for (ItemEntity* itemEntity : itemEntities)
	{
		result.push_back(itemEntity);
	}

	// NOTE: For future expansion, when World has GetAllAgents() or similar,
	// we would loop through all agents here and filter by distance
	// For now, we only detect Player and ItemEntity objects

	return result;
}
