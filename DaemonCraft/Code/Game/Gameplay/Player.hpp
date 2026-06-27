//----------------------------------------------------------------------------------------------------
// Player.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/Entity.hpp"
#include "Game/Gameplay/Inventory.hpp"
//----------------------------------------------------------------------------------------------------
#include "Engine/Math/IntVec3.hpp"  // Assignment 7: Required for m_targetBlockCoords member
//----------------------------------------------------------------------------------------------------
#include <cstdint>

//-Forward-Declaration--------------------------------------------------------------------------------
class Camera;

//----------------------------------------------------------------------------------------------------
// Assignment 6: Camera mode determines view perspective and camera behavior
enum class eCameraMode : uint8_t
{
    FIRST_PERSON,   // Camera at player eye height, player not rendered
    OVER_SHOULDER,  // Camera 4m behind player, player rendered
    SPECTATOR,      // Free-flying camera detached from player, full 3D movement
    SPECTATOR_XY,   // Free-flying camera detached from player, XY-plane only (no Z movement)
    INDEPENDENT     // Camera stays in place while player moves (developer debug mode)
};

//----------------------------------------------------------------------------------------------------
// Assignment 7: Mining state machine for progressive block breaking
enum class eMiningState : uint8_t
{
    IDLE,    // Not mining anything
    MINING,  // Actively mining a block (left-click held)
    BROKEN   // Block just broken (transition state, returns to IDLE next frame)
};

//----------------------------------------------------------------------------------------------------
class Player : public Entity
{
public:
    explicit Player(Game* owner);
    ~Player() override;

    void Update(float deltaSeconds) override;
    void Render() const override;
    EntityType GetEntityType() const override { return EntityType::PLAYER; }
    void UpdateFromInput(float deltaSeconds);
    void UpdateFromKeyBoard(float deltaSeconds);
    void UpdateFromController(float deltaSeconds);
    void UpdateCamera();  // Assignment 6: Position camera based on camera mode
    void UpdateMining(float deltaSeconds);  // Assignment 7: Mining state machine
    void UpdatePlacement();  // Assignment 7: Block placement with right-click
    void PickupNearbyItems();  // Assignment 7: ItemEntity collision detection and pickup

    Camera* GetCamera() const;
    Vec3 const& GetVelocity();
    Vec3 GetEyePosition() const;  // Assignment 6: Helper to get player eye position
    eCameraMode GetCameraMode() const;  // Get current camera mode

    // Assignment 7: Inventory access
    Inventory&       GetInventory() { return m_inventory; }
    Inventory const& GetInventory() const { return m_inventory; }
    ItemStack&       GetSelectedItemStack() { return m_inventory.GetSelectedHotbarItemStack(); }
    ItemStack const& GetSelectedItemStack() const { return m_inventory.GetSelectedHotbarItemStack(); }

    // Assignment 7: Block interaction raycasting
    struct RaycastResult RaycastForPlacement(float maxDistance = 6.0f) const;
    bool CanPlaceBlock(IntVec3 const& blockCoords) const;  // Validate block placement position

private:
    // Assignment 7: Mining helper methods
    float CalculateBreakTime(IntVec3 const& blockCoords) const;
    void  BreakBlock(IntVec3 const& blockCoords);
    void  RenderMiningProgress() const;  // Render crack overlay on target block

    Camera*      m_worldCamera   = nullptr;
    class Texture* m_crackTexture = nullptr;  // Assignment 7: 10-stage crack overlay texture
    float        m_moveSpeed     = 4.f;

    // Assignment 6: Camera system
    eCameraMode     m_cameraMode               = eCameraMode::FIRST_PERSON;  // Current camera view mode
    Vec3            m_spectatorPosition        = Vec3(-50.f, -50.f, 150.f);  // Spectator camera position (when detached)
    EulerAngles     m_spectatorOrientation     = EulerAngles(45.f, 45.f, 0.f); // Spectator camera orientation (when detached)
    bool            m_wasCKeyPressed           = false;                      // C key debounce for camera mode cycling

    // Assignment 7: Inventory system
    Inventory       m_inventory;  // 36-slot inventory (27 main + 9 hotbar)

    // Assignment 7: Mining system
    eMiningState    m_miningState        = eMiningState::IDLE;  // Current mining state
    IntVec3         m_targetBlockCoords;                        // Block being mined (initialized in constructor)
    float           m_miningProgress     = 0.0f;                // Mining progress (0.0 to 1.0)
    float           m_breakTime          = 1.0f;                // Time required to break current block
};
