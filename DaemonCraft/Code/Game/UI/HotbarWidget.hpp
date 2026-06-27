//----------------------------------------------------------------------------------------------------
// HotbarWidget.hpp
//----------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------
#include "Engine/Widget/IWidget.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/IntVec2.hpp"
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------------------------
class Player;
class Texture;
class BitmapFont;

//----------------------------------------------------------------------------------------------------
// HotbarWidget - Minecraft-style hotbar UI for displaying 9 inventory slots
//
// Renders at bottom-center of screen with 182×22 pixel background.
// Shows selected slot highlight, item icons, and durability bars (future).
//----------------------------------------------------------------------------------------------------
class HotbarWidget : public IWidget
{
public:
    explicit HotbarWidget(Player* player);
    ~HotbarWidget() override;

    // IWidget overrides
    void Update() override;
    void Draw() const override;

private:
    // Layout calculation
    void CalculateLayout();

    // Rendering helpers
    void RenderBackground() const;
    void RenderSelection() const;
    void RenderItems() const;
    void RenderQuantityText() const;
    void RenderCrosshair() const;  // Assignment 7: Crosshair at screen center

    // UV calculation helper
    AABB2 GetUVsForSpriteCoords(IntVec2 const& spriteCoords) const;

    // Owner reference
    Player* m_player = nullptr;

    // Layout properties (Minecraft-authentic dimensions)
    Vec2 m_position;                // Bottom-center screen position
    Vec2 m_backgroundSize;          // 182×22 pixels (Minecraft hotbar size)
    Vec2 m_slotSize;                // 18×18 pixels per slot
    float m_slotSpacing = 20.0f;    // 20 pixels between slot centers

    // UI Textures
    Texture* m_backgroundTexture = nullptr;  // Hotbar.png
    Texture* m_selectionTexture = nullptr;   // Selection.png (24×24 highlight)
    Texture* m_itemSpriteSheet = nullptr;    // ItemSprites.png

    // UI Font
    BitmapFont* m_font = nullptr;            // SquirrelFixedFont for quantity text
};
