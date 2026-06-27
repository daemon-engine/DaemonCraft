//----------------------------------------------------------------------------------------------------
// InventoryWidget.cpp
// Assignment 7-UI: Minecraft-style inventory screen with crafting interface
//----------------------------------------------------------------------------------------------------
#include "Game/UI/InventoryWidget.hpp"
#include "Game/Gameplay/Player.hpp"
#include "Game/Gameplay/Inventory.hpp"
#include "Game/Gameplay/ItemStack.hpp"
#include "Game/Definition/ItemDefinition.hpp"
#include "Game/Definition/ItemRegistry.hpp"
#include "Game/Definition/Recipe.hpp"
#include "Game/Definition/RecipeRegistry.hpp"
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
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Input/InputCommon.hpp" // For KEYCODE_LEFT_MOUSE

//----------------------------------------------------------------------------------------------------
// Constructor: Load UI textures and calculate layout
//----------------------------------------------------------------------------------------------------
InventoryWidget::InventoryWidget(Player* player)
	: m_player(player)
{
	// Set widget properties
	SetName("InventoryWidget");
	SetVisible(false); // Start hidden
	SetTick(true);
	SetZOrder(200); // Render on top of HotbarWidget (z-order 100)

	// Load UI textures
	if (g_resourceSubsystem != nullptr)
	{
		m_backgroundTexture = g_resourceSubsystem->CreateOrGetTextureFromFile("Data/Images/GUI/inventory_background.png");
		m_slotTexture = g_resourceSubsystem->CreateOrGetTextureFromFile("Data/Images/GUI/Slot.png");
		m_arrowTexture = g_resourceSubsystem->CreateOrGetTextureFromFile("Data/Images/GUI/CraftingArrow.png");
		m_itemSpriteSheet = g_resourceSubsystem->CreateOrGetTextureFromFile("Data/Images/ItemSprites.png");
		m_font = g_resourceSubsystem->CreateOrGetBitmapFontFromFile("Data/Fonts/SquirrelFixedFont");
	}

	// Calculate initial layout
	CalculateLayout();
}

//----------------------------------------------------------------------------------------------------
// Destructor: Cleanup (textures managed by ResourceSubsystem)
//----------------------------------------------------------------------------------------------------
InventoryWidget::~InventoryWidget()
{
	// Textures are managed by ResourceSubsystem, no manual cleanup needed
}

//----------------------------------------------------------------------------------------------------
// Update: Recalculate layout if screen size changes, handle mouse input
//----------------------------------------------------------------------------------------------------
void InventoryWidget::Update()
{
	// Recalculate layout every frame to handle window resize
	CalculateLayout();

	// Handle mouse input only when inventory is visible
	if (!m_isInventoryVisible || !IsVisible())
	{
		return;
	}

	// Check for left mouse button click
	if (g_input->WasKeyJustPressed(KEYCODE_LEFT_MOUSE))
	{
		// Get mouse position (Windows coordinates: Y=0 at top)
		Vec2 mousePos = g_input->GetCursorClientPosition();

		// Convert to OpenGL coordinates (Y=0 at bottom)
		Vec2 screenDimensions = g_window->GetClientDimensions();
		mousePos.y = screenDimensions.y - mousePos.y;

		// Find which slot was clicked
		int slotIndex = GetSlotIndexAtPosition(mousePos);

		// Handle click if valid slot
		if (slotIndex != -1)
		{
			// Check if SHIFT is held for quick transfer
			if (g_input->IsKeyDown(KEYCODE_SHIFT))
			{
				HandleShiftLeftClick(slotIndex);
			}
			else
			{
				HandleLeftClick(slotIndex);
			}
		}
	}

	// Check for right mouse button click
	if (g_input->WasKeyJustPressed(KEYCODE_RIGHT_MOUSE))
	{
		// Get mouse position (Windows coordinates: Y=0 at top)
		Vec2 mousePos = g_input->GetCursorClientPosition();

		// Convert to OpenGL coordinates (Y=0 at bottom)
		Vec2 screenDimensions = g_window->GetClientDimensions();
		mousePos.y = screenDimensions.y - mousePos.y;

		// Find which slot was clicked
		int slotIndex = GetSlotIndexAtPosition(mousePos);

		// Handle click if valid slot
		if (slotIndex != -1)
		{
			HandleRightClick(slotIndex);
		}
	}
}

//----------------------------------------------------------------------------------------------------
// Draw: Render inventory screen when visible
//----------------------------------------------------------------------------------------------------
void InventoryWidget::Draw() const
{
	if (!m_isInventoryVisible || !IsVisible() || g_renderer == nullptr)
	{
		return;
	}

	RenderDarkOverlay();
	RenderBackground();
	// RenderSlots(); // DISABLED: Slots are already baked into inventory_background.png
	RenderItems();
	RenderDebugSlotBoxes(); // DEBUG: Draw slot AABB2 wireframes
	RenderCursorItem(); // Draw cursor item last (topmost layer)
}

//----------------------------------------------------------------------------------------------------
// Show: Display inventory screen
//----------------------------------------------------------------------------------------------------
void InventoryWidget::Show()
{
	m_isInventoryVisible = true;
	SetVisible(true);
}

//----------------------------------------------------------------------------------------------------
// Hide: Close inventory screen
//----------------------------------------------------------------------------------------------------
void InventoryWidget::Hide()
{
	m_isInventoryVisible = false;
	SetVisible(false);
}

//----------------------------------------------------------------------------------------------------
// IsModal: Returns true when inventory is visible (blocks world input)
//----------------------------------------------------------------------------------------------------
bool InventoryWidget::IsModal() const
{
	return m_isInventoryVisible;
}

//----------------------------------------------------------------------------------------------------
// ToggleVisibility: Toggle inventory screen on/off (bound to E key)
//----------------------------------------------------------------------------------------------------
void InventoryWidget::ToggleVisibility()
{
	if (m_isInventoryVisible)
	{
		Hide();
	}
	else
	{
		Show();
	}
}

//----------------------------------------------------------------------------------------------------
// CalculateLayout: Calculate all slot positions based on screen center and Minecraft layout
// Minecraft layout (from design.md):
// - Background: 176×166 px centered on screen
// - Crafting Input: 2×2 grid at (30, 17) from background top-left, 18×18 slots with 2px spacing
// - Crafting Output: 1 slot at (124, 35) from background top-left
// - Main Inventory: 3×9 grid at (8, 84) from background top-left, 18×18 slots with 2px spacing
// - Hotbar: 1×9 row at (8, 142) from background top-left, 18×18 slots with 2px spacing
//----------------------------------------------------------------------------------------------------
void InventoryWidget::CalculateLayout()
{
	if (g_window == nullptr)
	{
		return;
	}

	// Get screen dimensions
	Vec2 screenDimensions = g_window->GetClientDimensions();
	m_screenCenter = screenDimensions * 0.5f;

	// Calculate background position (centered on screen)
	m_backgroundPosition = m_screenCenter - (m_backgroundSize * 0.5f);

	// Slot size (Minecraft standard: 18×18 pixels) - keep unscaled for offset calculations
	constexpr float SLOT_SIZE_UNSCALED = 18.0f;
	constexpr float SLOT_SPACING_UNSCALED = 0.1f; // Gap between slots
	constexpr float SLOT_SIZE_SCALED = SLOT_SIZE_UNSCALED * UI_SCALE;

	// Helper lambda: Create AABB2 for slot at offset from background top-left
	// Applies UI_SCALE to the offset and size
	// Note: Flips Y coordinate because texture Y increases downward, but screen Y increases upward
	auto CreateSlot = [&](float offsetX, float offsetY) -> AABB2
	{
		// Flip Y: background height (166 * UI_SCALE) minus offsetY to convert from top-left to bottom-left origin
		constexpr float BACKGROUND_HEIGHT_UNSCALED = 166.0f;
		float flippedY = (BACKGROUND_HEIGHT_UNSCALED - offsetY - SLOT_SIZE_UNSCALED) * UI_SCALE;
		Vec2 slotMins = m_backgroundPosition + Vec2(offsetX * UI_SCALE, flippedY);
		Vec2 slotMaxs = slotMins + Vec2(SLOT_SIZE_SCALED, SLOT_SIZE_SCALED);
		return AABB2(slotMins, slotMaxs);
	};

	// 1. Crafting Input Grid (2×2 at offset 30, 17)
	constexpr float CRAFTING_START_X = 97.5f;
	constexpr float CRAFTING_START_Y = 17.0f;

	for (int row = 0; row < 2; ++row)
	{
		for (int col = 0; col < 2; ++col)
		{
			int slotIndex = row * 2 + col;
			float offsetX = CRAFTING_START_X + col * (SLOT_SIZE_UNSCALED + SLOT_SPACING_UNSCALED);
			float offsetY = CRAFTING_START_Y + row * (SLOT_SIZE_UNSCALED + SLOT_SPACING_UNSCALED);
			m_craftingInputSlots[slotIndex] = CreateSlot(offsetX, offsetY);
		}
	}

	// 2. Crafting Output Slot (1 slot at offset 124, 35)
	m_craftingOutputSlot = CreateSlot(152.5f, 27.5f);

	// 3. Main Inventory Grid (3×9 at offset 8, 84)
	constexpr float MAIN_START_X = 6.f;
	constexpr float MAIN_START_Y = 82.0f;

	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 9; ++col)
		{
			int slotIndex = row * 9 + col;
			float offsetX = MAIN_START_X + col * (SLOT_SIZE_UNSCALED + SLOT_SPACING_UNSCALED);
			float offsetY = MAIN_START_Y + row * (SLOT_SIZE_UNSCALED + SLOT_SPACING_UNSCALED);
			m_mainInventorySlots[slotIndex] = CreateSlot(offsetX, offsetY);
		}
	}

	// 4. Hotbar Row (1×9 at offset 8, 142)
	// Note: Adjusted X by +1px to account for slot border in texture
	constexpr float HOTBAR_START_X = 6.0f;
	constexpr float HOTBAR_START_Y = 140.0f;

	for (int col = 0; col < 9; ++col)
	{
		float offsetX = HOTBAR_START_X + col * (SLOT_SIZE_UNSCALED + SLOT_SPACING_UNSCALED);
		float offsetY = HOTBAR_START_Y;
		m_hotbarSlots[col] = CreateSlot(offsetX, offsetY);
	}
}

//----------------------------------------------------------------------------------------------------
// RenderDarkOverlay: Render semi-transparent dark overlay behind inventory screen
//----------------------------------------------------------------------------------------------------
void InventoryWidget::RenderDarkOverlay() const
{
	if (g_window == nullptr)
	{
		return;
	}

	// Get full screen dimensions
	Vec2 screenDimensions = g_window->GetClientDimensions();
	AABB2 fullScreen = AABB2(Vec2::ZERO, screenDimensions);

	// Build vertex array for dark overlay (50% transparent black)
	std::vector<Vertex_PCU> verts;
	Rgba8 overlayColor = Rgba8(0, 0, 0, 128); // 50% alpha
	AddVertsForAABB2D(verts, fullScreen, overlayColor);

	// Render overlay with no texture
	g_renderer->BindShader(g_resourceSubsystem->CreateOrGetShaderFromFile("Data/Shaders/Default"));
	g_renderer->BindTexture(nullptr);
	g_renderer->SetBlendMode(eBlendMode::ALPHA);
	g_renderer->DrawVertexArray(static_cast<int>(verts.size()), verts.data());
}

//----------------------------------------------------------------------------------------------------
// RenderBackground: Draw 176×166 pixel inventory background texture
//----------------------------------------------------------------------------------------------------
void InventoryWidget::RenderBackground() const
{
	if (m_backgroundTexture == nullptr)
	{
		return;
	}

	// Calculate background AABB (centered on screen)
	AABB2 backgroundBounds = AABB2(m_backgroundPosition, m_backgroundPosition + m_backgroundSize);

	// Build vertex array
	std::vector<Vertex_PCU> verts;
	AddVertsForAABB2D(verts, backgroundBounds, Rgba8::WHITE);

	// Render background
	g_renderer->BindTexture(m_backgroundTexture);
	g_renderer->SetBlendMode(eBlendMode::ALPHA);
	g_renderer->DrawVertexArray(static_cast<int>(verts.size()), verts.data());
}

//----------------------------------------------------------------------------------------------------
// RenderSlots: Draw empty slot textures for all 40 slots
//----------------------------------------------------------------------------------------------------
void InventoryWidget::RenderSlots() const
{
	if (m_slotTexture == nullptr)
	{
		return;
	}

	std::vector<Vertex_PCU> verts;

	// Render crafting input slots (2×2)
	for (int i = 0; i < CRAFTING_INPUT_SLOTS; ++i)
	{
		AddVertsForAABB2D(verts, m_craftingInputSlots[i], Rgba8::WHITE);
	}

	// Render crafting output slot
	AddVertsForAABB2D(verts, m_craftingOutputSlot, Rgba8::WHITE);

	// Render main inventory slots (3×9)
	for (int i = 0; i < MAIN_INVENTORY_SLOTS; ++i)
	{
		AddVertsForAABB2D(verts, m_mainInventorySlots[i], Rgba8::WHITE);
	}

	// Render hotbar slots (1×9)
	for (int i = 0; i < HOTBAR_SLOTS; ++i)
	{
		AddVertsForAABB2D(verts, m_hotbarSlots[i], Rgba8::WHITE);
	}

	// Render all slots with slot texture
	g_renderer->BindTexture(m_slotTexture);
	g_renderer->SetBlendMode(eBlendMode::ALPHA);
	g_renderer->DrawVertexArray(static_cast<int>(verts.size()), verts.data());
}

//----------------------------------------------------------------------------------------------------
// RenderItems: Draw item icons in all slots (A7-UI Phase 2 - Item rendering)
//----------------------------------------------------------------------------------------------------
void InventoryWidget::RenderItems() const
{
	if (m_player == nullptr)
	{
		return;
	}

	// Render items in all inventory sections
	RenderInventoryGrid();   // Main inventory (27 slots)
	RenderHotbarGrid();       // Hotbar (9 slots)
	RenderCraftingGrid();     // Crafting input (4 slots)
	RenderCraftingOutput();   // Crafting output (1 slot)
}

//----------------------------------------------------------------------------------------------------
// GetUVsForSpriteCoords: Convert sprite coordinates to UV bounds for 16×16 grid
//----------------------------------------------------------------------------------------------------
AABB2 InventoryWidget::GetUVsForSpriteCoords(IntVec2 const& spriteCoords) const
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
// RenderSlot: Draw single slot with item icon, quantity text, and durability bar
//----------------------------------------------------------------------------------------------------
void InventoryWidget::RenderSlot(AABB2 const& bounds, ItemStack const& itemStack, bool isSelected, bool isHovered) const
{
	// Skip rendering if slot is empty
	if (itemStack.IsEmpty())
	{
		return;
	}

	// Get item definition from registry
	sItemDefinition const* itemDef = ItemRegistry::GetInstance().Get(itemStack.itemID);
	if (itemDef == nullptr)
	{
		return; // Invalid item ID
	}

	// 1. Draw item icon (16×16 centered in 18×18 slot)
	if (m_itemSpriteSheet != nullptr)
	{
		// Calculate centered icon bounds (item is 16×16, slot is 18×18 * UI_SCALE)
		constexpr float ITEM_SIZE_UNSCALED = 16.0f;
		constexpr float SLOT_SIZE_UNSCALED = 18.0f;
		constexpr float CENTERING_OFFSET = (SLOT_SIZE_UNSCALED - ITEM_SIZE_UNSCALED) * 0.5f * UI_SCALE;

		Vec2 iconMins = bounds.m_mins + Vec2(CENTERING_OFFSET, CENTERING_OFFSET);
		Vec2 iconMaxs = iconMins + Vec2(ITEM_SIZE_UNSCALED * UI_SCALE, ITEM_SIZE_UNSCALED * UI_SCALE);
		AABB2 iconBounds(iconMins, iconMaxs);

		// Get UVs for item sprite
		AABB2 uvs = GetUVsForSpriteCoords(itemDef->GetSpriteCoords());

		// Build vertex array for item icon
		std::vector<Vertex_PCU> itemVerts;
		AddVertsForAABB2D(itemVerts, iconBounds, Rgba8::WHITE, uvs.m_mins, uvs.m_maxs);

		// Render item icon
		g_renderer->BindTexture(m_itemSpriteSheet);
		g_renderer->SetBlendMode(eBlendMode::ALPHA);
		g_renderer->DrawVertexArray(static_cast<int>(itemVerts.size()), itemVerts.data());
	}

	// 2. Draw quantity text (bottom-right corner, if quantity > 1)
	if (itemStack.quantity > 1 && m_font != nullptr)
	{
		String quantityText = Stringf("%d", itemStack.quantity);
		constexpr float FONT_SIZE = 8.0f * UI_SCALE;
		Vec2 textPosition = bounds.m_maxs - Vec2(2.0f * UI_SCALE, 2.0f * UI_SCALE); // Bottom-right with padding

		// Build vertex array for quantity text
		std::vector<Vertex_PCU> textVerts;
		float textWidth = m_font->GetTextWidth(FONT_SIZE, quantityText);
		Vec2 textMins = textPosition - Vec2(textWidth, FONT_SIZE);
		m_font->AddVertsForText2D(textVerts, quantityText, textMins, FONT_SIZE, Rgba8::WHITE);

		// Render quantity text
		g_renderer->BindTexture(&m_font->GetTexture());
		g_renderer->SetBlendMode(eBlendMode::ALPHA);
		g_renderer->DrawVertexArray(static_cast<int>(textVerts.size()), textVerts.data());
	}

	// 3. Draw durability bar (if tool with durability < max)
	if (itemDef->IsTool() && itemDef->GetMaxDurability() > 0 && itemStack.durability < itemDef->GetMaxDurability())
	{
		// Calculate durability percentage
		float durabilityPercent = static_cast<float>(itemStack.durability) / static_cast<float>(itemDef->GetMaxDurability());

		// Determine bar color based on percentage
		Rgba8 barColor;
		if (durabilityPercent > 0.6f)
		{
			barColor = Rgba8(0, 255, 0, 255); // Green
		}
		else if (durabilityPercent > 0.3f)
		{
			barColor = Rgba8(255, 255, 0, 255); // Yellow
		}
		else
		{
			barColor = Rgba8(255, 0, 0, 255); // Red
		}

		// Calculate bar bounds (14×2 pixels at top of slot)
		constexpr float BAR_WIDTH_UNSCALED = 14.0f;
		constexpr float BAR_HEIGHT_UNSCALED = 2.0f;
		constexpr float BAR_OFFSET_X = 2.0f * UI_SCALE; // 2px padding from left
		constexpr float BAR_OFFSET_Y = 2.0f * UI_SCALE; // 2px padding from top

		Vec2 barMins = bounds.m_mins + Vec2(BAR_OFFSET_X, bounds.GetDimensions().y - BAR_OFFSET_Y - BAR_HEIGHT_UNSCALED * UI_SCALE);
		float barWidth = BAR_WIDTH_UNSCALED * UI_SCALE * durabilityPercent;
		Vec2 barMaxs = barMins + Vec2(barWidth, BAR_HEIGHT_UNSCALED * UI_SCALE);
		AABB2 barBounds(barMins, barMaxs);

		// Render durability bar (no texture, solid color)
		std::vector<Vertex_PCU> barVerts;
		AddVertsForAABB2D(barVerts, barBounds, barColor);
		g_renderer->BindTexture(nullptr);
		g_renderer->SetBlendMode(eBlendMode::ALPHA);
		g_renderer->DrawVertexArray(static_cast<int>(barVerts.size()), barVerts.data());
	}

	// 4. Draw selection outline (if selected)
	if (isSelected)
	{
		// Draw white outline (2px border)
		constexpr float OUTLINE_THICKNESS = 2.0f;
		std::vector<Vertex_PCU> outlineVerts;

		// Top edge
		AABB2 topEdge(bounds.m_mins.x, bounds.m_maxs.y - OUTLINE_THICKNESS, bounds.m_maxs.x, bounds.m_maxs.y);
		AddVertsForAABB2D(outlineVerts, topEdge, Rgba8::WHITE);

		// Bottom edge
		AABB2 bottomEdge(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.x, bounds.m_mins.y + OUTLINE_THICKNESS);
		AddVertsForAABB2D(outlineVerts, bottomEdge, Rgba8::WHITE);

		// Left edge
		AABB2 leftEdge(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.x + OUTLINE_THICKNESS, bounds.m_maxs.y);
		AddVertsForAABB2D(outlineVerts, leftEdge, Rgba8::WHITE);

		// Right edge
		AABB2 rightEdge(bounds.m_maxs.x - OUTLINE_THICKNESS, bounds.m_mins.y, bounds.m_maxs.x, bounds.m_maxs.y);
		AddVertsForAABB2D(outlineVerts, rightEdge, Rgba8::WHITE);

		// Render outline
		g_renderer->BindTexture(nullptr);
		g_renderer->SetBlendMode(eBlendMode::ALPHA);
		g_renderer->DrawVertexArray(static_cast<int>(outlineVerts.size()), outlineVerts.data());
	}

	// 5. Draw hover effect (subtle white outline, thinner than selection)
	if (isHovered && !isSelected)  // Don't draw hover if already selected
	{
		// Draw subtle hover outline (1px border, semi-transparent)
		constexpr float HOVER_THICKNESS = 1.0f;
		Rgba8 hoverColor(255, 255, 255, 128);  // Semi-transparent white
		std::vector<Vertex_PCU> hoverVerts;

		// Top edge
		AABB2 topEdge(bounds.m_mins.x, bounds.m_maxs.y - HOVER_THICKNESS, bounds.m_maxs.x, bounds.m_maxs.y);
		AddVertsForAABB2D(hoverVerts, topEdge, hoverColor);

		// Bottom edge
		AABB2 bottomEdge(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.x, bounds.m_mins.y + HOVER_THICKNESS);
		AddVertsForAABB2D(hoverVerts, bottomEdge, hoverColor);

		// Left edge
		AABB2 leftEdge(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.x + HOVER_THICKNESS, bounds.m_maxs.y);
		AddVertsForAABB2D(hoverVerts, leftEdge, hoverColor);

		// Right edge
		AABB2 rightEdge(bounds.m_maxs.x - HOVER_THICKNESS, bounds.m_mins.y, bounds.m_maxs.x, bounds.m_maxs.y);
		AddVertsForAABB2D(hoverVerts, rightEdge, hoverColor);

		// Render hover outline
		g_renderer->BindTexture(nullptr);
		g_renderer->SetBlendMode(eBlendMode::ALPHA);
		g_renderer->DrawVertexArray(static_cast<int>(hoverVerts.size()), hoverVerts.data());
	}
}

//----------------------------------------------------------------------------------------------------
// RenderInventoryGrid: Render main inventory (27 slots, indices 0-26)
//----------------------------------------------------------------------------------------------------
void InventoryWidget::RenderInventoryGrid() const
{
	Inventory const& inventory = m_player->GetInventory();

	// Get mouse position for hover detection
	Vec2 mousePos = g_input->GetCursorClientPosition();
	Vec2 screenDimensions = g_window->GetClientDimensions();
	mousePos.y = screenDimensions.y - mousePos.y;  // Convert to OpenGL coordinates
	int hoveredSlot = GetSlotIndexAtPosition(mousePos);

	for (int i = 0; i < MAIN_INVENTORY_SLOTS; ++i)
	{
		ItemStack const& itemStack = inventory.GetMainSlot(i);
		bool isHovered = (hoveredSlot == i);
		RenderSlot(m_mainInventorySlots[i], itemStack, false, isHovered);
	}
}

//----------------------------------------------------------------------------------------------------
// RenderHotbarGrid: Render hotbar (9 slots, indices 27-35)
//----------------------------------------------------------------------------------------------------
void InventoryWidget::RenderHotbarGrid() const
{
	Inventory const& inventory = m_player->GetInventory();
	int selectedHotbarSlot = inventory.GetSelectedHotbarSlot();

	// Get mouse position for hover detection
	Vec2 mousePos = g_input->GetCursorClientPosition();
	Vec2 screenDimensions = g_window->GetClientDimensions();
	mousePos.y = screenDimensions.y - mousePos.y;  // Convert to OpenGL coordinates
	int hoveredSlot = GetSlotIndexAtPosition(mousePos);

	for (int i = 0; i < HOTBAR_SLOTS; ++i)
	{
		ItemStack const& itemStack = inventory.GetHotbarSlot(i);
		bool isSelected = (i == selectedHotbarSlot);
		bool isHovered = (hoveredSlot == (MAIN_INVENTORY_SLOTS + i));  // Hotbar slots are 27-35
		RenderSlot(m_hotbarSlots[i], itemStack, isSelected, isHovered);
	}
}

//----------------------------------------------------------------------------------------------------
// RenderCraftingGrid: Render crafting input (4 slots in 2×2 grid)
//----------------------------------------------------------------------------------------------------
void InventoryWidget::RenderCraftingGrid() const
{
	// Get mouse position for hover detection
	Vec2 mousePos = g_input->GetCursorClientPosition();
	Vec2 screenDimensions = g_window->GetClientDimensions();
	mousePos.y = screenDimensions.y - mousePos.y;  // Convert to OpenGL coordinates
	int hoveredSlot = GetSlotIndexAtPosition(mousePos);

	// Render all 4 crafting input slots
	for (int i = 0; i < CRAFTING_INPUT_SLOTS; ++i)
	{
		ItemStack const& itemStack = m_craftingInputItems[i];
		bool isHovered = (hoveredSlot == (MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + i));  // Crafting slots are 36-39
		RenderSlot(m_craftingInputSlots[i], itemStack, false, isHovered);
	}
}

//----------------------------------------------------------------------------------------------------
// RenderCraftingOutput: Render crafting output (1 slot)
//----------------------------------------------------------------------------------------------------
void InventoryWidget::RenderCraftingOutput() const
{
	// Get mouse position for hover detection
	Vec2 mousePos = g_input->GetCursorClientPosition();
	Vec2 screenDimensions = g_window->GetClientDimensions();
	mousePos.y = screenDimensions.y - mousePos.y;  // Convert to OpenGL coordinates
	int hoveredSlot = GetSlotIndexAtPosition(mousePos);
	constexpr int CRAFTING_OUTPUT_SLOT_INDEX = MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + CRAFTING_INPUT_SLOTS;  // 40

	// Render crafting output slot
	// Output is auto-calculated based on crafting pattern (A7-UI Phase 3: Recipe matching)
	if (m_currentRecipe != nullptr)
	{
		// Display recipe result
		ItemStack recipeResult;
		recipeResult.itemID = m_currentRecipe->GetOutputItemID();
		recipeResult.quantity = m_currentRecipe->GetOutputQuantity();
		recipeResult.durability = 0; // Crafted items start with full durability
		bool isHovered = (hoveredSlot == CRAFTING_OUTPUT_SLOT_INDEX);
		RenderSlot(m_craftingOutputSlot, recipeResult, false, isHovered);
	}
	else
	{
		// No recipe match, display empty slot (no hover needed for empty slot)
		ItemStack emptyStack;
		RenderSlot(m_craftingOutputSlot, emptyStack, false, false);
	}
}

//----------------------------------------------------------------------------------------------------
// GetSlotIndexAtPosition: Determine which slot (if any) the mouse is over
// Slot index mapping:
// - [0-26]: Main inventory (3×9 grid)
// - [27-35]: Hotbar (1×9 row)
// - [36-39]: Crafting input (2×2 grid)
// - [40]: Crafting output (1 slot)
// Returns -1 if mouse is outside all slots
//----------------------------------------------------------------------------------------------------
int InventoryWidget::GetSlotIndexAtPosition(Vec2 const& mousePos) const
{
	// Check main inventory slots (0-26)
	for (int i = 0; i < MAIN_INVENTORY_SLOTS; ++i)
	{
		if (m_mainInventorySlots[i].IsPointInside(mousePos))
		{
			return i;
		}
	}

	// Check hotbar slots (27-35)
	for (int i = 0; i < HOTBAR_SLOTS; ++i)
	{
		if (m_hotbarSlots[i].IsPointInside(mousePos))
		{
			return MAIN_INVENTORY_SLOTS + i; // Offset by 27
		}
	}

	// Check crafting input slots (36-39)
	for (int i = 0; i < CRAFTING_INPUT_SLOTS; ++i)
	{
		if (m_craftingInputSlots[i].IsPointInside(mousePos))
		{
			return MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + i; // Offset by 36
		}
	}

	// Check crafting output slot (40)
	if (m_craftingOutputSlot.IsPointInside(mousePos))
	{
		return MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + CRAFTING_INPUT_SLOTS; // Index 40
	}

	// Mouse is outside all slots
	return -1;
}

//----------------------------------------------------------------------------------------------------
// GetSlotItemStack: Get pointer to ItemStack at given slot index (non-const version)
// Slot index mapping:
// - [0-26]: Main inventory
// - [27-35]: Hotbar
// - [36-39]: Crafting input
// - [40]: Crafting output
// Returns nullptr for invalid indices
//----------------------------------------------------------------------------------------------------
ItemStack* InventoryWidget::GetSlotItemStack(int slotIndex)
{
	if (m_player == nullptr)
	{
		return nullptr;
	}

	// Main inventory slots (0-26)
	if (slotIndex >= 0 && slotIndex < MAIN_INVENTORY_SLOTS)
	{
		return &m_player->GetInventory().GetMainSlot(slotIndex);
	}

	// Hotbar slots (27-35)
	if (slotIndex >= MAIN_INVENTORY_SLOTS && slotIndex < MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS)
	{
		int hotbarIndex = slotIndex - MAIN_INVENTORY_SLOTS;
		return &m_player->GetInventory().GetHotbarSlot(hotbarIndex);
	}

	// Crafting input slots (36-39)
	if (slotIndex >= MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS && slotIndex < MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + CRAFTING_INPUT_SLOTS)
	{
		int craftingIndex = slotIndex - (MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS);
		return &m_craftingInputItems[craftingIndex];
	}

	// Crafting output slot (40)
	if (slotIndex == MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + CRAFTING_INPUT_SLOTS)
	{
		return &m_craftingOutputItem;
	}

	// Invalid slot index
	return nullptr;
}

//----------------------------------------------------------------------------------------------------
// GetSlotItemStack: Get pointer to ItemStack at given slot index (const version)
//----------------------------------------------------------------------------------------------------
ItemStack const* InventoryWidget::GetSlotItemStack(int slotIndex) const
{
	if (m_player == nullptr)
	{
		return nullptr;
	}

	// Main inventory slots (0-26)
	if (slotIndex >= 0 && slotIndex < MAIN_INVENTORY_SLOTS)
	{
		return &m_player->GetInventory().GetMainSlot(slotIndex);
	}

	// Hotbar slots (27-35)
	if (slotIndex >= MAIN_INVENTORY_SLOTS && slotIndex < MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS)
	{
		int hotbarIndex = slotIndex - MAIN_INVENTORY_SLOTS;
		return &m_player->GetInventory().GetHotbarSlot(hotbarIndex);
	}

	// Crafting input slots (36-39)
	if (slotIndex >= MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS && slotIndex < MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + CRAFTING_INPUT_SLOTS)
	{
		int craftingIndex = slotIndex - (MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS);
		return &m_craftingInputItems[craftingIndex];
	}

	// Crafting output slot (40)
	if (slotIndex == MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + CRAFTING_INPUT_SLOTS)
	{
		return &m_craftingOutputItem;
	}

	// Invalid slot index
	return nullptr;
}

//----------------------------------------------------------------------------------------------------
// HandleLeftClick: Handle left mouse button click on slot (Minecraft-authentic behavior)
// Cases:
// 1. Empty cursor + Empty slot → Nothing
// 2. Empty cursor + Filled slot → Pick up entire stack
// 3. Held item + Empty slot → Place entire stack
// 4. Held item + Filled slot (same item) → Merge stacks (respecting maxStackSize)
// 5. Held item + Filled slot (different item) → Swap stacks
//----------------------------------------------------------------------------------------------------
void InventoryWidget::HandleLeftClick(int slotIndex)
{
	// DEBUG: Log slot clicks
	DebuggerPrintf("HandleLeftClick: slotIndex=%d\n", slotIndex);

	// Check if this is a crafting slot (will need to update crafting output after modification)
	constexpr int CRAFTING_START = MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS; // 36
	constexpr int CRAFTING_END = CRAFTING_START + CRAFTING_INPUT_SLOTS - 1; // 39
	bool isCraftingSlot = (slotIndex >= CRAFTING_START && slotIndex <= CRAFTING_END);

	// Special case: Crafting output slot (A7-UI Phase 3: Recipe system)
	constexpr int CRAFTING_OUTPUT_SLOT = MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + CRAFTING_INPUT_SLOTS; // 40
	if (slotIndex == CRAFTING_OUTPUT_SLOT)
	{
		// Route to appropriate crafting handler based on SHIFT modifier
		if (g_input->IsKeyDown(KEYCODE_SHIFT))
		{
			HandleShiftCraftingOutputClick();
		}
		else
		{
			HandleCraftingOutputClick();
		}
		return;
	}

	// Get pointer to slot's ItemStack
	ItemStack* slotItemPtr = GetSlotItemStack(slotIndex);
	if (slotItemPtr == nullptr)
	{
		return; // Invalid slot index
	}

	ItemStack& slotItem = *slotItemPtr;

	// Case 1: Empty cursor + Empty slot → Nothing
	if (m_cursorItem.IsEmpty() && slotItem.IsEmpty())
	{
		if (isCraftingSlot) UpdateCraftingOutput();
		return;
	}

	// Case 2: Empty cursor + Filled slot → Pick up entire stack
	if (m_cursorItem.IsEmpty() && !slotItem.IsEmpty())
	{
		m_cursorItem = slotItem;
		slotItem.Clear();
		if (isCraftingSlot) UpdateCraftingOutput();
		return;
	}

	// Case 3: Held item + Empty slot → Place entire stack
	if (!m_cursorItem.IsEmpty() && slotItem.IsEmpty())
	{
		slotItem = m_cursorItem;
		m_cursorItem.Clear();
		if (isCraftingSlot) UpdateCraftingOutput();
		return;
	}

	// Case 4: Held item + Filled slot (same item) → Merge stacks
	if (!m_cursorItem.IsEmpty() && !slotItem.IsEmpty())
	{
		if (m_cursorItem.itemID == slotItem.itemID)
		{
			// Get max stack size for this item type
			sItemDefinition const* itemDef = ItemRegistry::GetInstance().Get(m_cursorItem.itemID);
			if (itemDef == nullptr)
			{
				if (isCraftingSlot) UpdateCraftingOutput();
				return; // Invalid item ID
			}

			uint8_t maxStack = itemDef->GetMaxStackSize();

			// Calculate how much we can transfer
			uint8_t space = maxStack - slotItem.quantity;
			uint8_t toTransfer = std::min(space, m_cursorItem.quantity);

			// Transfer items
			slotItem.quantity += toTransfer;
			m_cursorItem.quantity -= toTransfer;

			// Clear cursor if empty
			if (m_cursorItem.quantity == 0)
			{
				m_cursorItem.Clear();
			}

			if (isCraftingSlot) UpdateCraftingOutput();
			return;
		}

		// Case 5: Held item + Filled slot (different item) → Swap
		ItemStack temp = m_cursorItem;
		m_cursorItem = slotItem;
		slotItem = temp;
	}

	// Update crafting output if crafting grid was modified
	if (isCraftingSlot)
	{
		UpdateCraftingOutput();
	}
}

//----------------------------------------------------------------------------------------------------
// HandleRightClick: Handle right-click interaction on inventory slots (Minecraft half-stack/single-item mechanics)
//----------------------------------------------------------------------------------------------------
void InventoryWidget::HandleRightClick(int slotIndex)
{
	// Special case: Crafting output slot (will be handled in Phase 3 recipe system)
	constexpr int CRAFTING_OUTPUT_SLOT = MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + CRAFTING_INPUT_SLOTS; // 40
	if (slotIndex == CRAFTING_OUTPUT_SLOT)
	{
		// TODO: HandleCraftingOutputClick() in Phase 3
		return;
	}

	// Get pointer to slot's ItemStack
	ItemStack* slotItemPtr = GetSlotItemStack(slotIndex);
	if (slotItemPtr == nullptr)
	{
		return; // Invalid slot index
	}

	ItemStack& slotItem = *slotItemPtr;

	// Case 1: Empty cursor + Filled slot → Pick up half stack (rounded up)
	if (m_cursorItem.IsEmpty() && !slotItem.IsEmpty())
	{
		uint8_t half = (slotItem.quantity + 1) / 2;  // Round up
		m_cursorItem.itemID = slotItem.itemID;
		m_cursorItem.quantity = half;
		m_cursorItem.durability = slotItem.durability;

		slotItem.quantity -= half;
		if (slotItem.quantity == 0)
		{
			slotItem.Clear();
		}

		return;
	}

	// Case 2: Held item + Empty slot → Place 1 item
	if (!m_cursorItem.IsEmpty() && slotItem.IsEmpty())
	{
		slotItem.itemID = m_cursorItem.itemID;
		slotItem.quantity = 1;
		slotItem.durability = m_cursorItem.durability;

		m_cursorItem.quantity--;
		if (m_cursorItem.quantity == 0)
		{
			m_cursorItem.Clear();
		}

		return;
	}

	// Case 3: Held item + Filled slot (same item) → Add 1 item to slot
	if (!m_cursorItem.IsEmpty() && !slotItem.IsEmpty())
	{
		if (m_cursorItem.itemID == slotItem.itemID)
		{
			// Get max stack size for this item type
			sItemDefinition const* itemDef = ItemRegistry::GetInstance().Get(m_cursorItem.itemID);
			if (itemDef == nullptr)
			{
				return; // Invalid item ID
			}

			uint8_t maxStack = itemDef->GetMaxStackSize();
			if (slotItem.quantity < maxStack)
			{
				slotItem.quantity++;
				m_cursorItem.quantity--;

				if (m_cursorItem.quantity == 0)
				{
					m_cursorItem.Clear();
				}
			}
			return;
		}

		// Case 4: Different item → Swap (same as left-click)
		ItemStack temp = m_cursorItem;
		m_cursorItem = slotItem;
		slotItem = temp;
	}

	// Update crafting output if crafting grid was modified (slots 36-39)
	constexpr int CRAFTING_START = MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS; // 36
	constexpr int CRAFTING_END = CRAFTING_START + CRAFTING_INPUT_SLOTS - 1; // 39
	if (slotIndex >= CRAFTING_START && slotIndex <= CRAFTING_END)
	{
		UpdateCraftingOutput();
	}
}

//----------------------------------------------------------------------------------------------------
// HandleShiftLeftClick: Quick transfer items between inventory sections (hotbar ↔ main inventory)
//----------------------------------------------------------------------------------------------------
void InventoryWidget::HandleShiftLeftClick(int slotIndex)
{
	// Get pointer to slot's ItemStack
	ItemStack* slotItemPtr = GetSlotItemStack(slotIndex);
	if (slotItemPtr == nullptr || slotItemPtr->IsEmpty())
	{
		return; // Invalid slot or empty slot
	}

	ItemStack& slotItem = *slotItemPtr;

	// Determine source and destination ranges
	bool isHotbar = (slotIndex >= 27 && slotIndex <= 35);
	bool isCrafting = (slotIndex >= 36 && slotIndex <= 39);

	int destStart, destEnd;
	if (isHotbar || isCrafting)
	{
		// Hotbar/Crafting → Main Inventory (slots 0-26)
		destStart = 0;
		destEnd = 26;
	}
	else
	{
		// Main Inventory → Hotbar (slots 27-35)
		destStart = 27;
		destEnd = 35;
	}

	// Get item definition for max stack size
	sItemDefinition const* itemDef = ItemRegistry::GetInstance().Get(slotItem.itemID);
	if (itemDef == nullptr)
	{
		return; // Invalid item ID
	}

	uint8_t maxStack = itemDef->GetMaxStackSize();

	// Phase 1: Try to merge with existing stacks in destination range
	for (int i = destStart; i <= destEnd; i++)
	{
		ItemStack* destSlotPtr = GetSlotItemStack(i);
		if (destSlotPtr == nullptr)
		{
			continue;
		}

		ItemStack& destSlot = *destSlotPtr;
		if (!destSlot.IsEmpty() && destSlot.itemID == slotItem.itemID && destSlot.quantity < maxStack)
		{
			uint8_t space = maxStack - destSlot.quantity;
			uint8_t toTransfer = std::min(space, slotItem.quantity);

			destSlot.quantity += toTransfer;
			slotItem.quantity -= toTransfer;

			if (slotItem.quantity == 0)
			{
				slotItem.Clear();
				return;
			}
		}
	}

	// Phase 2: Try to find empty slot in destination range
	for (int i = destStart; i <= destEnd; i++)
	{
		ItemStack* destSlotPtr = GetSlotItemStack(i);
		if (destSlotPtr == nullptr)
		{
			continue;
		}

		ItemStack& destSlot = *destSlotPtr;
		if (destSlot.IsEmpty())
		{
			destSlot = slotItem;
			slotItem.Clear();
			return;
		}
	}

	// No space available, do nothing (item stays in original slot)

	// Update crafting output if crafting grid was modified (slots 36-39)
	constexpr int CRAFTING_START = MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS; // 36
	constexpr int CRAFTING_END = CRAFTING_START + CRAFTING_INPUT_SLOTS - 1; // 39
	if (slotIndex >= CRAFTING_START && slotIndex <= CRAFTING_END)
	{
		UpdateCraftingOutput();
	}
}

//----------------------------------------------------------------------------------------------------
// HandleCraftingOutputClick: Craft 1 item from crafting output (normal click)
//----------------------------------------------------------------------------------------------------
void InventoryWidget::HandleCraftingOutputClick()
{
	// Check if valid recipe exists
	if (m_currentRecipe == nullptr)
	{
		return; // No valid recipe
	}

	// Get recipe output
	uint16_t outputItemID = m_currentRecipe->GetOutputItemID();
	uint8_t outputQuantity = m_currentRecipe->GetOutputQuantity();

	// Check if cursor can hold output
	if (!m_cursorItem.IsEmpty() && m_cursorItem.itemID != outputItemID)
	{
		return; // Cannot merge different items
	}

	// Check if cursor has space (check max stack size)
	if (!m_cursorItem.IsEmpty())
	{
		sItemDefinition const* itemDef = ItemRegistry::GetInstance().Get(outputItemID);
		if (itemDef == nullptr)
		{
			return; // Invalid item ID
		}

		uint8_t maxStack = itemDef->GetMaxStackSize();
		if (m_cursorItem.quantity + outputQuantity > maxStack)
		{
			return; // Cursor would overflow max stack size
		}
	}

	// Consume ingredients from crafting grid (slots 36-39)
	for (int i = 0; i < CRAFTING_INPUT_SLOTS; i++)
	{
		ItemStack* slotItemPtr = GetSlotItemStack(MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + i);
		if (slotItemPtr != nullptr && !slotItemPtr->IsEmpty())
		{
			slotItemPtr->quantity--;
			if (slotItemPtr->quantity == 0)
			{
				slotItemPtr->Clear();
			}
		}
	}

	// Add output to cursor
	if (m_cursorItem.IsEmpty())
	{
		m_cursorItem.itemID = outputItemID;
		m_cursorItem.quantity = outputQuantity;
		m_cursorItem.durability = 0; // Crafted items start with full durability
	}
	else
	{
		m_cursorItem.quantity += outputQuantity;
	}

	// Update crafting output (re-check recipe with remaining items)
	UpdateCraftingOutput();
}

//----------------------------------------------------------------------------------------------------
// HandleShiftCraftingOutputClick: Craft as many items as possible (shift-click)
//----------------------------------------------------------------------------------------------------
void InventoryWidget::HandleShiftCraftingOutputClick()
{
	// Check if valid recipe exists
	if (m_currentRecipe == nullptr)
	{
		return; // No valid recipe
	}

	// Craft as many as possible
	while (m_currentRecipe != nullptr)
	{
		// Get recipe output
		uint16_t outputItemID = m_currentRecipe->GetOutputItemID();
		uint8_t outputQuantity = m_currentRecipe->GetOutputQuantity();

		// Try to add output to player inventory
		if (m_player == nullptr)
		{
			break; // No player reference
		}

		bool addedToInventory = m_player->GetInventory().AddItem(outputItemID, outputQuantity);
		if (!addedToInventory)
		{
			break; // Inventory full
		}

		// Consume ingredients from crafting grid (slots 36-39)
		for (int i = 0; i < CRAFTING_INPUT_SLOTS; i++)
		{
			ItemStack* slotItemPtr = GetSlotItemStack(MAIN_INVENTORY_SLOTS + HOTBAR_SLOTS + i);
			if (slotItemPtr != nullptr && !slotItemPtr->IsEmpty())
			{
				slotItemPtr->quantity--;
				if (slotItemPtr->quantity == 0)
				{
					slotItemPtr->Clear();
				}
			}
		}

		// Re-check recipe with remaining items
		UpdateCraftingOutput();
	}
}

//----------------------------------------------------------------------------------------------------
// UpdateCraftingOutput: Check crafting grid against all recipes and update m_currentRecipe
//----------------------------------------------------------------------------------------------------
void InventoryWidget::UpdateCraftingOutput()
{
	// Get crafting grid items (slots 36-39)
	std::array<uint16_t, 4> pattern;
	for (int i = 0; i < 4; i++)
	{
		ItemStack const* slotPtr = GetSlotItemStack(36 + i);
		if (slotPtr != nullptr)
		{
			pattern[i] = slotPtr->itemID;
		}
		else
		{
			pattern[i] = 0; // Empty slot
		}
	}

	// Check all recipes
	RecipeRegistry& recipeReg = RecipeRegistry::GetInstance();
	m_currentRecipe = nullptr;

	// DEBUG: Log recipe count and pattern
	std::vector<Recipe*> allRecipes = recipeReg.GetAll();
	DebuggerPrintf("UpdateCraftingOutput: %d recipes loaded, pattern = [%d, %d, %d, %d]\n",
		(int)allRecipes.size(), pattern[0], pattern[1], pattern[2], pattern[3]);

	for (Recipe* recipe : allRecipes)
	{
		if (recipe != nullptr && recipe->Matches(pattern))
		{
			m_currentRecipe = recipe;
			DebuggerPrintf("  -> Recipe MATCHED: ID=%d, Output=%d x%d\n",
				recipe->GetRecipeID(), recipe->GetOutputItemID(), recipe->GetOutputQuantity());
			break; // First match wins
		}
	}

	if (m_currentRecipe == nullptr)
	{
		DebuggerPrintf("  -> No recipe matched\n");
	}
}

//----------------------------------------------------------------------------------------------------
// RenderCursorItem: Draw item icon centered at mouse position (topmost layer)
//----------------------------------------------------------------------------------------------------
void InventoryWidget::RenderCursorItem() const
{
	// Don't render if cursor is empty
	if (m_cursorItem.IsEmpty())
	{
		return;
	}

	// Get item definition
	sItemDefinition const* itemDef = ItemRegistry::GetInstance().Get(m_cursorItem.itemID);
	if (itemDef == nullptr || m_itemSpriteSheet == nullptr)
	{
		return; // Invalid item ID or missing sprite sheet
	}

	// Get mouse position (Windows coordinates: Y=0 at top)
	Vec2 mousePos = g_input->GetCursorClientPosition();

	// Convert to OpenGL coordinates (Y=0 at bottom)
	Vec2 screenDimensions = g_window->GetClientDimensions();
	mousePos.y = screenDimensions.y - mousePos.y;

	// Calculate icon bounds (centered at mouse, 16×16 scaled)
	constexpr float ITEM_SIZE_UNSCALED = 16.0f;
	float iconSize = ITEM_SIZE_UNSCALED * UI_SCALE;
	Vec2 iconMins = mousePos - Vec2(iconSize * 0.5f, iconSize * 0.5f);
	Vec2 iconMaxs = iconMins + Vec2(iconSize, iconSize);
	AABB2 iconBounds(iconMins, iconMaxs);

	// Get UVs for item sprite
	AABB2 uvs = GetUVsForSpriteCoords(itemDef->GetSpriteCoords());

	// Build vertex array for item icon
	std::vector<Vertex_PCU> itemVerts;
	AddVertsForAABB2D(itemVerts, iconBounds, Rgba8::WHITE, uvs.m_mins, uvs.m_maxs);

	// Render item icon
	g_renderer->BindTexture(m_itemSpriteSheet);
	g_renderer->SetBlendMode(eBlendMode::ALPHA);
	g_renderer->DrawVertexArray(static_cast<int>(itemVerts.size()), itemVerts.data());

	// Draw quantity text (bottom-right corner, if quantity > 1)
	if (m_cursorItem.quantity > 1 && m_font != nullptr)
	{
		String quantityText = Stringf("%d", m_cursorItem.quantity);
		constexpr float FONT_SIZE = 8.0f * UI_SCALE;
		Vec2 textPosition = iconMaxs - Vec2(2.0f * UI_SCALE, 2.0f * UI_SCALE); // Bottom-right with padding

		// Build vertex array for quantity text
		std::vector<Vertex_PCU> textVerts;
		float textWidth = m_font->GetTextWidth(FONT_SIZE, quantityText);
		Vec2 textMins = textPosition - Vec2(textWidth, FONT_SIZE);
		m_font->AddVertsForText2D(textVerts, quantityText, textMins, FONT_SIZE, Rgba8::WHITE);

		// Render quantity text
		g_renderer->BindTexture(&m_font->GetTexture());
		g_renderer->SetBlendMode(eBlendMode::ALPHA);
		g_renderer->DrawVertexArray(static_cast<int>(textVerts.size()), textVerts.data());
	}
}

//----------------------------------------------------------------------------------------------------
// RenderDebugSlotBoxes: Draw wireframe boxes around all slot AABBs for debugging
//----------------------------------------------------------------------------------------------------
void InventoryWidget::RenderDebugSlotBoxes() const
{
	if (g_renderer == nullptr)
	{
		return;
	}

	Rgba8 debugColor = Rgba8(255, 0, 255, 255); // Bright magenta, fully opaque
	float lineThickness = 2.0f;

	// Helper lambda: Draw wireframe box for an AABB2 using thin filled quads for each edge
	auto DrawWireframeBox = [&](AABB2 const& box)
	{
		std::vector<Vertex_PCU> lineVerts;

		// Bottom edge (horizontal line)
		AABB2 bottomEdge = AABB2(
			box.m_mins.x,
			box.m_mins.y,
			box.m_maxs.x,
			box.m_mins.y + lineThickness
		);
		AddVertsForAABB2D(lineVerts, bottomEdge, debugColor);

		// Top edge (horizontal line)
		AABB2 topEdge = AABB2(
			box.m_mins.x,
			box.m_maxs.y - lineThickness,
			box.m_maxs.x,
			box.m_maxs.y
		);
		AddVertsForAABB2D(lineVerts, topEdge, debugColor);

		// Left edge (vertical line)
		AABB2 leftEdge = AABB2(
			box.m_mins.x,
			box.m_mins.y,
			box.m_mins.x + lineThickness,
			box.m_maxs.y
		);
		AddVertsForAABB2D(lineVerts, leftEdge, debugColor);

		// Right edge (vertical line)
		AABB2 rightEdge = AABB2(
			box.m_maxs.x - lineThickness,
			box.m_mins.y,
			box.m_maxs.x,
			box.m_maxs.y
		);
		AddVertsForAABB2D(lineVerts, rightEdge, debugColor);

		// Render this box's edges
		g_renderer->BindTexture(nullptr);
		g_renderer->SetBlendMode(eBlendMode::ALPHA);
		g_renderer->DrawVertexArray(static_cast<int>(lineVerts.size()), lineVerts.data());
	};

	// Draw all main inventory slots (0-26)
	for (int i = 0; i < MAIN_INVENTORY_SLOTS; ++i)
	{
		DrawWireframeBox(m_mainInventorySlots[i]);
	}

	// Draw all hotbar slots (27-35)
	for (int i = 0; i < HOTBAR_SLOTS; ++i)
	{
		DrawWireframeBox(m_hotbarSlots[i]);
	}

	// Draw crafting input slots (36-39)
	for (int i = 0; i < CRAFTING_INPUT_SLOTS; ++i)
	{
		DrawWireframeBox(m_craftingInputSlots[i]);
	}

	// Draw crafting output slot (40)
	DrawWireframeBox(m_craftingOutputSlot);
}
