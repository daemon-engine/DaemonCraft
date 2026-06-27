//----------------------------------------------------------------------------------------------------
// InventoryWidget.hpp
// Assignment 7-UI: Minecraft-style inventory screen with crafting interface
//----------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------
#include "Engine/Widget/IWidget.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Game/Gameplay/ItemStack.hpp"
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------------------------
class Player;
class Texture;
class BitmapFont;
class Recipe;

//----------------------------------------------------------------------------------------------------
// InventoryWidget - Minecraft-authentic inventory screen with 2×2 crafting grid
//
// Layout (176×166 px background):
// - Crafting Grid: 2×2 at (30, 17) from top-left
// - Crafting Output: 1 slot at (124, 35) from top-left
// - Main Inventory: 3×9 grid at (8, 84) from top-left
// - Hotbar: 1×9 row at (8, 142) from top-left
//
// Total: 40 slots (4 crafting input + 1 crafting output + 27 main + 9 hotbar - hotbar shared with Player)
//----------------------------------------------------------------------------------------------------
class InventoryWidget : public IWidget
{
public:
	explicit InventoryWidget(Player* player);
	~InventoryWidget() override;

	// IWidget overrides
	void Update() override;
	void Draw() const override;
	bool IsModal() const override;  // Returns true when inventory is visible (blocks world input)

	// Visibility control
	void Show();
	void Hide();
	void ToggleVisibility();
	bool IsInventoryVisible() const { return m_isInventoryVisible; }

private:
	// Layout calculation
	void CalculateLayout();

	// Rendering helpers
	void RenderDarkOverlay() const;
	void RenderBackground() const;
	void RenderSlots() const;
	void RenderItems() const;
	void RenderDebugSlotBoxes() const;  // DEBUG: Draw slot AABB2 wireframes

	// Item rendering helpers (A7-UI Phase 2)
	void RenderSlot(AABB2 const& bounds, ItemStack const& itemStack, bool isSelected = false, bool isHovered = false) const;
	void RenderInventoryGrid() const;  // Main inventory (27 slots)
	void RenderHotbarGrid() const;      // Hotbar (9 slots)
	void RenderCraftingGrid() const;    // Crafting input (4 slots)
	void RenderCraftingOutput() const;  // Crafting output (1 slot)

	// Mouse interaction helpers (A7-UI Phase 3)
	int         GetSlotIndexAtPosition(Vec2 const& mousePos) const;  // Returns 0-40 or -1 if no slot
	ItemStack*  GetSlotItemStack(int slotIndex);                      // Get ItemStack pointer for slot index
	ItemStack const* GetSlotItemStack(int slotIndex) const;           // Const version
	void        HandleLeftClick(int slotIndex);                       // Handle left mouse click on slot
	void        HandleRightClick(int slotIndex);                      // Handle right mouse click on slot
	void        HandleShiftLeftClick(int slotIndex);                  // Handle shift-left-click quick transfer
	void        HandleCraftingOutputClick();                          // Handle normal click on crafting output (craft 1)
	void        HandleShiftCraftingOutputClick();                     // Handle shift-click on crafting output (craft max)
	void        RenderCursorItem() const;                             // Render item following mouse cursor

	// UV calculation helper
	AABB2 GetUVsForSpriteCoords(IntVec2 const& spriteCoords) const;

	// Owner reference
	Player* m_player = nullptr;

	// Visibility state
	bool m_isInventoryVisible = false;

	// Layout properties (Minecraft-authentic dimensions scaled for visibility)
	static constexpr float UI_SCALE = 3.0f;      // Scale factor for modern displays (same as HotbarWidget)
	Vec2 m_screenCenter;                         // Screen center position
	Vec2 m_backgroundPosition;                   // Top-left corner of background
	Vec2 m_backgroundSize = Vec2(176.0f * UI_SCALE, 166.0f * UI_SCALE); // Scaled inventory background size

	// Slot layout (all positions in screen space)
	static constexpr int CRAFTING_INPUT_SLOTS = 4;
	static constexpr int CRAFTING_OUTPUT_SLOTS = 1;
	static constexpr int MAIN_INVENTORY_SLOTS = 27; // 3 rows × 9 columns
	static constexpr int HOTBAR_SLOTS = 9;
	static constexpr int TOTAL_SLOTS = CRAFTING_INPUT_SLOTS + CRAFTING_OUTPUT_SLOTS + MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS;

	// Slot bounds (AABB2 for easy hit testing)
	AABB2 m_craftingInputSlots[CRAFTING_INPUT_SLOTS];   // [0-3]: 2×2 crafting grid
	AABB2 m_craftingOutputSlot;                          // Crafting result slot
	AABB2 m_mainInventorySlots[MAIN_INVENTORY_SLOTS];   // [0-26]: 3×9 main inventory
	AABB2 m_hotbarSlots[HOTBAR_SLOTS];                  // [0-8]: Hotbar (shared with Player)

	// UI Textures
	Texture* m_backgroundTexture = nullptr;  // inventory_background.png (176×166)
	Texture* m_slotTexture = nullptr;        // Slot.png (18×18 empty slot)
	Texture* m_arrowTexture = nullptr;       // CraftingArrow.png (crafting progress arrow)
	Texture* m_itemSpriteSheet = nullptr;    // ItemSprites.png (16×16 grid)

	// UI Font
	BitmapFont* m_font = nullptr;            // SquirrelFixedFont for quantity text

	// Crafting grid storage (A7-UI Phase 2+)
	ItemStack m_craftingInputItems[CRAFTING_INPUT_SLOTS];  // 2×2 crafting grid items
	ItemStack m_craftingOutputItem;                         // Crafting result (auto-calculated)

	// Crafting recipe matching (A7-UI Phase 3)
	Recipe const* m_currentRecipe = nullptr;                // Currently matched recipe (nullptr if no match)
	void UpdateCraftingOutput();                            // Check crafting grid against all recipes

	// Cursor item (for drag-and-drop, A7-UI Phase 2)
	ItemStack m_cursorItem;
};
