//----------------------------------------------------------------------------------------------------
// AgentCommand.hpp
// Assignment 7-AI: Command base class and 5 concrete command implementations
//----------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/IntVec3.hpp"
//----------------------------------------------------------------------------------------------------
#include <string>
#include <cstdint>
//----------------------------------------------------------------------------------------------------

// Forward declarations
class Agent;

//----------------------------------------------------------------------------------------------------
// Command Status
enum class eCommandStatus : uint8_t
{
	NOT_STARTED,  // Command not started yet
	IN_PROGRESS,  // Command actively executing
	COMPLETED,    // Command successfully finished
	FAILED        // Command failed (invalid target, unreachable, etc.)
};

//----------------------------------------------------------------------------------------------------
// AgentCommand - Abstract base class for agent actions
//
// All commands implement:
// - Start() - Initialize command state
// - Execute() - Perform one frame of work, return status
// - GetType() - Return command type as string
// - GetFailureReason() - Return reason if status == FAILED
//
// Lifecycle:
// 1. Agent::QueueCommand() adds command to queue
// 2. ProcessCommandQueue() dequeues and calls Start()
// 3. ExecuteCurrentCommand() calls Execute() every frame
// 4. CompleteCurrentCommand() deletes finished command
//----------------------------------------------------------------------------------------------------
class AgentCommand
{
public:
	virtual ~AgentCommand() = default;

	// Command lifecycle
	virtual void Start() = 0;
	virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) = 0;

	// Command metadata
	virtual std::string GetType() const = 0;
	std::string GetFailureReason() const { return m_failureReason; }

protected:
	eCommandStatus m_status = eCommandStatus::NOT_STARTED;
	std::string m_failureReason;
};

//----------------------------------------------------------------------------------------------------
// MoveCommand - Navigate agent to target position
//----------------------------------------------------------------------------------------------------
class MoveCommand : public AgentCommand
{
public:
	explicit MoveCommand(Vec3 const& targetPosition, float moveSpeed = 4.0f);

	virtual void Start() override;
	virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) override;
	virtual std::string GetType() const override { return "MOVE"; }

private:
	Vec3 m_targetPosition;
	float m_moveSpeed; // Blocks per second (Player uses 4.0f)

	static constexpr float ARRIVAL_THRESHOLD = 0.5f; // Within 0.5 blocks
};

//----------------------------------------------------------------------------------------------------
// MineCommand - Progressive block breaking (reuses World::BreakBlock logic)
//----------------------------------------------------------------------------------------------------
class MineCommand : public AgentCommand
{
public:
	explicit MineCommand(IntVec3 const& blockCoords);

	virtual void Start() override;
	virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) override;
	virtual std::string GetType() const override { return "MINE"; }

private:
	IntVec3 m_blockCoords;
	float m_miningProgress = 0.0f;  // 0.0 to 1.0
	float m_miningDuration = 0.0f;  // Seconds (from block definition)

	static constexpr float MAX_MINING_DISTANCE = 5.0f; // Same as Player
};

//----------------------------------------------------------------------------------------------------
// PlaceCommand - Block placement from inventory (reuses World::PlaceBlock logic)
//----------------------------------------------------------------------------------------------------
class PlaceCommand : public AgentCommand
{
public:
	explicit PlaceCommand(IntVec3 const& blockCoords, uint16_t itemID);

	virtual void Start() override;
	virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) override;
	virtual std::string GetType() const override { return "PLACE"; }

private:
	IntVec3 m_blockCoords;
	uint16_t m_itemID; // ItemRegistry ID (must be a block item)

	static constexpr float MAX_PLACEMENT_DISTANCE = 5.0f;
};

//----------------------------------------------------------------------------------------------------
// CraftCommand - Recipe execution (instant crafting)
//----------------------------------------------------------------------------------------------------
class CraftCommand : public AgentCommand
{
public:
	explicit CraftCommand(uint16_t recipeID);

	virtual void Start() override;
	virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) override;
	virtual std::string GetType() const override { return "CRAFT"; }

private:
	uint16_t m_recipeID; // RecipeRegistry ID
};

//----------------------------------------------------------------------------------------------------
// WaitCommand - Timer delay (pauses agent for specified duration)
//----------------------------------------------------------------------------------------------------
class WaitCommand : public AgentCommand
{
public:
	explicit WaitCommand(float duration); // Duration in seconds

	virtual void Start() override;
	virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) override;
	virtual std::string GetType() const override { return "WAIT"; }

private:
	float m_duration;
	float m_elapsedTime = 0.0f;
};
