//----------------------------------------------------------------------------------------------------
// Entity.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/Entity.hpp"
#include "Game/Framework/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"   // Assignment 6: For GetWorld() in UpdateIsGrounded()
#include "Game/Gameplay/World.hpp"  // Assignment 6: For IsEntityOnGround() in UpdateIsGrounded()
#include "Engine/Math/Vec2.hpp"

//----------------------------------------------------------------------------------------------------
Entity::Entity(Game* owner)
    : m_game(owner)
{
}

//----------------------------------------------------------------------------------------------------
Entity::~Entity()
{
}

//----------------------------------------------------------------------------------------------------
Mat44 Entity::GetModelToWorldTransform() const
{
    Mat44 m2w;

    m2w.SetTranslation3D(m_position);

    m2w.AppendZRotation(m_orientation.m_yawDegrees);
    m2w.AppendYRotation(m_orientation.m_pitchDegrees);
    m2w.AppendXRotation(m_orientation.m_rollDegrees);

    // m2w.Append(m_orientation.GetAsMatrix_IFwd_JLeft_KUp());

    return m2w;
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Newtonian Physics Integration
// Updates velocity based on gravity, friction, and accumulated forces
// Does NOT update position or handle collision - those happen separately
//----------------------------------------------------------------------------------------------------
void Entity::UpdatePhysics(float deltaSeconds)
{
    // Skip physics if disabled (for static props)
    if (!m_physicsEnabled)
    {
        m_acceleration = Vec3::ZERO;
        return;
    }

    // Step 1: Apply gravity (only in WALKING mode when not on ground)
    if (m_physicsMode == ePhysicsMode::WALKING && !m_isOnGround)
    {
        m_acceleration.z += GRAVITY_ACCELERATION * m_gravityCoefficient;
    }

    // Step 2: Apply horizontal friction (opposes horizontal velocity)
    Vec2 horizontalVelocity(m_velocity.x, m_velocity.y);
    float friction = m_isOnGround ? FRICTION_GROUND : FRICTION_AIR;
    friction *= m_frictionCoefficient;  // Apply entity-specific friction multiplier

    // Friction force opposes velocity: F = -friction * velocity
    m_acceleration.x += -horizontalVelocity.x * friction;
    m_acceleration.y += -horizontalVelocity.y * friction;

    // Step 3: Integrate velocity using Euler method: v = v + a * dt
    m_velocity += m_acceleration * deltaSeconds;

    // Step 4: Clamp horizontal speed to maximum (prevents voluntary over-speed)
    Vec2 newHorizontalVelocity(m_velocity.x, m_velocity.y);
    float horizontalSpeed = newHorizontalVelocity.GetLength();

    if (horizontalSpeed > PLAYER_MAX_HORIZONTAL_SPEED)
    {
        // Normalize and scale to max speed
        newHorizontalVelocity.Normalize();
        newHorizontalVelocity *= PLAYER_MAX_HORIZONTAL_SPEED;
        m_velocity.x = newHorizontalVelocity.x;
        m_velocity.y = newHorizontalVelocity.y;
    }

    // Step 5: Reset acceleration for next frame (forces are instantaneous)
    m_acceleration = Vec3::ZERO;
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Get physics AABB transformed to world space
// Returns local-space m_physicsAABB translated by entity position
//----------------------------------------------------------------------------------------------------
AABB3 Entity::GetWorldAABB() const
{
    // Manually translate AABB by adding entity position to mins and maxs
    AABB3 worldAABB;
    worldAABB.m_mins = m_physicsAABB.m_mins + m_position;
    worldAABB.m_maxs = m_physicsAABB.m_maxs + m_position;
    return worldAABB;
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Update grounded state by querying World for ground contact
// Always false in FLYING/NOCLIP modes (no ground interaction)
// Must be called every frame to keep m_isOnGround synchronized with physics state
//----------------------------------------------------------------------------------------------------
void Entity::UpdateIsGrounded()
{
    // FLYING and NOCLIP modes ignore ground entirely
    if (m_physicsMode == ePhysicsMode::FLYING || m_physicsMode == ePhysicsMode::NOCLIP)
    {
        m_isOnGround = false;
        return;
    }

    // WALKING mode: Use World's 4-corner raycast to detect ground
    World* world = m_game->GetWorld();
    if (world == nullptr)
    {
        m_isOnGround = false; // No world means no ground
        return;
    }

    m_isOnGround = world->IsEntityOnGround(this);
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Resolve collisions using 12-corner raycasts in movement direction
// Prevents tunneling through blocks at high speeds by checking multiple corner points
// Zeros velocity components on blocked axes to prevent continued movement into blocks
//----------------------------------------------------------------------------------------------------
// Assignment 6: Per-axis collision detection to prevent corner-sticking bug
// FIX: Previous diagonal raycast approach caused horizontal collisions to block vertical movement
// NEW: Independent axis processing allows jumping near walls without sticking
void Entity::ResolveCollisionWithWorld(Vec3& deltaPosition)
{
    // Skip collision if NOCLIP mode or no World available
    if (m_physicsMode == ePhysicsMode::NOCLIP)
    {
        return; // NOCLIP passes through all blocks
    }

    World* world = m_game->GetWorld();
    if (world == nullptr)
    {
        return; // No world means no collision
    }

    // Early exit if no significant movement on any axis
    if (fabsf(deltaPosition.x) < 0.0001f &&
        fabsf(deltaPosition.y) < 0.0001f &&
        fabsf(deltaPosition.z) < 0.0001f)
    {
        return; // No significant movement, skip raycasts
    }

    // Calculate 8 AABB corners in world space (bottom 4 + top 4)
    Vec3 corners[8];
    corners[0] = m_position + m_physicsAABB.m_mins; // Bottom-Left-Front
    corners[1] = m_position + Vec3(m_physicsAABB.m_maxs.x, m_physicsAABB.m_mins.y, m_physicsAABB.m_mins.z); // Bottom-Right-Front
    corners[2] = m_position + Vec3(m_physicsAABB.m_mins.x, m_physicsAABB.m_maxs.y, m_physicsAABB.m_mins.z); // Bottom-Left-Back
    corners[3] = m_position + Vec3(m_physicsAABB.m_maxs.x, m_physicsAABB.m_maxs.y, m_physicsAABB.m_mins.z); // Bottom-Right-Back
    corners[4] = m_position + Vec3(m_physicsAABB.m_mins.x, m_physicsAABB.m_mins.y, m_physicsAABB.m_maxs.z); // Top-Left-Front
    corners[5] = m_position + Vec3(m_physicsAABB.m_maxs.x, m_physicsAABB.m_mins.y, m_physicsAABB.m_maxs.z); // Top-Right-Front
    corners[6] = m_position + Vec3(m_physicsAABB.m_mins.x, m_physicsAABB.m_maxs.y, m_physicsAABB.m_maxs.z); // Top-Left-Back
    corners[7] = m_position + m_physicsAABB.m_maxs; // Top-Right-Back

    // === X-AXIS COLLISION (Left/Right walls) ===
    if (fabsf(deltaPosition.x) > 0.0001f)
    {
        Vec3 xDir = (deltaPosition.x > 0.0f) ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(-1.0f, 0.0f, 0.0f);
        float xDist = fabsf(deltaPosition.x) + RAYCAST_OFFSET;

        // Select 4 corners on the moving face (right face if moving +X, left face if moving -X)
        int xCornerIndices[4];
        if (deltaPosition.x > 0.0f)
        {
            // Moving right: check right face corners (maxs.x)
            xCornerIndices[0] = 1; xCornerIndices[1] = 3; xCornerIndices[2] = 5; xCornerIndices[3] = 7;
        }
        else
        {
            // Moving left: check left face corners (mins.x)
            xCornerIndices[0] = 0; xCornerIndices[1] = 2; xCornerIndices[2] = 4; xCornerIndices[3] = 6;
        }

        float closestImpact = FLT_MAX;
        for (int i = 0; i < 4; ++i)
        {
            RaycastResult result = world->RaycastVoxel(corners[xCornerIndices[i]], xDir, xDist);
            if (result.m_didImpact && world->IsBlockSolid(result.m_impactBlockCoords))
            {
                // Verify this is an X-axis wall (normal points mostly in X direction)
                if (fabsf(result.m_impactNormal.x) > 0.5f)
                {
                    closestImpact = (closestImpact < result.m_impactDistance) ? closestImpact : result.m_impactDistance;
                }
            }
        }

        // Constrain X movement if collision detected
        if (closestImpact < fabsf(deltaPosition.x))
        {
            deltaPosition.x = xDir.x * closestImpact;
            m_velocity.x = 0.0f; // Zero X velocity to prevent continued pushing into wall
        }
    }

    // === Y-AXIS COLLISION (Front/Back walls) ===
    if (fabsf(deltaPosition.y) > 0.0001f)
    {
        Vec3 yDir = (deltaPosition.y > 0.0f) ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(0.0f, -1.0f, 0.0f);
        float yDist = fabsf(deltaPosition.y) + RAYCAST_OFFSET;

        // Select 4 corners on the moving face (back face if moving +Y, front face if moving -Y)
        int yCornerIndices[4];
        if (deltaPosition.y > 0.0f)
        {
            // Moving back: check back face corners (maxs.y)
            yCornerIndices[0] = 2; yCornerIndices[1] = 3; yCornerIndices[2] = 6; yCornerIndices[3] = 7;
        }
        else
        {
            // Moving forward: check front face corners (mins.y)
            yCornerIndices[0] = 0; yCornerIndices[1] = 1; yCornerIndices[2] = 4; yCornerIndices[3] = 5;
        }

        float closestImpact = FLT_MAX;
        for (int i = 0; i < 4; ++i)
        {
            RaycastResult result = world->RaycastVoxel(corners[yCornerIndices[i]], yDir, yDist);
            if (result.m_didImpact && world->IsBlockSolid(result.m_impactBlockCoords))
            {
                // Verify this is a Y-axis wall (normal points mostly in Y direction)
                if (fabsf(result.m_impactNormal.y) > 0.5f)
                {
                    closestImpact = (closestImpact < result.m_impactDistance) ? closestImpact : result.m_impactDistance;
                }
            }
        }

        // Constrain Y movement if collision detected
        if (closestImpact < fabsf(deltaPosition.y))
        {
            deltaPosition.y = yDir.y * closestImpact;
            m_velocity.y = 0.0f; // Zero Y velocity to prevent continued pushing into wall
        }
    }

    // === Z-AXIS COLLISION (Floor/Ceiling) ===
    if (fabsf(deltaPosition.z) > 0.0001f)
    {
        Vec3 zDir = (deltaPosition.z > 0.0f) ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 0.0f, -1.0f);
        float zDist = fabsf(deltaPosition.z) + RAYCAST_OFFSET;

        // Select 4 corners on the moving face (top face if moving +Z, bottom face if moving -Z)
        int zCornerIndices[4];
        if (deltaPosition.z > 0.0f)
        {
            // Moving up: check top face corners (maxs.z)
            zCornerIndices[0] = 4; zCornerIndices[1] = 5; zCornerIndices[2] = 6; zCornerIndices[3] = 7;
        }
        else
        {
            // Moving down: check bottom face corners (mins.z)
            zCornerIndices[0] = 0; zCornerIndices[1] = 1; zCornerIndices[2] = 2; zCornerIndices[3] = 3;
        }

        float closestImpact = FLT_MAX;
        for (int i = 0; i < 4; ++i)
        {
            RaycastResult result = world->RaycastVoxel(corners[zCornerIndices[i]], zDir, zDist);
            if (result.m_didImpact && world->IsBlockSolid(result.m_impactBlockCoords))
            {
                // Verify this is a Z-axis surface (normal points mostly in Z direction)
                if (fabsf(result.m_impactNormal.z) > 0.5f)
                {
                    closestImpact = (closestImpact < result.m_impactDistance) ? closestImpact : result.m_impactDistance;
                }
            }
        }

        // Constrain Z movement if collision detected
        if (closestImpact < fabsf(deltaPosition.z))
        {
            deltaPosition.z = zDir.z * closestImpact;
            m_velocity.z = 0.0f; // Zero Z velocity to prevent continued pushing into surface
        }
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Core physics integration loop
// Coordinates all physics methods in correct order every frame
// Subclasses (Player) should call this base implementation via Entity::Update(deltaSeconds)
//----------------------------------------------------------------------------------------------------
void Entity::Update(float deltaSeconds)
{
    // Step 1: Update velocity based on forces (gravity, friction)
    UpdatePhysics(deltaSeconds);

    // Step 2: Calculate desired movement for this frame
    Vec3 deltaPosition = m_velocity * deltaSeconds;

    // Step 3: Resolve collision in movement direction (NOCLIP skips collision entirely)
    if (m_physicsMode != ePhysicsMode::NOCLIP)
    {
        ResolveCollisionWithWorld(deltaPosition);  // Modifies deltaPosition and m_velocity
    }

    // Step 4: Apply final position change
    m_position += deltaPosition;

    // Step 5: Safety check - push entity out if stuck in blocks (NOCLIP skips)
    if (m_physicsMode != ePhysicsMode::NOCLIP)
    {
        World* world = m_game->GetWorld();
        if (world != nullptr)
        {
            world->PushEntityOutOfBlocks(this);
        }
    }

    // Step 6: Update grounded state for next frame
    UpdateIsGrounded();
}

