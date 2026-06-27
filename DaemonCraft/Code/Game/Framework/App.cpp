//----------------------------------------------------------------------------------------------------
// App.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/App.hpp"
//----------------------------------------------------------------------------------------------------
#include "Game/Framework/GameCommon.hpp"
#include "Game/Framework/WorldGenConfig.hpp"
#include "Game/Gameplay/Game.hpp"
//----------------------------------------------------------------------------------------------------
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Game/EngineBuildPreferences.hpp"
#ifdef ENGINE_SCRIPTING_ENABLED
#include "Engine/Network/KADIWebSocketSubsystem.hpp"
#endif // ENGINE_SCRIPTING_ENABLED
#include "Engine/Platform/Window.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/UI/ImGuiSubsystem.hpp"
#include "Engine/Widget/WidgetSubsystem.hpp"


//----------------------------------------------------------------------------------------------------
App*  g_app  = nullptr;       // Created and owned by Main_Windows.cpp
Game* g_game = nullptr;       // Created and owned by the App

//----------------------------------------------------------------------------------------------------
STATIC bool App::m_isQuitting = false;

//----------------------------------------------------------------------------------------------------
App::App()
{
    GEngine::Get().Construct();
}

//----------------------------------------------------------------------------------------------------
App::~App()
{
    GEngine::Get().Destruct();
}

//----------------------------------------------------------------------------------------------------
void App::Startup()
{
    GEngine::Get().Startup();

    g_eventSystem->SubscribeEventCallbackFunction("OnCloseButtonClicked", OnCloseButtonClicked);
    g_eventSystem->SubscribeEventCallbackFunction("quit", OnCloseButtonClicked);

    // Initialize world generation config system (Assignment 4: Phase 5B.4)
    g_worldGenConfig = new WorldGenConfig();
    g_worldGenConfig->LoadFromXML("Data/GameConfig.xml");  // Load saved config if exists

    g_game = new Game();
}

//----------------------------------------------------------------------------------------------------
// All Destroy and ShutDown process should be reverse order of the StartUp
//
void App::Shutdown()
{
#ifdef ENGINE_SCRIPTING_ENABLED
    // Stage 0: Disconnect KADI WebSocket cleanly BEFORE any other cleanup
    // This sends a proper CLOSE frame (RFC 6455) so the broker knows we disconnected gracefully
    // Must happen while network stack is still alive
    if (g_kadiSubsystem != nullptr)
    {
        g_kadiSubsystem->Shutdown();
    }
#endif // ENGINE_SCRIPTING_ENABLED

    // CRITICAL FIX: Three-stage shutdown to handle both threading AND DirectX cleanup
    //
    // Problem 1: Worker threads accessing deleted chunks (crash on fast shutdown)
    // Problem 2: Chunks trying to release D3D11 buffers after Renderer destroyed (memory leaks)
    //
    // Solution: Stop workers → Delete chunks → Destroy Renderer

    // Stage 1: Stop worker threads BEFORE deleting chunks
    // This prevents workers from accessing freed chunk memory
    if (g_jobSystem != nullptr)
    {
        g_jobSystem->Shutdown();  // Waits for ALL workers to exit cleanly
    }

    // Stage 2: Delete game/world/chunks WHILE Renderer still alive
    // This allows chunk destructors to properly release DirectX buffers
    GAME_SAFE_RELEASE(g_game);

    // Stage 3: Cleanup config and shutdown rest of engine
    if (g_worldGenConfig != nullptr)
    {
        delete g_worldGenConfig;
        g_worldGenConfig = nullptr;
    }

    // Stage 4: Shutdown remaining engine systems (destroys Renderer last)
    GEngine::Get().Shutdown();
}

//----------------------------------------------------------------------------------------------------
void App::RunMainLoop()
{
    // Program main loop; keep running frames until it's time to quit
    while (!m_isQuitting)
    {
        // Sleep(16); // Temporary code to "slow down" our app to ~60Hz until we have proper frame timing in
        RunFrame();
    }
}

//----------------------------------------------------------------------------------------------------
// One "frame" of the game.  Generally: Input, Update, Render.  We call this 60+ times per second.
//
void App::RunFrame()
{
    BeginFrame();   // Engine pre-frame stuff
    Update();       // Game updates / moves / spawns / hurts / kills stuff
    Render();       // Game draws current state of things
    EndFrame();     // Engine post-frame stuff
}

//----------------------------------------------------------------------------------------------------
STATIC bool App::OnCloseButtonClicked(EventArgs& args)
{
    UNUSED(args)

    RequestQuit();

    return false;
}

//----------------------------------------------------------------------------------------------------
STATIC void App::RequestQuit()
{
    m_isQuitting = true;
}

//----------------------------------------------------------------------------------------------------
void App::BeginFrame() const
{
    g_eventSystem->BeginFrame();
    g_window->BeginFrame();
    g_renderer->BeginFrame();
    DebugRenderBeginFrame();
    g_devConsole->BeginFrame();
    g_input->BeginFrame();
    g_audio->BeginFrame();

#ifdef ENGINE_SCRIPTING_ENABLED
    // Assignment 7-AI: KADI subsystem frame processing
    if (g_kadiSubsystem != nullptr)
    {
        g_kadiSubsystem->BeginFrame();
    }
#endif // ENGINE_SCRIPTING_ENABLED

    // Assignment 7-UI: Widget subsystem frame start
    if (g_widgetSubsystem != nullptr)
    {
        g_widgetSubsystem->BeginFrame();
    }
}

//----------------------------------------------------------------------------------------------------
void App::Update()
{
    Clock::TickSystemClock();
    UpdateCursorMode();

    // Start ImGui frame via subsystem
    if (g_imgui)
    {
        g_imgui->Update();
    }

    g_game->Update();

    // CRITICAL: Check if game requested restart (F8 or Xbox START button)
    // Must check AFTER Update() completes to avoid use-after-delete crash
    if (g_game && g_game->RequestedNewGame())
    {
        DeleteAndCreateNewGame();
        // Do not continue processing this frame - new game will update next frame
        return;
    }
}

//----------------------------------------------------------------------------------------------------
// Some simple OpenGL example drawing code.
// This is the graphical equivalent of printing "Hello, world."
//
// Ultimately, this function (App::Render) will only call methods on Renderer (like Renderer::DrawVertexArray)
//	to draw things, never calling OpenGL (nor DirectX) functions directly.
//
void App::Render() const
{
    // Assignment 5 Stage 8: Use World's computed sky color for clear screen
    Rgba8 skyColor = Rgba8::WHITE;  // Default fallback (white sky if World unavailable)

    if (g_game != nullptr)  // Null-check g_game
    {
        World* world = g_game->GetWorld();
        if (world != nullptr)  // Null-check World pointer
        {
            skyColor = world->GetSkyColor();  // Get computed sky color with day/night + lightning
        }
    }

    g_renderer->ClearScreen(skyColor, Rgba8::BLACK);
    g_game->Render();

    AABB2 const box = AABB2(Vec2::ZERO, Vec2(1600.f, 30.f));

    g_devConsole->Render(box);

    // Render ImGui UI last via subsystem
    if (g_imgui)
    {
        g_imgui->Render();
    }
}

//----------------------------------------------------------------------------------------------------
void App::EndFrame() const
{
    // Assignment 7-UI: Widget subsystem frame end
    if (g_widgetSubsystem != nullptr)
    {
        g_widgetSubsystem->EndFrame();
    }

    g_eventSystem->EndFrame();
    g_window->EndFrame();
    g_renderer->EndFrame();
    DebugRenderEndFrame();
    g_devConsole->EndFrame();
    g_input->EndFrame();
    g_audio->EndFrame();
}

//----------------------------------------------------------------------------------------------------
void App::UpdateCursorMode()
{
    bool const doesWindowHasFocus   = GetActiveWindow() == g_window->GetWindowHandle();
    bool const shouldUsePointerMode = !doesWindowHasFocus || g_devConsole->IsOpen() || g_game->IsAttractMode() || g_game->IsInventoryOpen();

    if (shouldUsePointerMode == true)
    {
        g_input->SetCursorMode(eCursorMode::POINTER);
    }
    else
    {
        g_input->SetCursorMode(eCursorMode::FPS);
    }
}

//----------------------------------------------------------------------------------------------------
void App::DeleteAndCreateNewGame()
{
    GAME_SAFE_RELEASE(g_game);
    g_game = new Game();
}
