//----------------------------------------------------------------------------------------------------
// HotbarWidget.cpp
//----------------------------------------------------------------------------------------------------
#include "Game/UI/HotbarWidget.hpp"
#include "Game/Gameplay/Player.hpp"
#include "Game/Gameplay/Inventory.hpp"
#include "Game/Gameplay/ItemStack.hpp"
#include "Game/Definition/ItemDefinition.hpp"
#include "Game/Definition/ItemRegistry.hpp"
//----------------------------------------------------------------------------------------------------
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Platform/Window.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Renderer/VertexUtils.hpp"
#include "Engine/Core/StringUtils.hpp"

//----------------------------------------------------------------------------------------------------
// Constructor: Load UI textures and initialize layout
//----------------------------------------------------------------------------------------------------
HotbarWidget::HotbarWidget(Player* player)
    : m_player(player)
{
    // Set widget properties
    SetName("HotbarWidget");
    SetVisible(true);
    SetTick(true);
    SetZOrder(100); // Render on top of world

    // Initialize Minecraft-authentic dimensions (scaled 3x for visibility)
    constexpr float SCALE = 3.0f;  // Scale factor for better visibility on modern displays
    m_backgroundSize = Vec2(182.0f * SCALE, 22.0f * SCALE);  // Minecraft hotbar background size
    m_slotSize = Vec2(18.0f * SCALE, 18.0f * SCALE);          // Minecraft slot size
    m_slotSpacing = 20.0f * SCALE;                            // 20 pixels between slot centers

    // Load UI textures and font
    if (g_resourceSubsystem != nullptr)
    {
        m_backgroundTexture = g_resourceSubsystem->CreateOrGetTextureFromFile("Data/Images/GUI/hotbar_background.png");
        m_selectionTexture = g_resourceSubsystem->CreateOrGetTextureFromFile("Data/Images/GUI/hotbar_selector.png");
        m_itemSpriteSheet = g_resourceSubsystem->CreateOrGetTextureFromFile("Data/Images/ItemSprites.png");
        m_font = g_resourceSubsystem->CreateOrGetBitmapFontFromFile("Data/Fonts/SquirrelFixedFont");
    }

    // Calculate initial layout
    CalculateLayout();
}

//----------------------------------------------------------------------------------------------------
// Destructor: Cleanup (textures managed by ResourceSubsystem)
//----------------------------------------------------------------------------------------------------
HotbarWidget::~HotbarWidget()
{
    // Textures are managed by ResourceSubsystem, no manual cleanup needed
}

//----------------------------------------------------------------------------------------------------
// Update: Recalculate layout if screen size changes
//----------------------------------------------------------------------------------------------------
void HotbarWidget::Update()
{
    // Recalculate layout every frame to handle window resize
    CalculateLayout();
}

//----------------------------------------------------------------------------------------------------
// Draw: Render hotbar background, selection highlight, and items
//----------------------------------------------------------------------------------------------------
void HotbarWidget::Draw() const
{
    if (!IsVisible() || g_renderer == nullptr)
    {
        return;
    }

    RenderBackground();
    RenderSelection();
    RenderItems();
    RenderQuantityText();
    RenderCrosshair();  // Assignment 7: Render crosshair last (always on top)
}

//----------------------------------------------------------------------------------------------------
// CalculateLayout: Calculate hotbar position at bottom-center of screen
//----------------------------------------------------------------------------------------------------
void HotbarWidget::CalculateLayout()
{
    if (g_window == nullptr)
    {
        return;
    }

    // Get screen dimensions
    Vec2 screenDimensions = g_window->GetClientDimensions();

    // Calculate bottom-center position
    float centerX = screenDimensions.x * 0.5f;
    float bottomY = 0.0f + 20.0f;  // 20 pixels from bottom

    m_position = Vec2(centerX, bottomY);
}

//----------------------------------------------------------------------------------------------------
// RenderBackground: Draw 182×22 pixel hotbar background texture
//----------------------------------------------------------------------------------------------------
void HotbarWidget::RenderBackground() const
{
    if (m_backgroundTexture == nullptr)
    {
        return;
    }

    // Calculate background AABB (centered at m_position)
    AABB2 backgroundBounds = AABB2(
        m_position.x - m_backgroundSize.x * 0.5f,
        m_position.y,
        m_position.x + m_backgroundSize.x * 0.5f,
        m_position.y + m_backgroundSize.y
    );

    // Build vertex array
    std::vector<Vertex_PCU> verts;
    AddVertsForAABB2D(verts, backgroundBounds, Rgba8::WHITE);

    // Render background
    g_renderer->BindTexture(m_backgroundTexture);
    g_renderer->DrawVertexArray(static_cast<int>(verts.size()), verts.data());
}

//----------------------------------------------------------------------------------------------------
// RenderSelection: Draw highlight around selected hotbar slot
//----------------------------------------------------------------------------------------------------
void HotbarWidget::RenderSelection() const
{
    if (m_selectionTexture == nullptr || m_player == nullptr)
    {
        return;
    }

    // Get selected slot index (0-8)
    int selectedSlot = m_player->GetInventory().GetSelectedHotbarSlot();

    // Calculate selection position (24×24 highlight centered on 18×18 slot, scaled)
    constexpr float SCALE = 3.0f;
    float startX = m_position.x - (m_backgroundSize.x * 0.5f) + (3.0f * SCALE);  // 3px left padding, scaled
    float slotCenterX = startX + (selectedSlot * m_slotSpacing) + (m_slotSize.x * 0.5f);
    float slotCenterY = m_position.y + (m_backgroundSize.y * 0.5f);

    Vec2 selectionSize = Vec2(24.0f * SCALE, 24.0f * SCALE);  // Selection highlight is 24×24, scaled
    AABB2 selectionBounds = AABB2(
        slotCenterX - selectionSize.x * 0.5f,
        slotCenterY - selectionSize.y * 0.5f,
        slotCenterX + selectionSize.x * 0.5f,
        slotCenterY + selectionSize.y * 0.5f
    );

    // Build vertex array
    std::vector<Vertex_PCU> verts;
    AddVertsForAABB2D(verts, selectionBounds, Rgba8::WHITE);

    // Render selection highlight
    g_renderer->BindTexture(m_selectionTexture);
    g_renderer->DrawVertexArray(static_cast<int>(verts.size()), verts.data());
}

//----------------------------------------------------------------------------------------------------
// RenderItems: Draw item icons in hotbar slots
//----------------------------------------------------------------------------------------------------
void HotbarWidget::RenderItems() const
{
    if (m_itemSpriteSheet == nullptr || m_player == nullptr)
    {
        return;
    }

    // Get player inventory
    Inventory const& inventory = m_player->GetInventory();

    // Calculate starting position for first slot (scaled padding)
    constexpr float SCALE = 3.0f;
    float startX = m_position.x - (m_backgroundSize.x * 0.5f) + (3.0f * SCALE);  // 3px left padding, scaled
    float slotCenterY = m_position.y + (m_backgroundSize.y * 0.5f);

    // Render each hotbar slot (0-8)
    for (int slotIndex = 0; slotIndex < 9; ++slotIndex)
    {
        // Get ItemStack from hotbar slot
        ItemStack const& itemStack = inventory.GetHotbarSlot(slotIndex);

        // Skip empty slots
        if (itemStack.IsEmpty())
        {
            continue;
        }

        // Get ItemDefinition from registry
        sItemDefinition const* itemDef = ItemRegistry::GetInstance().Get(itemStack.itemID);
        if (itemDef == nullptr)
        {
            continue; // Invalid item ID
        }

        // Get sprite coordinates from ItemDefinition
        IntVec2 spriteCoords = itemDef->GetSpriteCoords();

        // Calculate sprite UVs for 16×16 grid
        AABB2 spriteUVs = GetUVsForSpriteCoords(spriteCoords);

        // Calculate slot center position
        float slotCenterX = startX + (slotIndex * m_slotSpacing) + (m_slotSize.x * 0.5f);

        // Calculate item icon bounds (16×16 icon centered in 18×18 slot = 1px padding, scaled)
        constexpr float SCALE = 3.0f;
        Vec2 iconSize = Vec2(16.0f * SCALE, 16.0f * SCALE);
        AABB2 iconBounds = AABB2(
            slotCenterX - iconSize.x * 0.5f,
            slotCenterY - iconSize.y * 0.5f,
            slotCenterX + iconSize.x * 0.5f,
            slotCenterY + iconSize.y * 0.5f
        );

        // Build vertex array with custom UVs
        std::vector<Vertex_PCU> verts;
        AddVertsForAABB2D(verts, iconBounds, Rgba8::WHITE, spriteUVs.m_mins, spriteUVs.m_maxs);

        // Render item icon
        g_renderer->BindTexture(m_itemSpriteSheet);
        g_renderer->DrawVertexArray(static_cast<int>(verts.size()), verts.data());

        // TODO: Render durability bar if tool with durability (future task)
    }
}

//----------------------------------------------------------------------------------------------------
// GetUVsForSpriteCoords: Convert sprite coordinates to UV bounds for 16×16 grid
//----------------------------------------------------------------------------------------------------
AABB2 HotbarWidget::GetUVsForSpriteCoords(IntVec2 const& spriteCoords) const
{
    // 16×16 grid layout (256×256 texture with 16×16 sprites)
    constexpr int SPRITE_GRID_SIZE = 16;
    constexpr float UV_STEP = 1.0f / static_cast<float>(SPRITE_GRID_SIZE);

    // Calculate UV mins and maxs
    float uMin = static_cast<float>(spriteCoords.x) * UV_STEP;
    float vMin = static_cast<float>(spriteCoords.y) * UV_STEP;
    float uMax = uMin + UV_STEP;
    float vMax = vMin + UV_STEP;

    return AABB2(uMin, vMin, uMax, vMax);
}

//----------------------------------------------------------------------------------------------------
// RenderQuantityText: Draw quantity numbers in bottom-right corner of slots (Minecraft style)
//----------------------------------------------------------------------------------------------------
void HotbarWidget::RenderQuantityText() const
{
    if (m_font == nullptr || m_player == nullptr)
    {
        return;
    }

    // Get player inventory
    Inventory const& inventory = m_player->GetInventory();

    // Calculate starting position for first slot (scaled padding)
    constexpr float SCALE = 3.0f;
    float startX = m_position.x - (m_backgroundSize.x * 0.5f) + (3.0f * SCALE);  // 3px left padding, scaled
    float slotCenterY = m_position.y + (m_backgroundSize.y * 0.5f);

    // Render quantity text for each hotbar slot (0-8)
    for (int slotIndex = 0; slotIndex < 9; ++slotIndex)
    {
        // Get ItemStack from hotbar slot
        ItemStack const& itemStack = inventory.GetHotbarSlot(slotIndex);

        // Skip empty slots or stacks with quantity <= 1
        if (itemStack.IsEmpty() || itemStack.quantity <= 1)
        {
            continue;
        }

        // Calculate slot bottom-right position
        float slotCenterX = startX + (slotIndex * m_slotSpacing) + (m_slotSize.x * 0.5f);

        // Calculate text position: bottom-right of slot with 2px offset (scaled)
        constexpr float SCALE = 3.0f;
        Vec2 textBottomRight = Vec2(
            slotCenterX + (m_slotSize.x * 0.5f) - (2.0f * SCALE),  // Right edge - 2px, scaled
            slotCenterY - (m_slotSize.y * 0.5f)                    // Bottom edge
        );

        // Convert quantity to string
        String quantityText = Stringf("%u", itemStack.quantity);

        // Calculate text width to right-align it (scaled text size)
        float cellHeight = 8.0f * SCALE;  // 8pt text, scaled
        float textWidth = m_font->GetTextWidth(cellHeight, quantityText);

        // Adjust position to right-align (textBottomRight is the right edge)
        Vec2 textMins = Vec2(textBottomRight.x - textWidth, textBottomRight.y);

        // Build vertex array for text
        std::vector<Vertex_PCU> verts;
        m_font->AddVertsForText2D(verts, quantityText, textMins, cellHeight, Rgba8::WHITE);

        // Render text with font texture
        g_renderer->BindTexture(&m_font->GetTexture());
        g_renderer->DrawVertexArray(static_cast<int>(verts.size()), verts.data());
    }
}

//----------------------------------------------------------------------------------------------------
// RenderCrosshair: Draw simple white crosshair at screen center for aiming
// Assignment 7: Essential for mining and placement targeting
//----------------------------------------------------------------------------------------------------
void HotbarWidget::RenderCrosshair() const
{
    if (g_window == nullptr || g_renderer == nullptr)
    {
        return;
    }

    // Get screen center position
    Vec2 screenDimensions = g_window->GetClientDimensions();
    Vec2 screenCenter = screenDimensions * 0.5f;

    // Crosshair dimensions (Minecraft-style)
    constexpr float CROSSHAIR_SIZE = 5.0f;   // 5 pixels from center
    constexpr float LINE_THICKNESS = 2.0f;   // 2 pixels wide

    // Build vertex array for crosshair (two perpendicular lines)
    std::vector<Vertex_PCU> verts;
    Rgba8 crosshairColor = Rgba8::WHITE;

    // Horizontal line: center + Vec2(-5, 0) to center + Vec2(5, 0)
    Vec2 horizontalStart = screenCenter + Vec2(-CROSSHAIR_SIZE, 0.0f);
    Vec2 horizontalEnd = screenCenter + Vec2(CROSSHAIR_SIZE, 0.0f);
    AddVertsForLineSegment2D(verts, horizontalStart, horizontalEnd, LINE_THICKNESS, false, crosshairColor);

    // Vertical line: center + Vec2(0, -5) to center + Vec2(0, 5)
    Vec2 verticalStart = screenCenter + Vec2(0.0f, -CROSSHAIR_SIZE);
    Vec2 verticalEnd = screenCenter + Vec2(0.0f, CROSSHAIR_SIZE);
    AddVertsForLineSegment2D(verts, verticalStart, verticalEnd, LINE_THICKNESS, false, crosshairColor);

    // Render crosshair with default shader (no texture)
    g_renderer->BindShader(g_resourceSubsystem->CreateOrGetShaderFromFile("Data/Shaders/Default"));
    g_renderer->BindTexture(nullptr);
    g_renderer->DrawVertexArray(static_cast<int>(verts.size()), verts.data());
}
