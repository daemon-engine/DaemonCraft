//----------------------------------------------------------------------------------------------------
// Agent.hpp
// Assignment 7-AI: AI-controlled entity with inventory and command queue
//----------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/Entity.hpp"
#include "Game/Gameplay/Inventory.hpp"
//----------------------------------------------------------------------------------------------------
#include "Engine/Math/IntVec3.hpp"  // For BlockInfo struct
//----------------------------------------------------------------------------------------------------
#include <string>
#include <queue>
#include <cstdint>
#include <vector>
//----------------------------------------------------------------------------------------------------

// Forward declarations (AgentCommand will be implemented in future tasks)
class AgentCommand;

//----------------------------------------------------------------------------------------------------
// Agent - AI-controlled entity with inventory and command queue
//
// Extends Entity to provide:
// - 36-slot inventory (same as Player)
// - Command queue for autonomous actions (MOVE, MINE, PLACE, CRAFT, WAIT)
// - Command execution state machine
//
// Design Notes:
// - Shares Entity physics (collision, gravity) like Player
// - Command queue processed one command per Update() call
// - Commands are heap-allocated (new/delete), CompleteCurrentCommand() deletes finished commands
// - Rendered as green wireframe cube (2×2×2 blocks tall) during development
//----------------------------------------------------------------------------------------------------
class Agent : public Entity
{
public:
	// Constructor
	explicit Agent(Game* owner, std::string const& name, uint64_t agentID, Vec3 const& position);
	~Agent() override;

	// Entity Interface
	void Update(float deltaSeconds) override;
	void Render() const override;
	EntityType GetEntityType() const override { return EntityType::AGENT; }

	// Agent Identity
	std::string const& GetName() const { return m_agentName; }
	uint64_t GetAgentID() const { return m_agentID; }

	// Command Queue
	void QueueCommand(AgentCommand* command);
	bool HasPendingCommands() const { return !m_commandQueue.empty(); }
	int GetCommandQueueSize() const { return static_cast<int>(m_commandQueue.size()); }
	void ClearCommandQueue(); // For emergency stops

	// Inventory Access
	Inventory&       GetInventory() { return m_inventory; }
	Inventory const& GetInventory() const { return m_inventory; }

	// Command Execution State
	bool IsExecutingCommand() const { return m_currentCommand != nullptr; }
	std::string GetCurrentCommandType() const;  // Returns "NONE" if no command executing

	// Vision System (Assignment 7-AI: Environment perception for TypeScript agents)
	struct BlockInfo
	{
		IntVec3     blockCoords;  // Global block coordinates
		uint16_t    blockID;      // Block type ID (for registry lookup)
		std::string blockName;    // Human-readable name (e.g., "Diamond_Ore")
	};

	std::vector<BlockInfo> GetNearbyBlocks(float radius) const;
	std::vector<Entity*>   GetNearbyEntities(float radius) const;

private:
	// Agent Identity
	std::string m_agentName;          // Unique name (e.g., "MinerBot")
	uint64_t m_agentID;               // Unique ID from KADI

	// Inventory
	Inventory m_inventory;            // 36 slots (matches Player)

	// Command System
	std::queue<AgentCommand*> m_commandQueue;
	AgentCommand* m_currentCommand = nullptr;

	// Command Execution
	void ProcessCommandQueue(float deltaSeconds);
	void ExecuteCurrentCommand(float deltaSeconds);
	void CompleteCurrentCommand();
};
