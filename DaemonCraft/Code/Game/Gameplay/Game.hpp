//----------------------------------------------------------------------------------------------------
// Game.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/World.hpp"
#include <unordered_map>
#include <string>
#include <memory>

#include "imgui.h"

#include "Game/EngineBuildPreferences.hpp"
#ifdef ENGINE_SCRIPTING_ENABLED
// Assignment 7-AI: KADI WebSocket integration
#include "Engine/Network/KADIWebSocketSubsystem.hpp"
#include "Engine/Network/KADIAuthenticationUtility.hpp"
#include "ThirdParty/json/json.hpp"
#endif // ENGINE_SCRIPTING_ENABLED

//-Forward-Declaration--------------------------------------------------------------------------------
class Camera;
class Clock;
class Player;
class Prop;
class HotbarWidget;
class InventoryWidget;  // Assignment 7-UI: Inventory screen widget
struct Vec3;

//----------------------------------------------------------------------------------------------------
enum class eGameState : uint8_t
{
    ATTRACT,
    GAME
};

//----------------------------------------------------------------------------------------------------
class Game
{
public:
    Game();
    ~Game();

    void Update();
    void Render() const;
    bool IsAttractMode() const;
    bool RequestedNewGame() const { return m_requestNewGame; }  // Check if F8 was pressed
    bool IsInventoryOpen() const;  // Assignment 7-UI: Check if inventory screen is visible
    Vec3 GetPlayerCameraPosition() const;
    Vec3 GetPlayerVelocity() const;  // For directional chunk preloading (Task 0.7)
    void ShowTerrainDebugWindow();

    // Accessors for debug visualization (Phase 0, Task 0.4)
    World* GetWorld() const { return m_world; }
    Player* GetPlayer() const { return m_player; }  // Assignment 7: For ItemEntity magnetic pickup


private:
#ifdef ENGINE_SCRIPTING_ENABLED
    // Assignment 7-AI: KADI initialization and callbacks
    void InitializeKADI();
    void OnKADIToolInvoke(int requestId, std::string const& toolName, nlohmann::json const& arguments);

    // Assignment 7-AI: KADI tool handlers
    void HandleSpawnAgent(int requestId, nlohmann::json const& arguments);
    void HandleDespawnAgent(int requestId, nlohmann::json const& arguments);
    void HandleListAgents(int requestId, nlohmann::json const& arguments);
    void HandleQueueCommand(int requestId, nlohmann::json const& arguments);
    void HandleGetNearbyBlocks(int requestId, nlohmann::json const& arguments);
    void HandleGetAgentInventory(int requestId, nlohmann::json const& arguments);
    void HandleGetAgentStatus(int requestId, nlohmann::json const& arguments);
#endif // ENGINE_SCRIPTING_ENABLED

    // ImGui Helper Functions (Assignment 4: Phase 5B.4)
    void ShowCurvesTab();
    void ShowBiomeNoiseTab();
    void ShowDensityTab();
    void ShowCavesTab();
    void ShowTreesTab();
    void ShowCarversTab();
    void ShowCurveEditor(char const* label, class PiecewiseCurve1D& curve, float minValue, float maxValue);

    // Interactive curve editor helpers
    struct GraphInteraction {
        bool isMouseInGraph;
        ImVec2 mouseScreenPos;
        float mouseT;         // [-1, 1]
        float mouseValue;     // [minValue, maxValue]
    };

    struct CurveEditorState {
        int hoveredPointIndex = -1;
        int draggedPointIndex = -1;
        bool isDragging = false;
    };

    GraphInteraction GetGraphInteraction(struct ImVec2 graphMin, struct ImVec2 graphSize, float minValue, float maxValue);
    int FindHoveredPoint(class PiecewiseCurve1D const& curve, GraphInteraction const& interaction,
                         struct ImVec2 graphMin, struct ImVec2 graphSize, float minValue, float maxValue);

    std::unordered_map<std::string, CurveEditorState> m_curveEditorStates;
    void UpdateFromInput();
    void UpdateFromKeyBoard();
    void UpdateFromController();
    void UpdateEntities(float gameDeltaSeconds, float systemDeltaSeconds) const;
    void UpdateWorld(float gameDeltaSeconds);

    void RenderAttractMode() const;
    void RenderEntities() const;
    void RenderPlayerBasis() const;

    void SpawnPlayer();

    Camera*    m_screenCamera = nullptr;
    Player*    m_player       = nullptr;
    World*     m_world        = nullptr;
    Clock*     m_gameClock    = nullptr;
    eGameState m_gameState    = eGameState::GAME;

    // Assignment 7: UI Widgets
    std::shared_ptr<HotbarWidget> m_hotbarWidget = nullptr;
    std::shared_ptr<InventoryWidget> m_inventoryWidget = nullptr;  // Assignment 7-UI: Inventory screen

    // Debug display toggle
    bool m_showDebugInfo = true; // F3 toggleable debug info display (default visible)

    // Game restart request (F8 key)
    bool m_requestNewGame = false;  // Set to true when F8 is pressed, prevents use-after-delete crash

    // Assignment 5 Phase 10: Raycast visual feedback
    RaycastResult m_lastRaycastHit;  // Last raycast result for debug visualization
};
