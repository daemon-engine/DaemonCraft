//----------------------------------------------------------------------------------------------------
// ItemEntity.cpp
//----------------------------------------------------------------------------------------------------

#include "Game/Gameplay/ItemEntity.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Game/Gameplay/World.hpp"
#include "Game/Gameplay/Player.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexUtils.hpp"
#include "Engine/Audio/AudioSystem.hpp"  // Assignment 7: For item pickup sound effect

//----------------------------------------------------------------------------------------------------
// Constructor: Initialize item entity with position and item data
// Physics: Small AABB (0.25 x 0.25 x 0.25), WALKING mode (gravity + collision)
//----------------------------------------------------------------------------------------------------
ItemEntity::ItemEntity(Game* owner, Vec3 const& position, ItemStack const& item)
    : Entity(owner)
    , m_item(item)
{
    m_position = position;
    m_physicsAABB = AABB3(Vec3(-0.125f, -0.125f, -0.125f), Vec3(0.125f, 0.125f, 0.125f)); // 0.25 x 0.25 x 0.25 block
    m_physicsMode = ePhysicsMode::WALKING; // Falls with gravity, collides with blocks
    m_physicsEnabled = true;
    m_gravityCoefficient = 1.0f; // Full gravity
    m_frictionCoefficient = 0.8f; // Slightly reduced friction for sliding

    // Start with slight random velocity for natural drop effect
    m_velocity = Vec3(0.0f, 0.0f, 1.0f); // Small upward velocity
}

//----------------------------------------------------------------------------------------------------
// Update: Physics, magnetic pickup, item stacking, despawn timer
//----------------------------------------------------------------------------------------------------
void ItemEntity::Update(float deltaSeconds)
{
    // Step 1: Decrement pickup cooldown (prevents immediate re-pickup after drop)
    if (m_pickupCooldown > 0.0f)
    {
        m_pickupCooldown -= deltaSeconds;
    }

    // Step 2: Decrement despawn timer (5-minute lifetime)
    m_despawnTimer -= deltaSeconds;
    if (m_despawnTimer <= 0.0f)
    {
        return; // Entity will be marked for deletion by Game
    }

    // Step 3: Apply base entity physics (gravity, collision, movement)
    Entity::Update(deltaSeconds);

    // Step 4: Magnetic pickup - pull toward nearest player
    World* world = m_game->GetWorld();
    if (world != nullptr && CanBePickedUp())
    {
        Player* player = m_game->GetPlayer();
        if (player != nullptr)
        {
            ApplyMagneticPull(player->m_position, deltaSeconds);
        }
    }

    // Step 5: Item stacking - merge with nearby identical items
    TryMergeWithNearbyItems();
}

//----------------------------------------------------------------------------------------------------
// ApplyMagneticPull: Pull item toward player within magnetic radius
// Minecraft mechanic: Items move toward player when close enough
//----------------------------------------------------------------------------------------------------
void ItemEntity::ApplyMagneticPull(Vec3 const& playerPosition, float deltaSeconds)
{
    Vec3 toPlayer = playerPosition - m_position;
    float distance = toPlayer.GetLength();

    // Only apply magnetic pull within radius
    if (distance < m_magnetRadius && distance > 0.001f)
    {
        // Calculate pull velocity (5.0 blocks/sec toward player)
        Vec3 pullDirection = toPlayer / distance; // Normalize
        float pullSpeed = 5.0f;

        // Apply pull force as velocity adjustment (not acceleration)
        m_velocity += pullDirection * pullSpeed * deltaSeconds;
    }
}

//----------------------------------------------------------------------------------------------------
// TryMergeWithNearbyItems: Merge with nearby ItemEntity of same type
// Prevents world clutter from mass block breaking
//----------------------------------------------------------------------------------------------------
void ItemEntity::TryMergeWithNearbyItems()
{
    // TODO: Implement after World::GetNearbyEntities() is available
    // Pseudocode:
    // 1. Query World for nearby ItemEntity within 1 block radius
    // 2. For each nearby ItemEntity:
    //    - If same itemID && not full && not self:
    //      - Transfer items using ItemStack::Add()
    //      - Mark other entity for deletion if empty
    // 3. Mark self for deletion if empty
}

//----------------------------------------------------------------------------------------------------
// TryPickup: Attempt to add item to player inventory
// Returns true if successfully picked up (inventory had space)
//----------------------------------------------------------------------------------------------------
bool ItemEntity::TryPickup(Player* player)
{
    if (!CanBePickedUp())
    {
        return false; // Still in pickup cooldown
    }

    // Try to add item to player inventory
    Inventory& inventory = player->GetInventory();
    DebuggerPrintf("[ITEMENTITY] TryPickup - Attempting to add itemID=%u, quantity=%u to inventory\n",
        m_item.itemID, m_item.quantity);

    bool success = inventory.AddItem(m_item.itemID, m_item.quantity);

    if (success)
    {
        // Play item pickup sound effect (Assignment 7: Sound effects)
        if (g_audio != nullptr)
        {
            // Authentic Minecraft pickup "pop" sound (orb.mp3 from SoundDino)
            SoundID pickupSound = g_audio->CreateOrGetSound("Data/Audio/item_pickup.mp3", eAudioSystemSoundDimension::Sound2D);
            if (pickupSound != MISSING_SOUND_ID)
            {
                g_audio->StartSound(pickupSound, false, 0.5f);  // 50% volume for satisfying pickup feedback
            }
        }

        // Mark for deletion by clearing item
        m_item.Clear();
        m_despawnTimer = 0.0f; // Trigger immediate despawn
        DebuggerPrintf("[ITEMENTITY] TryPickup SUCCESS - Item added to inventory, marked for despawn (timer=%.2f)\n", m_despawnTimer);
        return true;
    }

    DebuggerPrintf("[ITEMENTITY] TryPickup FAILED - Inventory full or AddItem returned false\n");
    return false; // Inventory full
}

//----------------------------------------------------------------------------------------------------
// Render: Draw item as colored cube for debugging
// Future: Render item icon as billboard or 3D model
//----------------------------------------------------------------------------------------------------
void ItemEntity::Render() const
{
    if (g_renderer == nullptr)
    {
        return;
    }

    // Get world AABB for rendering (entity position + physics AABB)
    AABB3 worldAABB = GetWorldAABB();

    // Render as solid colored cube (debug visualization)
    std::vector<Vertex_PCU> verts;
    AddVertsForAABB3D(verts, worldAABB, Rgba8::YELLOW);

    // Set up rendering state (use default shader for colored geometry)
    g_renderer->SetBlendMode(eBlendMode::OPAQUE);
    g_renderer->SetRasterizerMode(eRasterizerMode::SOLID_CULL_BACK);
    g_renderer->SetSamplerMode(eSamplerMode::POINT_CLAMP);
    g_renderer->SetDepthMode(eDepthMode::READ_WRITE_LESS_EQUAL);
    g_renderer->BindTexture(nullptr); // No texture, use vertex colors
    g_renderer->BindShader(nullptr);  // Use default shader for untextured geometry

    g_renderer->DrawVertexArray(static_cast<int>(verts.size()), verts.data());

    // Future: Render item icon as billboard facing camera
    // Future: Render 3D item model with rotation animation
}
