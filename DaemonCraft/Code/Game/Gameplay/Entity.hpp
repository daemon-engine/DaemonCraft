//----------------------------------------------------------------------------------------------------
// Entity.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <cstdint>

#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec3.hpp"

//----------------------------------------------------------------------------------------------------
class Game;

//----------------------------------------------------------------------------------------------------
// Physics mode determines entity movement and collision behavior
enum class ePhysicsMode : uint8_t
{
    WALKING,  // Full physics with gravity, can jump when grounded
    FLYING,   // No gravity, can fly up/down, collides with solid blocks
    NOCLIP    // No gravity, no collision, passes through solid blocks
};

//----------------------------------------------------------------------------------------------------
// Entity type identification (Assignment 7-AI)
enum class EntityType : uint8_t
{
    PLAYER,  // Player-controlled entity
    AGENT,   // AI-controlled entity
    PROP,    // Static/dynamic world object
    ITEM     // Dropped item entity
};

//----------------------------------------------------------------------------------------------------
class Entity
{
public:
    explicit Entity(Game* owner);
    virtual  ~Entity();

    virtual void  Update(float deltaSeconds);  // Assignment 6: Core physics integration loop (can be overridden)
    virtual void  Render() const = 0;
    virtual Mat44 GetModelToWorldTransform() const;
    virtual EntityType GetEntityType() const = 0;  // Assignment 7-AI: Entity type identification

    // Assignment 6: Physics simulation
    void   UpdatePhysics(float deltaSeconds);  // Newtonian integration: gravity, friction, velocity
    void   UpdateIsGrounded();                 // Update m_isOnGround state using World raycast
    void   ResolveCollisionWithWorld(Vec3& deltaPosition);  // 12-corner raycast collision detection
    AABB3  GetWorldAABB() const;               // Get physics AABB transformed to world space (m_physicsAABB + m_position)

    Game*       m_game            = nullptr;
    Vec3        m_position        = Vec3::ZERO;
    Vec3        m_velocity        = Vec3::ZERO;
    EulerAngles m_orientation     = EulerAngles::ZERO;
    EulerAngles m_angularVelocity = EulerAngles::ZERO;
    Rgba8       m_color           = Rgba8::WHITE;

    // Assignment 6: Physics system fields
    AABB3        m_physicsAABB        = AABB3::ZERO_TO_ONE;   // Collision bounds in local space (centered on entity position)
    Vec3         m_acceleration       = Vec3::ZERO;           // Accumulated forces per frame (m/s²)
    bool         m_isOnGround         = false;                // True if solid blocks detected below entity
    ePhysicsMode m_physicsMode        = ePhysicsMode::WALKING; // Current physics behavior mode
    bool         m_physicsEnabled     = true;                 // Enable/disable physics for static objects
    float        m_gravityCoefficient = 1.0f;                 // Gravity multiplier (1.0 = full gravity)
    float        m_frictionCoefficient = 1.0f;                // Friction multiplier (1.0 = full friction)
};
