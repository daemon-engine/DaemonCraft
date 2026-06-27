//----------------------------------------------------------------------------------------------------
// Player.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/Player.hpp"
//----------------------------------------------------------------------------------------------------
#include "Game/Framework/GameCommon.hpp"
#include "Game/Framework/Chunk.hpp"      // Assignment 7: For MarkChunkForMeshRebuild()
#include "Game/Gameplay/Game.hpp"
#include "Game/Gameplay/ItemStack.hpp"   // Assignment 7: For BreakBlock item drops
#include "Game/Gameplay/ItemEntity.hpp"  // Assignment 7: For PickupNearbyItems collision detection
#include "Game/Gameplay/World.hpp"  // Assignment 6: For PushEntityOutOfBlocks() in physics mode cycling
#include "Game/Definition/BlockDefinition.hpp"  // Assignment 7: For mining break time calculation
#include "Game/Definition/ItemDefinition.hpp"   // Assignment 7: For tool durability (IsTool check)
#include "Game/Definition/ItemRegistry.hpp"     // Assignment 7: For tool durability (registry lookup)
//----------------------------------------------------------------------------------------------------
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"  // Assignment 6: For AABB debug wireframe rendering
#include "Engine/Renderer/Renderer.hpp"  // Assignment 7: For rendering crack overlay
#include "Engine/Resource/ResourceSubsystem.hpp"  // Assignment 7: For loading crack texture
#include "Engine/Audio/AudioSystem.hpp"  // Assignment 7: For block break sound effects
#include "Engine/Widget/WidgetSubsystem.hpp"

//----------------------------------------------------------------------------------------------------
Player::Player(Game* owner)
    : Entity(owner)
{
    m_worldCamera = new Camera();
    m_worldCamera->SetPerspectiveGraphicView(2.f, 60.f, 0.1f, 10000.f);
    m_worldCamera->SetNormalizedViewport(AABB2::ZERO_TO_ONE);

    Mat44 c2r;

    c2r.m_values[Mat44::Ix] = 0.f;
    c2r.m_values[Mat44::Iz] = 1.f;
    c2r.m_values[Mat44::Jx] = -1.f;
    c2r.m_values[Mat44::Jy] = 0.f;
    c2r.m_values[Mat44::Ky] = 1.f;
    c2r.m_values[Mat44::Kz] = 0.f;

    m_worldCamera->SetCameraToRenderTransform(c2r);

    m_position    = Vec3(-50, -50, 150);
    m_orientation = EulerAngles(45, 45, 0);

    // Assignment 6: Initialize Player physics AABB
    // Player is 1.80m tall, 0.60m wide (0.30m radius)
    // m_position is feet position, AABB is local space relative to position
    // Box extends from feet (z=0.0) to head (z=1.80), centered at (0, 0, 0.90)
    m_physicsAABB = AABB3(Vec3(-0.30f, -0.30f, 0.0f), Vec3(0.30f, 0.30f, 1.80f));

    // Assignment 6: Enable physics and set default mode to WALKING
    m_physicsMode = ePhysicsMode::WALKING;
    m_physicsEnabled = true;

    // Assignment 7: Initialize mining system
    m_targetBlockCoords = IntVec3(0, 0, 0);

    // Assignment 7: Load crack overlay texture (10 stages horizontal strip)
    // NOTE: Cracks.png must exist with 10 crack stages (640x64 pixels, 64x64 per stage)
    m_crackTexture = g_resourceSubsystem->CreateOrGetTextureFromFile("Data/Images/Cracks.png");
}

//----------------------------------------------------------------------------------------------------
Player::~Player()
{
    GAME_SAFE_RELEASE(m_worldCamera);
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Override Entity::Update() to ensure correct call order
// Critical order: UpdateFromInput() → Entity::Update() (physics) → UpdateCamera()
// This wiring enables:
// 1. Input builds acceleration (UpdateFromInput)
// 2. Physics integrates acceleration → velocity → position (Entity::Update)
// 3. Camera follows updated player state (UpdateCamera)
void Player::Update(float const deltaSeconds)
{
    // Step 1: Process all input and build acceleration for this frame
    // In spectator modes, this updates m_spectatorPosition/Orientation instead
    // Assignment 7-UI: Skip world input if modal widget is active (inventory open)
    if (g_widgetSubsystem == nullptr || !g_widgetSubsystem->HasModalWidget())
    {
        UpdateFromInput(deltaSeconds);
    }

    // Step 2: Run Newtonian physics simulation (gravity, friction, collision)
    // This integrates m_acceleration → m_velocity → m_position
    // Only affects player in non-spectator modes (spectator modes have no acceleration)
    Entity::Update(deltaSeconds);

    // Step 3: Update mining state machine (Assignment 7)
    // Handles progressive block breaking with left-click
    // Assignment 7-UI: Skip mining/placement if modal widget is active (inventory open)
    if (g_widgetSubsystem == nullptr || !g_widgetSubsystem->HasModalWidget())
    {
        UpdateMining(deltaSeconds);

        // Step 3b: Update block placement (Assignment 7)
        // Handles right-click block placement
        UpdatePlacement();
    }

    // Step 3c: Pickup nearby ItemEntity objects (Assignment 7)
    // Handles automatic item collection
    PickupNearbyItems();

    // Step 4: Update camera transform based on current camera mode
    // Camera positioning depends on final player position after physics
    UpdateCamera();
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Render player debug visualization
// Draw physics AABB as cyan wireframe (visible through walls) and position sphere at feet
void Player::Render() const
{
    // Display current camera mode and physics mode at top-left of screen
    // This provides essential feedback about current control state
    const char* cameraModeNames[] = {
        "FIRST_PERSON",   // eCameraMode::FIRST_PERSON
        "OVER_SHOULDER",  // eCameraMode::OVER_SHOULDER
        "SPECTATOR",      // eCameraMode::SPECTATOR
        "SPECTATOR_XY",   // eCameraMode::SPECTATOR_XY
        "INDEPENDENT"     // eCameraMode::INDEPENDENT
    };

    const char* physicsModeNames[] = {
        "WALKING",  // ePhysicsMode::WALKING
        "FLYING",   // ePhysicsMode::FLYING
        "NOCLIP"    // ePhysicsMode::NOCLIP
    };

    int cameraModeIndex = static_cast<int>(m_cameraMode);
    int physicsModeIndex = static_cast<int>(m_physicsMode);

    // Display mode information at top-left with key hints
    std::string modeDisplayText = Stringf("Camera: %s (C)  |  Physics: %s (V)",
        cameraModeNames[cameraModeIndex],
        physicsModeNames[physicsModeIndex]);

    // Position at top-left corner with small padding
    Vec2 position = Vec2(10.f, 1060.f);  // Near top-left (assuming 1080p screen height)
    float textSize = 20.f;
    Vec2 alignment = Vec2::ZERO;  // Left-aligned
    float duration = 0.0f;  // Single frame (redrawn every frame)
    Rgba8 textColor = Rgba8::YELLOW;  // High visibility yellow text

    DebugAddScreenText(modeDisplayText, position, textSize, alignment, duration, textColor, textColor);

    // Only render debug visualization if camera is not in first-person attached to this player
    // In first-person mode, we don't want to see our own collision box blocking the view
    if (m_cameraMode != eCameraMode::FIRST_PERSON)
    {
        // Get world-space AABB (m_physicsAABB transformed by m_position)
        AABB3 worldAABB = GetWorldAABB();

        // Draw AABB wireframe as 12 edges (cyan, X-ray mode for visibility through walls)
        // Duration 0.0f = single frame only (redrawn every frame)
        Rgba8 const wireframeColor = Rgba8::CYAN;
        float const lineDuration = 0.0f;
        float const lineThickness = 0.02f;  // Thin lines for wireframe
        eDebugRenderMode const mode = eDebugRenderMode::X_RAY;

        // Bottom face (4 edges at z = mins.z)
        Vec3 bottomBL = Vec3(worldAABB.m_mins.x, worldAABB.m_mins.y, worldAABB.m_mins.z);
        Vec3 bottomBR = Vec3(worldAABB.m_maxs.x, worldAABB.m_mins.y, worldAABB.m_mins.z);
        Vec3 bottomTR = Vec3(worldAABB.m_maxs.x, worldAABB.m_maxs.y, worldAABB.m_mins.z);
        Vec3 bottomTL = Vec3(worldAABB.m_mins.x, worldAABB.m_maxs.y, worldAABB.m_mins.z);

        DebugAddWorldLine(bottomBL, bottomBR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomBR, bottomTR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomTR, bottomTL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomTL, bottomBL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);

        // Top face (4 edges at z = maxs.z)
        Vec3 topBL = Vec3(worldAABB.m_mins.x, worldAABB.m_mins.y, worldAABB.m_maxs.z);
        Vec3 topBR = Vec3(worldAABB.m_maxs.x, worldAABB.m_mins.y, worldAABB.m_maxs.z);
        Vec3 topTR = Vec3(worldAABB.m_maxs.x, worldAABB.m_maxs.y, worldAABB.m_maxs.z);
        Vec3 topTL = Vec3(worldAABB.m_mins.x, worldAABB.m_maxs.y, worldAABB.m_maxs.z);

        DebugAddWorldLine(topBL, topBR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(topBR, topTR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(topTR, topTL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(topTL, topBL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);

        // Vertical edges (4 edges connecting bottom to top)
        DebugAddWorldLine(bottomBL, topBL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomBR, topBR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomTR, topTR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomTL, topTL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);

        // Draw cyan sphere at player feet position (origin marker)
        DebugAddWorldWireSphere(m_position, 0.1f, lineDuration, wireframeColor, wireframeColor, mode);
    }

    // Assignment 7: Render mining crack overlay on target block
    RenderMiningProgress();
}

//----------------------------------------------------------------------------------------------------
void Player::UpdateFromInput(float const deltaSeconds)
{
    UpdateFromKeyBoard(deltaSeconds);
    UpdateFromController(deltaSeconds);
}

//----------------------------------------------------------------------------------------------------
void Player::UpdateFromKeyBoard(float deltaSeconds)
{
    // Assignment 6: Physics-based movement with acceleration instead of direct velocity manipulation

    // Debug: Reset position to origin on H key
    if (g_input->WasKeyJustPressed(KEYCODE_H))
    {
        if (m_game->IsAttractMode() == false)
        {
            m_position    = Vec3::ZERO;
            m_orientation = EulerAngles::ZERO;
        }
    }

    // Assignment 6: Camera mode cycling with C key
    // Cycle: FIRST_PERSON → OVER_SHOULDER → SPECTATOR → SPECTATOR_XY → INDEPENDENT → FIRST_PERSON
    if (g_input->WasKeyJustPressed(KEYCODE_C))
    {
        // Store previous mode to detect spectator transitions
        eCameraMode previousMode = m_cameraMode;

        // Cycle to next mode (modulo 5 for wraparound)
        int currentModeInt = static_cast<int>(m_cameraMode);
        int nextModeInt = (currentModeInt + 1) % 5;
        m_cameraMode = static_cast<eCameraMode>(nextModeInt);

        // When transitioning TO spectator modes, save current camera state
        // This preserves the player's current view when entering free-flying mode
        bool enteringSpectator = (previousMode == eCameraMode::FIRST_PERSON || previousMode == eCameraMode::OVER_SHOULDER);
        bool newModeIsSpectator = (m_cameraMode == eCameraMode::SPECTATOR ||
                                   m_cameraMode == eCameraMode::SPECTATOR_XY ||
                                   m_cameraMode == eCameraMode::INDEPENDENT);

        if (enteringSpectator && newModeIsSpectator)
        {
            // Preserve current camera position and orientation when detaching
            m_spectatorPosition = m_position;
            m_spectatorOrientation = m_orientation;
        }

        // Display mode change for debugging
        switch (m_cameraMode)
        {
            case eCameraMode::FIRST_PERSON:
                DebuggerPrintf("Camera Mode: FIRST_PERSON (Eye height, player not rendered)\n");
                break;
            case eCameraMode::OVER_SHOULDER:
                DebuggerPrintf("Camera Mode: OVER_SHOULDER (4m behind, player visible)\n");
                break;
            case eCameraMode::SPECTATOR:
                DebuggerPrintf("Camera Mode: SPECTATOR (Free-fly detached, full 3D)\n");
                break;
            case eCameraMode::SPECTATOR_XY:
                DebuggerPrintf("Camera Mode: SPECTATOR_XY (Free-fly detached, XY-plane only)\n");
                break;
            case eCameraMode::INDEPENDENT:
                DebuggerPrintf("Camera Mode: INDEPENDENT (Frozen camera, player moves)\n");
                break;
        }
    }

    // Assignment 6: Physics mode cycling with V key
    // Cycle: WALKING → FLYING → NOCLIP → WALKING
    if (g_input->WasKeyJustPressed(KEYCODE_V))
    {
        // Store previous mode for safety check
        ePhysicsMode previousMode = m_physicsMode;

        // Cycle to next mode (modulo 3 for wraparound)
        int currentModeInt = static_cast<int>(m_physicsMode);
        int nextModeInt = (currentModeInt + 1) % 3;
        m_physicsMode = static_cast<ePhysicsMode>(nextModeInt);

        // Safety: When exiting NOCLIP mode, push player out of blocks to prevent stuck state
        if (previousMode == ePhysicsMode::NOCLIP && m_physicsMode != ePhysicsMode::NOCLIP)
        {
            World* world = m_game->GetWorld();
            if (world != nullptr)
            {
                world->PushEntityOutOfBlocks(this);
            }
        }

        // Display mode change for debugging
        switch (m_physicsMode)
        {
            case ePhysicsMode::WALKING:
                DebuggerPrintf("Physics Mode: WALKING (Gravity + Collision, Jump enabled)\n");
                break;
            case ePhysicsMode::FLYING:
                DebuggerPrintf("Physics Mode: FLYING (No gravity, Collision enabled, Q/E vertical)\n");
                break;
            case ePhysicsMode::NOCLIP:
                DebuggerPrintf("Physics Mode: NOCLIP (No gravity, No collision, Q/E vertical)\n");
                break;
        }
    }

    // Assignment 7: Hotbar slot selection with number keys (1-9)
    // Maps keyboard 1-9 to hotbar slots 0-8
    if (g_input->WasKeyJustPressed(NUMCODE_1)) m_inventory.SetSelectedHotbarSlot(0);
    if (g_input->WasKeyJustPressed(NUMCODE_2)) m_inventory.SetSelectedHotbarSlot(1);
    if (g_input->WasKeyJustPressed(NUMCODE_3)) m_inventory.SetSelectedHotbarSlot(2);
    if (g_input->WasKeyJustPressed(NUMCODE_4)) m_inventory.SetSelectedHotbarSlot(3);
    if (g_input->WasKeyJustPressed(NUMCODE_5)) m_inventory.SetSelectedHotbarSlot(4);
    if (g_input->WasKeyJustPressed(NUMCODE_6)) m_inventory.SetSelectedHotbarSlot(5);
    if (g_input->WasKeyJustPressed(NUMCODE_7)) m_inventory.SetSelectedHotbarSlot(6);
    if (g_input->WasKeyJustPressed(NUMCODE_8)) m_inventory.SetSelectedHotbarSlot(7);
    if (g_input->WasKeyJustPressed(NUMCODE_9)) m_inventory.SetSelectedHotbarSlot(8);

    // Assignment 6: Spectator camera mode handling
    // In SPECTATOR/SPECTATOR_XY modes: freeze player, move spectator camera instead
    // In INDEPENDENT mode: player moves normally, camera stays frozen
    bool isSpectatorMode = (m_cameraMode == eCameraMode::SPECTATOR || m_cameraMode == eCameraMode::SPECTATOR_XY);

    if (isSpectatorMode)
    {
        // Spectator mode: Move camera, not player
        // Build local-space movement vector from WASD input
        Vec3 localMovement = Vec3::ZERO;

        if (g_input->IsKeyDown(KEYCODE_W)) localMovement.x += 1.0f;  // Forward
        if (g_input->IsKeyDown(KEYCODE_S)) localMovement.x -= 1.0f;  // Backward
        if (g_input->IsKeyDown(KEYCODE_A)) localMovement.y += 1.0f;  // Left (positive = left direction)
        if (g_input->IsKeyDown(KEYCODE_D)) localMovement.y -= 1.0f;  // Right (negative = -left direction)

        // Q/E vertical movement (allowed in both SPECTATOR and SPECTATOR_XY)
        if (g_input->IsKeyDown(KEYCODE_Q)) localMovement.z -= 1.0f;  // Down
        if (g_input->IsKeyDown(KEYCODE_E)) localMovement.z += 1.0f;  // Up

        // SPECTATOR_XY mode: Lock Z movement (XY-plane only)
        if (m_cameraMode == eCameraMode::SPECTATOR_XY)
        {
            localMovement.z = 0.0f;
        }

        // Normalize to prevent diagonal speed exploit
        if (localMovement.GetLengthSquared() > 0.0f)
        {
            localMovement = localMovement.GetNormalized();
        }

        // Transform local movement to world space using spectator orientation
        Vec3 forward, left, up;
        m_spectatorOrientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

        Vec3 worldMovement = (forward * localMovement.x) + (left * localMovement.y) + (up * localMovement.z);

        // Apply sprint multiplier if Shift held
        float sprintMultiplier = g_input->IsKeyDown(KEYCODE_SHIFT) ? PLAYER_SPRINT_MULTIPLIER : 1.0f;

        // Update spectator camera position directly (not physics-based)
        float spectatorSpeed = 10.0f;  // Spectator camera movement speed (m/s)
        m_spectatorPosition += worldMovement * spectatorSpeed * sprintMultiplier * deltaSeconds;

        // Mouse look updates spectator orientation (not player orientation)
        m_spectatorOrientation.m_yawDegrees -= g_input->GetCursorClientDelta().x * 0.075f;
        m_spectatorOrientation.m_pitchDegrees += g_input->GetCursorClientDelta().y * 0.075f;
        m_spectatorOrientation.m_pitchDegrees = GetClamped(m_spectatorOrientation.m_pitchDegrees, -85.f, 85.f);

        // Early return: Don't process player movement in spectator modes
        UpdateCamera();
        return;
    }

    // Step 1: Build local-space movement vector from WASD input
    Vec3 localMovement = Vec3::ZERO;

    if (g_input->IsKeyDown(KEYCODE_W)) localMovement.x += 1.0f;  // Forward
    if (g_input->IsKeyDown(KEYCODE_S)) localMovement.x -= 1.0f;  // Backward
    if (g_input->IsKeyDown(KEYCODE_A)) localMovement.y += 1.0f;  // Left (positive = left direction)
    if (g_input->IsKeyDown(KEYCODE_D)) localMovement.y -= 1.0f;  // Right (negative = -left direction)

    // Q/E vertical movement only allowed in FLYING and NOCLIP modes
    if (m_physicsMode == ePhysicsMode::FLYING || m_physicsMode == ePhysicsMode::NOCLIP)
    {
        if (g_input->IsKeyDown(KEYCODE_Q)) localMovement.z -= 1.0f;  // Down
        if (g_input->IsKeyDown(KEYCODE_E)) localMovement.z += 1.0f;  // Up
    }

    // WALKING mode: Force Z component to zero (no manual vertical movement)
    if (m_physicsMode == ePhysicsMode::WALKING)
    {
        localMovement.z = 0.0f;
    }

    // Step 2: Normalize to prevent diagonal speed exploit (√2 speed when moving diagonally)
    if (localMovement.GetLengthSquared() > 0.0f)
    {
        localMovement = localMovement.GetNormalized();
    }

    // Step 4: Transform local movement to world space using player orientation
    Vec3 forward, left, up;
    m_orientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

    Vec3 worldMovement = (forward * localMovement.x) + (left * localMovement.y) + (up * localMovement.z);

    // Step 4: Apply sprint multiplier if Shift held
    float sprintMultiplier = g_input->IsKeyDown(KEYCODE_SHIFT) ? PLAYER_SPRINT_MULTIPLIER : 1.0f;

    // Step 5: Add acceleration (force) for this frame
    // Physics system will integrate this into velocity in Entity::UpdatePhysics()
    m_acceleration += worldMovement * PLAYER_WALK_ACCELERATION * sprintMultiplier;

    // Assignment 6: Jump logic - Space key applies instant vertical velocity impulse
    // Only works in WALKING mode when player is grounded (no double-jump, no air-jump)
    if (g_input->WasKeyJustPressed(KEYCODE_SPACE))
    {
        // Check if player can jump: must be grounded AND in WALKING mode
        if (m_isOnGround && m_physicsMode == ePhysicsMode::WALKING)
        {
            // Apply instant upward velocity impulse (+8.5 m/s)
            // This is an impulse (instant velocity change), not acceleration
            m_velocity.z = PLAYER_JUMP_VELOCITY;
        }
    }

    // Mouse look: Update player orientation
    // INDEPENDENT mode: Mouse controls player orientation for WASD movement direction (camera stays frozen)
    // Other modes: Mouse controls both player and camera orientation
    m_orientation.m_yawDegrees -= g_input->GetCursorClientDelta().x * 0.075f;
    m_orientation.m_pitchDegrees += g_input->GetCursorClientDelta().y * 0.075f;
    m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -85.f, 85.f);

    // Note: UpdateCamera() now called in Player::Update() after Entity::Update() (physics)
    // This ensures camera positioning uses final player position after physics integration
}

//----------------------------------------------------------------------------------------------------
void Player::UpdateFromController(float deltaSeconds)
{
    XboxController const& controller = g_input->GetController(0);

    if (controller.WasButtonJustPressed(XBOX_BUTTON_START))
    {
        if (m_game->IsAttractMode() == false)
        {
            m_position    = Vec3::ZERO;
            m_orientation = EulerAngles::ZERO;
        }
    }

    Vec2 const leftStickInput = controller.GetLeftStick().GetPosition();
    m_velocity += Vec3(leftStickInput.y, -leftStickInput.x, 0.f) * m_moveSpeed;

    if (controller.IsButtonDown(XBOX_BUTTON_LSHOULDER)) m_velocity -= Vec3(0.f, 0.f, 1.f) * m_moveSpeed;
    if (controller.IsButtonDown(XBOX_BUTTON_RSHOULDER)) m_velocity += Vec3(0.f, 0.f, 1.f) * m_moveSpeed;

    if (controller.IsButtonDown(XBOX_BUTTON_A)) deltaSeconds *= 20.f;

    m_position += m_velocity * deltaSeconds;

    Vec2 const rightStickInput = controller.GetRightStick().GetPosition();
    m_orientation.m_yawDegrees -= rightStickInput.x * 0.125f;
    m_orientation.m_pitchDegrees -= rightStickInput.y * 0.125f;

    m_angularVelocity.m_rollDegrees = 0.f;

    float const leftTriggerInput  = controller.GetLeftTrigger();
    float const rightTriggerInput = controller.GetRightTrigger();

    if (leftTriggerInput != 0.f)
    {
        m_angularVelocity.m_rollDegrees -= 90.f;
    }

    if (rightTriggerInput != 0.f)
    {
        m_angularVelocity.m_rollDegrees += 90.f;
    }

    m_orientation.m_rollDegrees += m_angularVelocity.m_rollDegrees * deltaSeconds;
    m_orientation.m_rollDegrees = GetClamped(m_orientation.m_rollDegrees, -45.f, 45.f);

    // Note: UpdateCamera() now called in Player::Update() after Entity::Update() (physics)
    // This ensures camera positioning uses final player position after physics integration
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Position camera based on current camera mode
void Player::UpdateCamera()
{
    switch (m_cameraMode)
    {
        case eCameraMode::FIRST_PERSON:
        {
            // Camera positioned at player eye height (1.65m above feet)
            // Orientation matches player orientation exactly
            // Player entity should not be rendered in this mode
            Vec3 eyePosition = GetEyePosition();
            m_worldCamera->SetPositionAndOrientation(eyePosition, m_orientation);
            break;
        }

        case eCameraMode::OVER_SHOULDER:
        {
            // Camera positioned 4m behind player eye position, with raycast wall clipping prevention
            // Player entity is visible in this mode (third-person view)
            Vec3 eyePosition = GetEyePosition();

            // Get forward vector from player orientation
            Vec3 forward, left, up;
            m_orientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

            // Calculate desired camera position (4m behind eye position)
            Vec3 desiredCameraPos = eyePosition - (forward * CAMERA_OVER_SHOULDER_DISTANCE);

            // Raycast from eye to desired camera position to check for walls
            // Direction is backward (-forward), distance is 4.0m
            RaycastResult result = m_game->GetWorld()->RaycastVoxel(eyePosition, -forward, CAMERA_OVER_SHOULDER_DISTANCE);

            // If ray hit a wall, pull camera forward to prevent clipping
            // Offset by 0.1m along impact normal to avoid z-fighting
            Vec3 actualCameraPos = result.m_didImpact
                ? (result.m_impactPosition + result.m_impactNormal * 0.1f)
                : desiredCameraPos;

            m_worldCamera->SetPositionAndOrientation(actualCameraPos, m_orientation);
            break;
        }

        case eCameraMode::SPECTATOR:
        {
            // Free-flying detached camera with full 3D movement
            // Camera uses m_spectatorPosition and m_spectatorOrientation (updated by input)
            // Player is frozen (no movement input applied to player)
            m_worldCamera->SetPositionAndOrientation(m_spectatorPosition, m_spectatorOrientation);
            break;
        }

        case eCameraMode::SPECTATOR_XY:
        {
            // Free-flying detached camera with XY-plane only movement (Z locked)
            // Camera uses m_spectatorPosition and m_spectatorOrientation
            // Player is frozen (no movement input applied to player)
            m_worldCamera->SetPositionAndOrientation(m_spectatorPosition, m_spectatorOrientation);
            break;
        }

        case eCameraMode::INDEPENDENT:
        {
            // Camera frozen at m_spectatorPosition with m_spectatorOrientation
            // Player continues to move normally (decoupled from camera)
            // Useful for debugging and observing player movement from fixed viewpoint
            m_worldCamera->SetPositionAndOrientation(m_spectatorPosition, m_spectatorOrientation);
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------
Camera* Player::GetCamera() const
{
    return m_worldCamera;
}

Vec3 const& Player::GetVelocity()
{
    return m_velocity;
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Helper method to calculate player eye position
// Returns player position offset by eye height (1.65m above feet)
Vec3 Player::GetEyePosition() const
{
    return m_position + Vec3(0.f, 0.f, PLAYER_EYE_HEIGHT);
}

//----------------------------------------------------------------------------------------------------
// Get current camera mode
eCameraMode Player::GetCameraMode() const
{
    return m_cameraMode;
}
//----------------------------------------------------------------------------------------------------
// Assignment 7: Mining state machine - Progressive block breaking
// Handles IDLE → MINING → BROKEN state transitions
//----------------------------------------------------------------------------------------------------
void Player::UpdateMining(float deltaSeconds)
{
    World* world = m_game->GetWorld();
    if (world == nullptr)
    {
        return; // No world, no mining
    }

    // Assignment 7-UI: Don't process mining input when inventory is open
    if (m_game->IsInventoryOpen())
    {
        // If currently mining, cancel mining state
        if (m_miningState == eMiningState::MINING)
        {
            m_miningState = eMiningState::IDLE;
            m_miningProgress = 0.0f;
        }
        return;
    }

    // Get raycast from camera (6 block range)
    Camera* camera = GetCamera();
    Vec3 rayStart = camera->GetPosition();

    // Extract forward vector from camera orientation
    EulerAngles cameraOrientation = camera->GetOrientation();
    Vec3 forwardIBasis, leftJBasis, upKBasis;
    cameraOrientation.GetAsVectors_IFwd_JLeft_KUp(forwardIBasis, leftJBasis, upKBasis);
    Vec3 rayDirection = forwardIBasis;

    float rayDistance = 6.0f; // Minecraft mining range

    RaycastResult raycast = world->RaycastVoxel(rayStart, rayDirection, rayDistance);

    // Check if left mouse button is pressed
    bool isLeftMouseDown = g_input->IsKeyDown(KEYCODE_LEFT_MOUSE);

    // State machine
    switch (m_miningState)
    {
        case eMiningState::IDLE:
        {
            // IDLE → MINING: Start mining if left-click on solid block
            if (isLeftMouseDown && raycast.m_didImpact && world->IsBlockSolid(raycast.m_impactBlockCoords))
            {
                m_miningState = eMiningState::MINING;
                m_targetBlockCoords = raycast.m_impactBlockCoords;
                m_miningProgress = 0.0f;
                m_breakTime = CalculateBreakTime(m_targetBlockCoords);
            }
            break;
        }

        case eMiningState::MINING:
        {
            // Check cancel conditions
            bool shouldCancel = false;

            // Cancel if mouse released
            if (!isLeftMouseDown)
            {
                shouldCancel = true;
            }

            // Cancel if target out of view or out of range
            if (!raycast.m_didImpact || raycast.m_impactBlockCoords != m_targetBlockCoords)
            {
                shouldCancel = true;
            }

            if (shouldCancel)
            {
                // MINING → IDLE: Reset mining progress
                m_miningState = eMiningState::IDLE;
                m_miningProgress = 0.0f;
                break;
            }

            // Increment mining progress
            m_miningProgress += deltaSeconds / m_breakTime;

            // Check if block broken
            if (m_miningProgress >= 1.0f)
            {
                // MINING → BROKEN: Break the block
                BreakBlock(m_targetBlockCoords);
                m_miningState = eMiningState::BROKEN;
            }
            break;
        }

        case eMiningState::BROKEN:
        {
            // BROKEN → IDLE: Reset to idle next frame
            m_miningState = eMiningState::IDLE;
            m_miningProgress = 0.0f;
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 7: Block placement with right-click
// Handles raycasting, validation, item consumption, and block placement
//----------------------------------------------------------------------------------------------------
void Player::UpdatePlacement()
{
    // Assignment 7-UI: Don't process placement input when inventory is open
    if (m_game->IsInventoryOpen())
    {
        return;
    }

    // a. Check if right mouse button just pressed
    if (!g_input->WasKeyJustPressed(KEYCODE_RIGHT_MOUSE))
    {
        return; // Not clicking right mouse
    }

    // b. Raycast to find placement position
    RaycastResult raycast = RaycastForPlacement(6.0f);

    // c. Check if ray hit a block
    if (!raycast.m_didImpact)
    {
        return; // No block hit, cannot place
    }

    // d. Calculate placement coords (adjacent to hit face)
    IntVec3 placementCoords = raycast.m_impactBlockCoords + IntVec3(
        static_cast<int>(raycast.m_impactNormal.x),
        static_cast<int>(raycast.m_impactNormal.y),
        static_cast<int>(raycast.m_impactNormal.z)
    );

    // e. Validate placement position
    if (!CanPlaceBlock(placementCoords))
    {
        return; // Cannot place at this position
    }

    // f. Get selected item from inventory
    ItemStack& selectedItem = GetSelectedItemStack();

    // g. Check if item exists
    if (selectedItem.IsEmpty())
    {
        return; // No item selected
    }

    // h. Get item definition and check if it's a BLOCK type
    sItemDefinition* itemDef = ItemRegistry::GetInstance().Get(selectedItem.itemID);
    if (itemDef == nullptr || !itemDef->IsBlock())
    {
        return; // Not a block item, cannot place
    }

    // i. Get block type ID from item definition (CRITICAL: Use GetBlockTypeID(), NOT itemID!)
    uint8_t blockTypeID = static_cast<uint8_t>(itemDef->GetBlockTypeID());

    // j. Place the block in the world
    World* world = m_game->GetWorld();
    if (world != nullptr)
    {
        world->SetBlockAtGlobalCoords(placementCoords, blockTypeID);

        // k. Consume 1 item from inventory
        selectedItem.Take(1);

        // l. Play block placement sound effect (Assignment 7: Sound effects)
        if (g_audio != nullptr)
        {
            // Subtle wood placement sound (wood-3.mp3 from SoundDino)
            SoundID placeSound = g_audio->CreateOrGetSound("Data/Audio/block_place.mp3", eAudioSystemSoundDimension::Sound2D);
            if (placeSound != MISSING_SOUND_ID)
            {
                g_audio->StartSound(placeSound, false, 0.4f);  // 40% volume for subtle placement sound
            }
        }

        // m. Mark chunk for mesh rebuild
        IntVec2 chunkCoords = Chunk::GetChunkCoords(placementCoords);
        Chunk* chunk = world->GetChunk(chunkCoords);
        if (chunk != nullptr)
        {
            world->MarkChunkForMeshRebuild(chunk);
        }
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 7: Pickup nearby ItemEntity objects
// Checks collision with ItemEntities within pickup radius and adds to inventory
//----------------------------------------------------------------------------------------------------
void Player::PickupNearbyItems()
{
    World* world = m_game->GetWorld();
    if (world == nullptr)
    {
        return;
    }

    // Get player AABB for collision detection
    AABB3 playerAABB = GetWorldAABB();

    // Define pickup radius (slightly larger than collision radius for magnetic pull)
    constexpr float PICKUP_RADIUS = 2.0f;

    // Get all ItemEntities within pickup radius
    std::vector<ItemEntity*> nearbyItems = world->GetNearbyItemEntities(m_position, PICKUP_RADIUS);

    // Check each ItemEntity for collision
    for (ItemEntity* itemEntity : nearbyItems)
    {
        if (itemEntity == nullptr || !itemEntity->CanBePickedUp())
        {
            continue; // Skip null or items still in pickup cooldown
        }

        // Get ItemEntity AABB (items use 0.25 x 0.25 x 0.25 bounds)
        AABB3 itemAABB = itemEntity->GetWorldAABB();

        // Check if player AABB overlaps with item AABB
        if (DoAABB3sOverlap3D(playerAABB, itemAABB))
        {
            // Attempt pickup (TryPickup adds to inventory and returns true on success)
            if (itemEntity->TryPickup(this))
            {
                // Item was successfully picked up and added to inventory
                // TryPickup() internally marks the entity for deletion
                // No further action needed here
                DebuggerPrintf("[PICKUP] Player picked up item! Inventory updated.\n");
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 7: Raycast for block placement
// Returns RaycastResult with hit block and adjacent placement coordinates
// Placement coords = hit block + hit normal (for placing blocks on faces)
//----------------------------------------------------------------------------------------------------
RaycastResult Player::RaycastForPlacement(float maxDistance) const
{
    // Get camera position and orientation
    Vec3 rayStart = GetEyePosition();

    // Extract forward vector from camera orientation
    Vec3 forwardIBasis, leftJBasis, upKBasis;
    EulerAngles cameraOrientation = m_worldCamera->GetOrientation();
    cameraOrientation.GetAsVectors_IFwd_JLeft_KUp(forwardIBasis, leftJBasis, upKBasis);
    Vec3 rayDirection = forwardIBasis;

    // Perform raycast
    World* world = m_game->GetWorld();
    if (world == nullptr)
    {
        return RaycastResult(); // Return empty result (no impact)
    }

    RaycastResult result = world->RaycastVoxel(rayStart, rayDirection, maxDistance);

    // Note: The RaycastResult already contains:
    // - m_didImpact: true if hit a block
    // - m_impactBlockCoords: coordinates of the hit block
    // - m_impactNormal: face direction (used to calculate placement coords)
    //
    // Placement coords can be calculated by caller as:
    // IntVec3 placementCoords = result.m_impactBlockCoords + IntVec3::FromVec3(result.m_impactNormal);

    return result;
}

//----------------------------------------------------------------------------------------------------
// Assignment 7: Validate block placement position
// Checks: position not occupied, player not intersecting, within reach
//----------------------------------------------------------------------------------------------------
bool Player::CanPlaceBlock(IntVec3 const& blockCoords) const
{
    World* world = m_game->GetWorld();
    if (world == nullptr)
    {
        return false;
    }

    // Check 1: Position not occupied by solid block
    if (world->IsBlockSolid(blockCoords))
    {
        return false; // Cannot place block on occupied position
    }

    // Check 2: Player not intersecting placed block position
    // Create AABB3 for block position (1x1x1 block at coords)
    Vec3 blockMins = Vec3(
        static_cast<float>(blockCoords.x),
        static_cast<float>(blockCoords.y),
        static_cast<float>(blockCoords.z)
    );
    Vec3 blockMaxs = blockMins + Vec3(1.0f, 1.0f, 1.0f);
    AABB3 blockAABB = AABB3(blockMins, blockMaxs);

    // Get player world-space AABB (physics bounds transformed by position)
    AABB3 playerAABB = GetWorldAABB();

    // Check if player would intersect the placed block
    if (DoAABB3sOverlap3D(playerAABB, blockAABB))
    {
        return false; // Cannot place block inside player (prevent self-block-in)
    }

    // Check 3: Within 6-block reach (Minecraft standard)
    Vec3 blockCenter = Vec3(
        static_cast<float>(blockCoords.x) + 0.5f,
        static_cast<float>(blockCoords.y) + 0.5f,
        static_cast<float>(blockCoords.z) + 0.5f
    );

    float distanceToBlock = (blockCenter - m_position).GetLength();
    if (distanceToBlock > 6.0f)
    {
        return false; // Beyond reach limit
    }

    // All checks passed - block can be placed
    return true;
}

//----------------------------------------------------------------------------------------------------
// Assignment 7: Calculate break time based on block type
// Returns time in seconds required to break the block
// TODO: Use block hardness property when available in BlockDefinition
//----------------------------------------------------------------------------------------------------
float Player::CalculateBreakTime(IntVec3 const& blockCoords) const
{
    World* world = m_game->GetWorld();
    if (world == nullptr)
    {
        return 1.0f; // Default break time
    }

    // Get block type index
    uint8_t typeIndex = world->GetBlockTypeAtGlobalCoords(blockCoords);
    
    if (typeIndex == 0)
    {
        return 0.1f; // AIR breaks instantly
    }

    sBlockDefinition const* blockDef = sBlockDefinition::GetDefinitionByIndex(typeIndex);
    if (blockDef == nullptr)
    {
        return 1.0f; // Default break time
    }

    // Simple hardness system (until BlockDefinition has m_hardness property)
    // Solid blocks take longer to break than non-solid blocks
    float hardness = blockDef->IsSolid() ? 1.5f : 0.5f;

    // Get tool effectiveness from selected item
    // TODO: Query ItemDefinition for mining speed when ItemRegistry is available
    float toolEffectiveness = 1.0f; // Default: hand mining speed

    // Calculate break time: breakTime = hardness / toolEffectiveness
    float breakTime = hardness / toolEffectiveness;

    return breakTime;
}

//----------------------------------------------------------------------------------------------------
// Assignment 7: Break block and spawn ItemEntity
// Destroys the block and creates dropped item in world
//----------------------------------------------------------------------------------------------------
void Player::BreakBlock(IntVec3 const& blockCoords)
{
    World* world = m_game->GetWorld();
    if (world == nullptr)
    {
        return;
    }

    // a. Get block type before destroying it (for item drop determination)
    uint8_t blockTypeIndex = world->GetBlockTypeAtGlobalCoords(blockCoords);

    // b. Determine drop item (handle special rules)
    uint16_t dropItemID = 0;
    uint8_t dropQuantity = 1;

    // Special drop rules
    if (blockTypeIndex == BLOCK_WATER)
    {
        // WATER drops nothing
        dropItemID = 0;
        dropQuantity = 0;
    }
    else if (blockTypeIndex == BLOCK_GRASS)
    {
        // GRASS drops DIRT item (not GRASS)
        // Use ItemRegistry to map BLOCK_DIRT → "dirt_block" item ID
        dropItemID = ItemRegistry::GetInstance().GetItemIDByBlockType(BLOCK_DIRT);
        dropQuantity = 1;

        if (dropItemID == static_cast<uint16_t>(-1))
        {
            // Fallback: If ItemRegistry mapping fails, drop nothing
            DebuggerPrintf("[PLAYER] WARNING: No item found for BLOCK_DIRT, dropping nothing\n");
            dropItemID = 0;
            dropQuantity = 0;
        }
    }
    else
    {
        // Default: Use ItemRegistry to map block type ID → item ID
        dropItemID = ItemRegistry::GetInstance().GetItemIDByBlockType(blockTypeIndex);
        dropQuantity = 1;

        // If no item exists for this block type, drop nothing
        if (dropItemID == static_cast<uint16_t>(-1))
        {
            DebuggerPrintf("[PLAYER] WARNING: No item found for blockType=%u, dropping nothing\n", blockTypeIndex);
            dropItemID = 0;
            dropQuantity = 0;
        }
    }

    // c. Destroy the block (set to AIR)
    world->SetBlockAtGlobalCoords(blockCoords, BLOCK_AIR);

    // d. Calculate spawn position (block center + 0.5 offset)
    Vec3 spawnPosition = Vec3(
        static_cast<float>(blockCoords.x) + 0.5f,
        static_cast<float>(blockCoords.y) + 0.5f,
        static_cast<float>(blockCoords.z) + 0.5f
    );

    // e. Create ItemStack for drop (skip if dropQuantity == 0)
    if (dropQuantity > 0)
    {
        ItemStack droppedItem;
        droppedItem.itemID = dropItemID;
        droppedItem.quantity = dropQuantity;
        droppedItem.durability = 0;  // Non-tool items have 0 durability

        // f. Spawn the ItemEntity in world
        world->SpawnItemEntity(spawnPosition, droppedItem);
    }

    // g-h. Tool durability handling (Assignment 7 Task: Tool Durability System)
    ItemStack& selectedItem = GetSelectedItemStack();
    if (!selectedItem.IsEmpty())
    {
        // Get item definition to check if it's a tool
        sItemDefinition* itemDef = ItemRegistry::GetInstance().Get(selectedItem.itemID);
        if (itemDef != nullptr && itemDef->IsTool())
        {
            // Decrease tool durability
            if (selectedItem.durability > 0)
            {
                selectedItem.durability--;

                // Remove tool if durability reaches 0
                if (selectedItem.durability == 0)
                {
                    selectedItem.Clear(); // Remove broken tool from inventory
                }
            }
        }
    }

    // i. Mark chunk for mesh rebuild (visual update)
    IntVec2 chunkCoords = Chunk::GetChunkCoords(blockCoords);
    Chunk* chunk = world->GetChunk(chunkCoords);
    if (chunk != nullptr)
    {
        world->MarkChunkForMeshRebuild(chunk);
    }

    // j. Play block break sound effect (Assignment 7: Sound effects)
    if (g_audio != nullptr)
    {
        // Authentic Minecraft stone break sound (stone-6.mp3 from SoundDino)
        SoundID breakSound = g_audio->CreateOrGetSound("Data/Audio/block_break.mp3", eAudioSystemSoundDimension::Sound2D);
        if (breakSound != MISSING_SOUND_ID)
        {
            g_audio->StartSound(breakSound, false); 
        }
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 7: Render mining crack overlay on target block
// Displays 10-stage crack texture on all 6 faces during mining
//----------------------------------------------------------------------------------------------------
void Player::RenderMiningProgress() const
{
    // Only render if actively mining
    if (m_miningState != eMiningState::MINING)
    {
        return;
    }

    // Check if crack texture loaded
    if (m_crackTexture == nullptr)
    {
        return; // No texture, skip rendering
    }

    // Calculate crack stage (0-9) from mining progress (0.0-1.0)
    int crackStage = static_cast<int>(m_miningProgress * 10.0f);
    crackStage = GetClamped(crackStage, 0, 9); // Ensure valid range

    // Calculate UV coordinates for crack stage (horizontal strip: 10 stages @ 0.1 width each)
    float uMin = crackStage * 0.1f;
    float uMax = uMin + 0.1f;
    AABB2 crackUVs = AABB2(Vec2(uMin, 0.0f), Vec2(uMax, 1.0f));

    // Get block world position (center of block)
    Vec3 blockCenter = Vec3(
        static_cast<float>(m_targetBlockCoords.x) + 0.5f,
        static_cast<float>(m_targetBlockCoords.y) + 0.5f,
        static_cast<float>(m_targetBlockCoords.z) + 0.5f
    );

    // Offset each face slightly outward to prevent Z-fighting with block surface
    float const offset = 0.001f;

    // Create vertex list for all 6 faces
    VertexList_PCU verts;
    IndexList indices;

    // Define face normals and tangents for all 6 faces
    // Face order: +X (East), -X (West), +Y (North), -Y (South), +Z (Top), -Z (Bottom)

    // +X face (East)
    Vec3 eastCenter = blockCenter + Vec3(0.5f + offset, 0.0f, 0.0f);
    AddVertsForQuad3D(verts, indices,
        eastCenter + Vec3(0.0f, -0.5f, -0.5f), // Bottom Left
        eastCenter + Vec3(0.0f, +0.5f, -0.5f), // Bottom Right
        eastCenter + Vec3(0.0f, -0.5f, +0.5f), // Top Left
        eastCenter + Vec3(0.0f, +0.5f, +0.5f), // Top Right
        Rgba8::WHITE, crackUVs);

    // -X face (West)
    Vec3 westCenter = blockCenter + Vec3(-0.5f - offset, 0.0f, 0.0f);
    AddVertsForQuad3D(verts, indices,
        westCenter + Vec3(0.0f, +0.5f, -0.5f), // Bottom Left
        westCenter + Vec3(0.0f, -0.5f, -0.5f), // Bottom Right
        westCenter + Vec3(0.0f, +0.5f, +0.5f), // Top Left
        westCenter + Vec3(0.0f, -0.5f, +0.5f), // Top Right
        Rgba8::WHITE, crackUVs);

    // +Y face (North)
    Vec3 northCenter = blockCenter + Vec3(0.0f, 0.5f + offset, 0.0f);
    AddVertsForQuad3D(verts, indices,
        northCenter + Vec3(+0.5f, 0.0f, -0.5f), // Bottom Left
        northCenter + Vec3(-0.5f, 0.0f, -0.5f), // Bottom Right
        northCenter + Vec3(+0.5f, 0.0f, +0.5f), // Top Left
        northCenter + Vec3(-0.5f, 0.0f, +0.5f), // Top Right
        Rgba8::WHITE, crackUVs);

    // -Y face (South)
    Vec3 southCenter = blockCenter + Vec3(0.0f, -0.5f - offset, 0.0f);
    AddVertsForQuad3D(verts, indices,
        southCenter + Vec3(-0.5f, 0.0f, -0.5f), // Bottom Left
        southCenter + Vec3(+0.5f, 0.0f, -0.5f), // Bottom Right
        southCenter + Vec3(-0.5f, 0.0f, +0.5f), // Top Left
        southCenter + Vec3(+0.5f, 0.0f, +0.5f), // Top Right
        Rgba8::WHITE, crackUVs);

    // +Z face (Top)
    Vec3 topCenter = blockCenter + Vec3(0.0f, 0.0f, 0.5f + offset);
    AddVertsForQuad3D(verts, indices,
        topCenter + Vec3(-0.5f, -0.5f, 0.0f), // Bottom Left
        topCenter + Vec3(+0.5f, -0.5f, 0.0f), // Bottom Right
        topCenter + Vec3(-0.5f, +0.5f, 0.0f), // Top Left
        topCenter + Vec3(+0.5f, +0.5f, 0.0f), // Top Right
        Rgba8::WHITE, crackUVs);

    // -Z face (Bottom)
    Vec3 bottomCenter = blockCenter + Vec3(0.0f, 0.0f, -0.5f - offset);
    AddVertsForQuad3D(verts, indices,
        bottomCenter + Vec3(-0.5f, +0.5f, 0.0f), // Bottom Left
        bottomCenter + Vec3(+0.5f, +0.5f, 0.0f), // Bottom Right
        bottomCenter + Vec3(-0.5f, -0.5f, 0.0f), // Top Left
        bottomCenter + Vec3(+0.5f, -0.5f, 0.0f), // Top Right
        Rgba8::WHITE, crackUVs);

    // Render all faces with crack texture
    // Use Default shader (not World shader) for proper transparency handling
    g_renderer->BindShader(g_resourceSubsystem->CreateOrGetShaderFromFile("Data/Shaders/Default"));
    g_renderer->BindTexture(m_crackTexture);
    g_renderer->SetBlendMode(eBlendMode::ALPHA); // Use alpha blending for transparency
    g_renderer->DrawVertexArray(verts, indices); // Use indexed rendering for efficiency
}
