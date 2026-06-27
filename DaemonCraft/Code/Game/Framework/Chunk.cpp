//----------------------------------------------------------------------------------------------------
// Chunk.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/Chunk.hpp"

#include <filesystem>
#include <unordered_map>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/VertexUtils.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Game/Definition/BlockDefinition.hpp"
#include "Game/Framework/GameCommon.hpp"
#include "Game/Framework/WorldGenConfig.hpp"  // For g_worldGenConfig (Assignment 4: Phase 5B.4)
#include "Game/Framework/BlockIterator.hpp"
#include "Game/Gameplay/Game.hpp"  // For g_game and visualization mode access
#include "Game/Gameplay/World.hpp"  // Assignment 5 Phase 6: For OnActivate() method
#include "ThirdParty/Noise/RawNoise.hpp"
#include "ThirdParty/Noise/SmoothNoise.hpp"

//----------------------------------------------------------------------------------------------------
// Block Type Constants - Must match BlockSpriteSheet_BlockDefinitions.xml exactly (0-indexed)
// Phase 1, Task 1.1: Updated to match professor's new XML (Assignment 4)
const uint8_t BLOCK_AIR                 = 0;   // Air
const uint8_t BLOCK_WATER               = 1;   // Water
const uint8_t BLOCK_SAND                = 2;   // Sand
const uint8_t BLOCK_SNOW                = 3;   // Snow
const uint8_t BLOCK_ICE                 = 4;   // Ice
const uint8_t BLOCK_DIRT                = 5;   // Dirt
const uint8_t BLOCK_STONE               = 6;   // Stone
const uint8_t BLOCK_COAL                = 7;   // Coal
const uint8_t BLOCK_IRON                = 8;   // Iron
const uint8_t BLOCK_GOLD                = 9;   // Gold
const uint8_t BLOCK_DIAMOND             = 10;  // Diamond
const uint8_t BLOCK_OBSIDIAN            = 11;  // Obsidian
const uint8_t BLOCK_LAVA                = 12;  // Lava
const uint8_t BLOCK_GLOWSTONE           = 13;  // Glowstone
const uint8_t BLOCK_COBBLESTONE         = 14;  // Cobblestone
const uint8_t BLOCK_CHISELED_BRICK      = 15;  // ChiseledBrick
const uint8_t BLOCK_GRASS               = 16;  // Grass (standard)
const uint8_t BLOCK_GRASS_LIGHT         = 17;  // GrassLight
const uint8_t BLOCK_GRASS_DARK          = 18;  // GrassDark
const uint8_t BLOCK_GRASS_YELLOW        = 19;  // GrassYellow
const uint8_t BLOCK_ACACIA_LOG          = 20;  // AcaciaLog
const uint8_t BLOCK_ACACIA_PLANKS       = 21;  // AcaciaPlanks
const uint8_t BLOCK_ACACIA_LEAVES       = 22;  // AcaciaLeaves
const uint8_t BLOCK_CACTUS_LOG          = 23;  // CactusLog
const uint8_t BLOCK_OAK_LOG             = 24;  // OakLog
const uint8_t BLOCK_OAK_PLANKS          = 25;  // OakPlanks
const uint8_t BLOCK_OAK_LEAVES          = 26;  // OakLeaves
const uint8_t BLOCK_BIRCH_LOG           = 27;  // BirchLog
const uint8_t BLOCK_BIRCH_PLANKS        = 28;  // BirchPlanks
const uint8_t BLOCK_BIRCH_LEAVES        = 29;  // BirchLeaves
const uint8_t BLOCK_JUNGLE_LOG          = 30;  // JungleLog
const uint8_t BLOCK_JUNGLE_PLANKS       = 31;  // JunglePlanks
const uint8_t BLOCK_JUNGLE_LEAVES       = 32;  // JungleLeaves
const uint8_t BLOCK_SPRUCE_LOG          = 33;  // SpruceLog
const uint8_t BLOCK_SPRUCE_PLANKS       = 34;  // SprucePlanks
const uint8_t BLOCK_SPRUCE_LEAVES       = 35;  // SpruceLeaves
const uint8_t BLOCK_SPRUCE_LEAVES_SNOW  = 36;  // SpruceLeavesSnow

//----------------------------------------------------------------------------------------------------
// Tree Stamp Definitions (Phase 3, Task 3B.1 - Assignment 4)
//----------------------------------------------------------------------------------------------------
// Pre-defined tree patterns based on Minecraft wiki structures, simplified for our available assets.
// Each stamp is a 3D grid: blocks[x + y*sizeX + z*sizeX*sizeY]
// Air (0) means skip that position during placement.
//
// Based on Minecraft wiki:
// - Oak (small): 4-5 logs, 25 leaves (simplified to 5x5x6)
// - Spruce (small): 7+ logs, 10+ leaves (matchstick variant, simplified to 5x5x8)
// - Jungle (bush): 1 log, 25-34 leaves (simplified to 5x5x4)
// - Acacia: 9+ logs, 84 leaves (complex angled trunk, simplified to 7x7x8)
//
// Note: Only using tree types with available sprites in BlockSpriteSheet_BlockDefinitions.xml

// Helper macro for cleaner tree stamp data
#define A BLOCK_AIR
#define L BLOCK_OAK_LEAVES
#define W BLOCK_OAK_LOG

// Oak Tree (Small Variant) - 5x5x6 (width x depth x height)
// Based on Minecraft small oak: 4-5 trunk logs, ~25 leaf blocks
// Cross-section at z=0-4 (trunk), z=5 (leaves only)
static TreeStamp g_oakTreeSmall = {
    5, 5, 6,    // sizeX, sizeY, sizeZ
    2, 2,       // trunkOffsetX, trunkOffsetY (trunk at center)
    {
        // Layer z=0 (bottom) - trunk only
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,W,A,A,
        A,A,A,A,A,
        A,A,A,A,A,

        // Layer z=1 - trunk
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,W,A,A,
        A,A,A,A,A,
        A,A,A,A,A,

        // Layer z=2 - trunk
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,W,A,A,
        A,A,A,A,A,
        A,A,A,A,A,

        // Layer z=3 - trunk + first leaves layer
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=4 - trunk + leaves layer
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=5 (top) - leaves only (no trunk)
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,L,L,A,
        A,A,L,A,A,
        A,A,A,A,A,
    }
};

// Oak Tree (Medium Variant) - 5x5x8
static TreeStamp g_oakTreeMedium = {
    5, 5, 8,    // sizeX, sizeY, sizeZ
    2, 2,       // trunkOffsetX, trunkOffsetY (trunk at center)
    {
        // Layer z=0-3 (trunk)
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,

        // Layer z=4 (trunk + leaves begin)
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=5 (trunk + full leaves)
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,W,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=6 (leaves only)
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,L,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=7 (top leaves)
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,L,L,A,
        A,A,L,A,A,
        A,A,A,A,A,
    }
};

// Oak Tree (Large Variant) - 5x5x11
static TreeStamp g_oakTreeLarge = {
    5, 5, 11,   // sizeX, sizeY, sizeZ (increased height)
    2, 2,       // trunkOffsetX, trunkOffsetY (trunk at center)
    {
        // Layer z=0-6 (trunk)
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,

        // Layer z=7 (trunk + leaves begin)
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,W,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=8 (trunk + full leaves)
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,W,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=9 (leaves only)
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,L,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=10 (top leaves)
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,L,L,A,
        A,A,L,A,A,
        A,A,A,A,A,
    }
};

#undef L
#undef W
#define L BLOCK_SPRUCE_LEAVES
#define W BLOCK_SPRUCE_LOG

// Spruce Tree (Small "Matchstick" Variant) - 5x5x8
// Based on Minecraft matchstick spruce: 7+ logs, 10+ leaves (narrow conical shape)
static TreeStamp g_spruceTreeSmall = {
    5, 5, 8,    // sizeX, sizeY, sizeZ
    2, 2,       // trunkOffsetX, trunkOffsetY
    {
        // Layer z=0-2 (bottom trunk)
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,

        // Layer z=3 - trunk + wide leaves base
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=4 - trunk + leaves
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=5 - trunk + narrower leaves
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,W,L,A,
        A,A,L,A,A,
        A,A,A,A,A,

        // Layer z=6 - trunk + leaves
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,W,L,A,
        A,A,L,A,A,
        A,A,A,A,A,

        // Layer z=7 (top) - leaves only
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,L,A,A,
        A,A,A,A,A,
        A,A,A,A,A,
    }
};

// Spruce Tree (Medium Variant) - 5x5x10
static TreeStamp g_spruceTreeMedium = {
    5, 5, 10,   // sizeX, sizeY, sizeZ
    2, 2,       // trunkOffsetX, trunkOffsetY
    {
        // Layer z=0-3 (bottom trunk)
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,

        // Layer z=4 - trunk + wide leaves base
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=5 - trunk + leaves
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=6 - trunk + narrower leaves
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,W,L,A,
        A,A,L,A,A,
        A,A,A,A,A,

        // Layer z=7 - trunk + leaves
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,W,L,A,
        A,A,L,A,A,
        A,A,A,A,A,

        // Layer z=8 - narrower leaves
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,W,A,A,
        A,A,A,A,A,
        A,A,A,A,A,

        // Layer z=9 (top) - leaves only
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,L,A,A,
        A,A,A,A,A,
        A,A,A,A,A,
    }
};

// Spruce Tree (Large Variant) - 5x5x15
static TreeStamp g_spruceTreeLarge = {
    5, 5, 15,   // sizeX, sizeY, sizeZ (increased height)
    2, 2,       // trunkOffsetX, trunkOffsetY
    {
        // Layer z=0-7 (bottom trunk)
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,  A,A,A,A,A,

        // Layer z=8 - trunk + wide leaves base
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=9 - trunk + leaves
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=10 - trunk + narrower leaves
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,W,L,A,
        A,A,L,A,A,
        A,A,A,A,A,

        // Layer z=11 - trunk + leaves
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,W,L,A,
        A,A,L,A,A,
        A,A,A,A,A,

        // Layer z=12 - trunk only
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,W,A,A,
        A,A,A,A,A,
        A,A,A,A,A,

        // Layer z=13 - trunk only
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,W,A,A,
        A,A,A,A,A,
        A,A,A,A,A,

        // Layer z=14 (top) - leaves only
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,L,A,A,
        A,A,A,A,A,
        A,A,A,A,A,
    }
};

#undef L
#undef W
#define L BLOCK_JUNGLE_LEAVES
#define W BLOCK_JUNGLE_LOG

// Jungle Tree (Bush Variant) - 5x5x4
// Based on Minecraft jungle bush: 1 log, 25-34 leaves (very short, bushy)
static TreeStamp g_jungleTreeBush = {
    5, 5, 4,    // sizeX, sizeY, sizeZ
    2, 2,       // trunkOffsetX, trunkOffsetY
    {
        // Layer z=0 (bottom) - trunk only
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,W,A,A,
        A,A,A,A,A,
        A,A,A,A,A,

        // Layer z=1 - trunk + leaves
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,W,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=2 - leaves
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,L,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=3 (top) - leaves
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,L,L,L,
        A,L,L,L,A,
        A,A,L,A,A,
    }
};

// Jungle Tree (Medium Variant) - 5x5x6
static TreeStamp g_jungleTreeMedium = {
    5, 5, 6,    // sizeX, sizeY, sizeZ
    2, 2,       // trunkOffsetX, trunkOffsetY
    {
        // Layer z=0 (bottom) - trunk only
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,W,A,A,
        A,A,A,A,A,
        A,A,A,A,A,

        // Layer z=1 - trunk only
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,W,A,A,
        A,A,A,A,A,
        A,A,A,A,A,

        // Layer z=2 - trunk + leaves
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,W,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=3 - leaves
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,L,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=4 - leaves
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,L,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=5 (top) - leaves
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,L,L,L,
        A,L,L,L,A,
        A,A,L,A,A,
    }
};

// Jungle Tree (Large Variant) - 5x5x9
static TreeStamp g_jungleTreeLarge = {
    5, 5, 9,    // sizeX, sizeY, sizeZ (increased height)
    2, 2,       // trunkOffsetX, trunkOffsetY
    {
        // Layer z=0-2 (bottom trunk)
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,

        // Layer z=3 - trunk + leaves begin
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,W,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=4 - leaves
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,L,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=5 - leaves
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,L,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=6 - leaves
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,L,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=7 - leaves
        A,L,L,L,A,
        L,L,L,L,L,
        L,L,L,L,L,
        L,L,L,L,L,
        A,L,L,L,A,

        // Layer z=8 (top) - leaves
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,L,L,L,
        A,L,L,L,A,
        A,A,L,A,A,
    }
};

#undef L
#undef W
#define L BLOCK_ACACIA_LEAVES
#define W BLOCK_ACACIA_LOG

// Acacia Tree (Simplified) - 7x7x7
// Based on Minecraft acacia: 9+ logs, 84 leaves (angled trunk)
// Simplified to vertical trunk with wide canopy
static TreeStamp g_acaciaTree = {
    7, 7, 7,    // sizeX, sizeY, sizeZ
    3, 3,       // trunkOffsetX, trunkOffsetY
    {
        // Layer z=0-2 (trunk)
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,

        // Layer z=3 - trunk + leaves start
        A,A,A,A,A,A,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,L,L,W,L,L,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,A,A,A,A,A,A,

        // Layer z=4 - trunk + wide canopy
        A,L,L,L,L,L,A,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,W,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        A,L,L,L,L,L,A,

        // Layer z=5 - leaves
        A,L,L,L,L,L,A,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        A,L,L,L,L,L,A,

        // Layer z=6 (top) - leaves only
        A,A,A,A,A,A,A,
        A,A,L,L,L,A,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,A,L,L,L,A,A,
        A,A,A,A,A,A,A,
    }
};

// Acacia Tree (Medium Variant) - 7x7x9
static TreeStamp g_acaciaTreeMedium = {
    7, 7, 9,    // sizeX, sizeY, sizeZ
    3, 3,       // trunkOffsetX, trunkOffsetY
    {
        // Layer z=0-3 (trunk)
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,

        // Layer z=4 - trunk + leaves start
        A,A,A,A,A,A,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,L,L,W,L,L,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,A,A,A,A,A,A,

        // Layer z=5 - trunk + wide canopy
        A,L,L,L,L,L,A,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,W,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        A,L,L,L,L,L,A,

        // Layer z=6 - leaves
        A,L,L,L,L,L,A,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        A,L,L,L,L,L,A,

        // Layer z=7 - leaves
        A,L,L,L,L,L,A,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        A,L,L,L,L,L,A,

        // Layer z=8 (top) - leaves only
        A,A,A,A,A,A,A,
        A,A,L,L,L,A,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,A,L,L,L,A,A,
        A,A,A,A,A,A,A,
    }
};

// Acacia Tree (Large Variant) - 7x7x12
static TreeStamp g_acaciaTreeLarge = {
    7, 7, 12,   // sizeX, sizeY, sizeZ
    3, 3,       // trunkOffsetX, trunkOffsetY
    {
        // Layer z=0-6 (trunk)
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,
        A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,W,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,  A,A,A,A,A,A,A,

        // Layer z=7 - trunk + leaves start
        A,A,A,A,A,A,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,L,L,W,L,L,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,A,A,A,A,A,A,

        // Layer z=8 - trunk + wide canopy
        A,L,L,L,L,L,A,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,W,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        A,L,L,L,L,L,A,

        // Layer z=9 - leaves
        A,L,L,L,L,L,A,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        A,L,L,L,L,L,A,

        // Layer z=10 - leaves
        A,L,L,L,L,L,A,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        L,L,L,L,L,L,L,
        A,L,L,L,L,L,A,

        // Layer z=11 (top) - leaves only
        A,A,A,A,A,A,A,
        A,A,L,L,L,A,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,L,L,L,L,L,A,
        A,A,L,L,L,A,A,
        A,A,A,A,A,A,A,
    }
};

#undef A
#undef L
#undef W

// Re-define macros for Birch tree
#define A BLOCK_AIR
#define L BLOCK_BIRCH_LEAVES
#define W BLOCK_BIRCH_LOG

// Birch Tree (Small Variant) - 5x5x7
// Based on Minecraft birch: 5+ logs, 57 leaves (tall and slender)
// Similar to oak but taller and narrower canopy
static TreeStamp g_birchTree = {
    5, 5, 7,    // sizeX, sizeY, sizeZ
    2, 2,       // trunkOffsetX, trunkOffsetY
    {
        // Layer z=0-2 (trunk only)
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,

        // Layer z=3 - trunk only
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,W,A,A,
        A,A,A,A,A,
        A,A,A,A,A,

        // Layer z=4 - trunk + first leaves
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=5 - trunk + leaves
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=6 (top) - leaves only
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,L,L,A,
        A,A,L,A,A,
        A,A,A,A,A,
    }
};

#undef L
#undef W
#define L BLOCK_SPRUCE_LEAVES_SNOW
#define W BLOCK_SPRUCE_LOG

// Snowy Spruce Tree (Small Variant) - 5x5x8
// Based on spruce matchstick but with snowy leaves for SNOWY_TAIGA biome
// Same structure as regular spruce, different leaf texture
static TreeStamp g_snowySpruceTree = {
    5, 5, 8,    // sizeX, sizeY, sizeZ
    2, 2,       // trunkOffsetX, trunkOffsetY
    {
        // Layer z=0-2 (bottom trunk)
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,
        A,A,A,A,A,  A,A,A,A,A,  A,A,W,A,A,  A,A,A,A,A,  A,A,A,A,A,

        // Layer z=3 - trunk + wide leaves base
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=4 - trunk + leaves
        A,A,L,A,A,
        A,L,L,L,A,
        L,L,W,L,L,
        A,L,L,L,A,
        A,A,L,A,A,

        // Layer z=5 - trunk + narrower leaves
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,W,L,A,
        A,A,L,A,A,
        A,A,A,A,A,

        // Layer z=6 - trunk + leaves
        A,A,A,A,A,
        A,A,L,A,A,
        A,L,W,L,A,
        A,A,L,A,A,
        A,A,A,A,A,

        // Layer z=7 (top) - leaves only
        A,A,A,A,A,
        A,A,A,A,A,
        A,A,L,A,A,
        A,A,A,A,A,
        A,A,A,A,A,
    }
};

#undef A
#undef L
#undef W

// Re-define macros for Cactus
#define A BLOCK_AIR
#define C BLOCK_CACTUS_LOG

// Cactus (Vertical Column) - 3x3x5
// Cacti are simple vertical columns, 3-5 blocks tall
// Found in DESERT biome only (not SAVANNA, which has acacia trees)
static TreeStamp g_cactus = {
    3, 3, 5,    // sizeX, sizeY, sizeZ (smaller footprint)
    1, 1,       // trunkOffsetX, trunkOffsetY (center column)
    {
        // Layer z=0 (bottom)
        A,A,A,
        A,C,A,
        A,A,A,

        // Layer z=1
        A,A,A,
        A,C,A,
        A,A,A,

        // Layer z=2
        A,A,A,
        A,C,A,
        A,A,A,

        // Layer z=3
        A,A,A,
        A,C,A,
        A,A,A,

        // Layer z=4 (top)
        A,A,A,
        A,C,A,
        A,A,A,
    }
};

// Cactus (Medium Variant) - 3x3x6
static TreeStamp g_cactusMedium = {
    3, 3, 6,    // sizeX, sizeY, sizeZ
    1, 1,       // trunkOffsetX, trunkOffsetY (center column)
    {
        // Layer z=0-5 (6 blocks tall)
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
    }
};

// Cactus (Large Variant) - 3x3x9
static TreeStamp g_cactusLarge = {
    3, 3, 9,    // sizeX, sizeY, sizeZ
    1, 1,       // trunkOffsetX, trunkOffsetY (center column)
    {
        // Layer z=0-8 (9 blocks tall)
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
        A,A,A,  A,C,A,  A,A,A,
    }
};

#undef A
#undef C

//----------------------------------------------------------------------------------------------------
Chunk::Chunk(IntVec2 const& chunkCoords)
    : m_chunkCoords(chunkCoords)
{
    // Calculate world bounds
    Vec3 worldMins((float)(chunkCoords.x) * CHUNK_SIZE_X, (float)(chunkCoords.y) * CHUNK_SIZE_Y, 0.f);
    Vec3 worldMaxs = worldMins + Vec3((float)CHUNK_SIZE_X, (float)CHUNK_SIZE_Y, (float)CHUNK_SIZE_Z);
    m_worldBounds  = AABB3(worldMins, worldMaxs);

    // Initialize all blocks to air (terrain generation happens asynchronously)
    // CRITICAL FIX (2025-11-16): Initialize ALL Block members to prevent garbage memory values
    // BUG WAS: Only m_typeIndex initialized, m_lightingData and m_bitFlags contained random values
    // This caused underground blocks to appear bright if garbage data had high outdoor light bits
    for (int i = 0; i < BLOCKS_PER_CHUNK; ++i)
    {
        m_blocks[i].m_typeIndex = 0;      // BLOCK_AIR
        m_blocks[i].m_lightingData = 0;   // outdoor=0, indoor=0 (both nibbles zero)
        m_blocks[i].m_bitFlags = 0;       // isSkyVisible=false, all flags clear
    }
}

//----------------------------------------------------------------------------------------------------
Chunk::~Chunk()
{
    // DebuggerPrintf("[CHUNK DESTRUCTOR] Chunk(%d,%d) deleting buffers...\n",
    //               m_chunkCoords.x, m_chunkCoords.y);
    // DebuggerPrintf("[CHUNK DESTRUCTOR]   m_vertexBuffer = %p\n", m_vertexBuffer);
    // DebuggerPrintf("[CHUNK DESTRUCTOR]   m_indexBuffer = %p\n", m_indexBuffer);
    // DebuggerPrintf("[CHUNK DESTRUCTOR]   m_debugVertexBuffer = %p\n", m_debugVertexBuffer);

    GAME_SAFE_RELEASE(m_vertexBuffer);
    GAME_SAFE_RELEASE(m_indexBuffer);
    GAME_SAFE_RELEASE(m_debugVertexBuffer);
    // GAME_SAFE_RELEASE(m_debugBuffer);

    // DebuggerPrintf("[CHUNK DESTRUCTOR] Chunk(%d,%d) buffers released\n",
    //               m_chunkCoords.x, m_chunkCoords.y);
}

//----------------------------------------------------------------------------------------------------
void Chunk::Update(float const deltaSeconds)
{
    UNUSED(deltaSeconds)

    // NOTE: Mesh rebuilding is now managed by World class to ensure only one chunk per frame
    // The World::Update() method calls RebuildMesh() directly on the nearest dirty chunk
    // This ensures the assignment requirement of "up to one active chunk at most may be built or rebuilt per frame"

    // NOTE: F2 debug key handling is now managed by World class to ensure consistent behavior
    // across all chunks including newly activated ones

    // Cross-Chunk Tree Placement (Option 1: Post-Processing Pass)
    // Place trees that extend into neighboring chunks after this chunk is complete
    // and neighbors are available. This ensures thread-safe cross-chunk operations.
    if (GetState() == ChunkState::COMPLETE && !m_crossChunkTrees.empty())
    {
        PlaceCrossChunkTrees();
    }
}

//----------------------------------------------------------------------------------------------------
void Chunk::Render()
{
    // CRITICAL FIX: Don't render dirty chunks - they have stale buffer data
    // When a neighbor chunk is modified, this chunk gets marked dirty but buffers
    // still contain old mesh data, causing a flash when rendered with stale data
    if (m_isMeshDirty)
    {
        return;  // Skip rendering - mesh is being rebuilt or needs rebuild
    }

    // CRITICAL FIX: Only check buffers, not m_vertices
    // m_vertices can be inconsistent during SetMeshData() causing false negatives
    // Buffers are the source of truth for whether chunk can be rendered
    if (m_vertexBuffer && m_indexBuffer)
    {
        g_renderer->SetBlendMode(eBlendMode::OPAQUE);
        g_renderer->SetRasterizerMode(eRasterizerMode::SOLID_CULL_BACK);
        g_renderer->SetSamplerMode(eSamplerMode::POINT_CLAMP);
        g_renderer->SetDepthMode(eDepthMode::READ_WRITE_LESS_EQUAL);

        // Phase 1, Task 1.1: Use Assignment 4 Dokucraft High 32px sprite sheet (matches new XML layout)
        // NOTE: World.hlsl shader already bound by World::Render() with constant buffer for day/night cycle
        // Do NOT bind shader here - it would require re-binding the constant buffer
        g_renderer->BindTexture(g_resourceSubsystem->CreateOrGetTextureFromFile("Data/Images/SpriteSheet_Faithful_64x.png"));
        // CRITICAL FIX: Use buffer's internal size to avoid race condition during RebuildMesh()
        // RebuildMesh() clears m_indices at line 579, causing m_indices.size() to return 0
        // This results in bright flashing when rendering with 0 indices
        // Buffer's size is stable and represents the actual GPU data
        int indexCount = m_indexBuffer->GetSize() / m_indexBuffer->GetStride();

        g_renderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, indexCount);
    }

    if (!m_drawDebug || !m_debugVertexBuffer) return;

    g_renderer->BindTexture(nullptr);
    // Use buffer's internal size for consistency (same fix as main rendering)
    int debugVertexCount = m_debugVertexBuffer->GetSize() / m_debugVertexBuffer->GetStride();
    g_renderer->DrawVertexBuffer(m_debugVertexBuffer, debugVertexCount);
}

//----------------------------------------------------------------------------------------------------
// Helper function for Phase 0, Task 0.4: Debug Visualization
// Converts noise value [-1, 1] to a colored block type for visualization
//----------------------------------------------------------------------------------------------------
static uint8_t GetDebugVisualizationBlock(float noiseValue, DebugVisualizationMode mode)
{
    // Clamp noise value to [-1, 1] range using Engine's GetClamped() template function
    float clampedValue = GetClamped(noiseValue, -1.f, 1.f);

    // Map to [0, 1] range for easier color mapping
    float normalizedValue = (clampedValue + 1.f) * 0.5f;

    // Different color schemes for different noise layers
    switch (mode)
    {
        case DebugVisualizationMode::TEMPERATURE:
            // Hot (red/yellow) to Cold (blue/white)
            if (normalizedValue > 0.75f)      return BLOCK_LAVA;          // Very hot - red
            if (normalizedValue > 0.5f)       return BLOCK_GOLD;          // Hot - yellow/gold
            if (normalizedValue > 0.25f)      return BLOCK_STONE;         // Cool - gray
            if (normalizedValue > 0.1f)       return BLOCK_COBBLESTONE;   // Cold - dark gray
            return BLOCK_ICE;  // Very cold - blue/white

        case DebugVisualizationMode::HUMIDITY:
            // Wet (blue) to Dry (yellow/brown)
            if (normalizedValue > 0.75f)      return BLOCK_ICE;           // Very wet - blue
            if (normalizedValue > 0.5f)       return BLOCK_COBBLESTONE;   // Wet - dark
            if (normalizedValue > 0.25f)      return BLOCK_DIRT;          // Medium - brown
            if (normalizedValue > 0.1f)       return BLOCK_SAND;          // Dry - tan
            return BLOCK_GOLD;  // Very dry - yellow

        case DebugVisualizationMode::CONTINENTALNESS:
            // Ocean (blue) to Inland (green/brown)
            if (normalizedValue > 0.75f)      return BLOCK_GRASS;         // Far inland - green
            if (normalizedValue > 0.5f)       return BLOCK_DIRT;          // Mid inland - brown
            if (normalizedValue > 0.25f)      return BLOCK_SAND;          // Coast - tan
            if (normalizedValue > 0.1f)       return BLOCK_COBBLESTONE;   // Near ocean - gray
            return BLOCK_ICE;  // Ocean - blue

        case DebugVisualizationMode::EROSION:
            // Flat (green) to Mountainous (brown/gray)
            if (normalizedValue > 0.75f)      return BLOCK_STONE;         // Very eroded/mountainous - gray
            if (normalizedValue > 0.5f)       return BLOCK_COBBLESTONE;   // Eroded - dark gray
            if (normalizedValue > 0.25f)      return BLOCK_DIRT;          // Medium - brown
            if (normalizedValue > 0.1f)       return BLOCK_GRASS;         // Flat - green
            return BLOCK_SAND;  // Very flat - smooth tan

        case DebugVisualizationMode::WEIRDNESS:
            // Normal (gray) to Weird (colorful)
            if (normalizedValue > 0.75f)      return BLOCK_DIAMOND;       // Very weird - cyan
            if (normalizedValue > 0.5f)       return BLOCK_GOLD;          // Weird - yellow
            if (normalizedValue > 0.25f)      return BLOCK_STONE;         // Normal - gray
            if (normalizedValue > 0.1f)       return BLOCK_COBBLESTONE;   // Slightly weird - dark gray
            return BLOCK_IRON;  // Normal - iron gray

        case DebugVisualizationMode::PEAKS_VALLEYS:
            // Valleys (dark) to Peaks (white/bright)
            if (normalizedValue > 0.75f)      return BLOCK_ICE;           // Peaks - white/bright
            if (normalizedValue > 0.5f)       return BLOCK_STONE;         // High - light gray
            if (normalizedValue > 0.25f)      return BLOCK_COBBLESTONE;   // Mid - gray
            if (normalizedValue > 0.1f)       return BLOCK_DIRT;          // Low - brown
            return BLOCK_COAL;  // Valleys - black/dark

        case DebugVisualizationMode::BIOME_TYPE:
        {
            // Special case: noiseValue is actually BiomeType enum cast to float
            // No need to normalize - directly cast back to BiomeType
            BiomeType biome = static_cast<BiomeType>(static_cast<int>(noiseValue));

            // Map each biome to a distinctive colored block
            // CRITICAL FIX: Use different colored blocks for each biome to show variety
            // Previous issue: PLAINS/FOREST/JUNGLE/TAIGA all mapped to BLOCK_GRASS (green)
            switch (biome)
            {
                case BiomeType::OCEAN:          return BLOCK_DIAMOND;       // Cyan (water)
                case BiomeType::DEEP_OCEAN:     return BLOCK_COBBLESTONE;   // Dark blue/gray
                case BiomeType::FROZEN_OCEAN:   return BLOCK_ICE;           // White/blue (ice)
                case BiomeType::BEACH:          return BLOCK_SAND;          // Tan (sand)
                case BiomeType::SNOWY_BEACH:    return BLOCK_SNOW;          // White (snow)
                case BiomeType::DESERT:         return BLOCK_GOLD;          // Yellow/gold (sand)
                case BiomeType::SAVANNA:        return BLOCK_DIRT;          // Brown (dirt)
                case BiomeType::PLAINS:         return BLOCK_GRASS;         // Light green (grass)
                case BiomeType::SNOWY_PLAINS:   return BLOCK_SNOW;          // White (snow)
                case BiomeType::FOREST:         return BLOCK_OAK_LEAVES;    // Green (distinct from plains)
                case BiomeType::JUNGLE:         return BLOCK_JUNGLE_LEAVES; // Dark green (dense forest)
                case BiomeType::TAIGA:          return BLOCK_SPRUCE_LEAVES; // Blue-green (conifer forest)
                case BiomeType::SNOWY_TAIGA:    return BLOCK_SPRUCE_LEAVES_SNOW; // White/green (snow + trees)
                case BiomeType::STONY_PEAKS:    return BLOCK_STONE;         // Gray (rocky mountains)
                case BiomeType::SNOWY_PEAKS:    return BLOCK_ICE;           // White (snowy mountains)
                default:                        return BLOCK_STONE;         // Fallback
            }
        }

        default:
            return BLOCK_STONE;  // Should never reach here
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 4: Biome Selection Helper Function (Phase 1, Task 1.3)
//----------------------------------------------------------------------------------------------------
// Determines biome type based on 6 noise parameters using lookup table approach
// Based on Minecraft 1.18+ biome selection system
// Reference: Docs/Biomes.txt for complete lookup tables
//----------------------------------------------------------------------------------------------------
static BiomeType SelectBiome(float temperature, float humidity, float continentalness,
                             float erosion, float peaksValleys)
{
    // Discretize temperature into 5 levels (T0-T4)
    // T0: [-1.00, -0.45) Frozen, T1: [-0.45, -0.15) Cool, T2: [-0.15, 0.20) Neutral
    // T3: [0.20, 0.55) Warm, T4: [0.55, 1.00] Hot
    int tempLevel = 2;  // Default: T2 (Neutral)
    if (temperature < -0.45f)      tempLevel = 0;  // T0 (Frozen)
    else if (temperature < -0.15f) tempLevel = 1;  // T1 (Cool)
    else if (temperature < 0.20f)  tempLevel = 2;  // T2 (Neutral)
    else if (temperature < 0.55f)  tempLevel = 3;  // T3 (Warm)
    else                           tempLevel = 4;  // T4 (Hot)

    // Discretize humidity into 5 levels (H0-H4)
    // H0: [-1.00, -0.35) Very Dry, H1: [-0.35, -0.10) Dry, H2: [-0.10, 0.10) Medium
    // H3: [0.10, 0.30) Wet, H4: [0.30, 1.00] Very Wet
    int humidLevel = 2;  // Default: H2 (Medium)
    if (humidity < -0.35f)      humidLevel = 0;  // H0 (Very Dry)
    else if (humidity < -0.10f) humidLevel = 1;  // H1 (Dry)
    else if (humidity < 0.10f)  humidLevel = 2;  // H2 (Medium)
    else if (humidity < 0.30f)  humidLevel = 3;  // H3 (Wet)
    else                        humidLevel = 4;  // H4 (Very Wet)

    // Hierarchical biome selection (simplified from Minecraft's lookup tables)

    // Step 1: Check continentalness for ocean biomes
    if (continentalness < -0.19f)  // Ocean/Deep Ocean range
    {
        if (tempLevel == 0)  // T0 = Frozen
            return BiomeType::FROZEN_OCEAN;
        else if (continentalness < -1.05f)  // Deep ocean threshold
            return BiomeType::DEEP_OCEAN;
        else
            return BiomeType::OCEAN;
    }

    // Step 2: Beach biomes (coast areas)
    if (continentalness < -0.11f)  // Coast range
    {
        if (tempLevel == 0)  // T0 = Frozen
            return BiomeType::SNOWY_BEACH;
        else
            return BiomeType::BEACH;
    }

    // Step 3: Inland biomes - Use Peaks & Valleys and Erosion for categorization

    // Peaks biomes (high PV values)
    if (peaksValleys > 0.7f)
    {
        if (tempLevel <= 2)  // T0-T2 = Cold/neutral
            return BiomeType::SNOWY_PEAKS;
        else
            return BiomeType::STONY_PEAKS;
    }

    // Badland biomes (low erosion + dry/medium humidity)
    // CRITICAL FIX: Outer condition checks humidLevel <= 2, so inner condition was redundant
    // Now properly distinguishes between DESERT (very dry) and SAVANNA (dry to medium)
    if (erosion < -0.2225f && humidLevel <= 2)
    {
        if (humidLevel <= 1)  // H0-H1 = Very Dry to Dry
            return BiomeType::DESERT;
        else  // H2 = Medium
            return BiomeType::SAVANNA;
    }

    // Middle biomes (most common) - Use temperature and humidity
    if (tempLevel == 0)  // T0 = Frozen
    {
        if (humidLevel <= 1)
            return BiomeType::SNOWY_PLAINS;
        else if (humidLevel <= 2)
            return BiomeType::SNOWY_TAIGA;
        else
            return BiomeType::TAIGA;
    }
    else if (tempLevel == 1)  // T1 = Cool
    {
        if (humidLevel >= 2)
            return BiomeType::FOREST;
        else
            return BiomeType::PLAINS;
    }
    else if (tempLevel >= 3)  // T3-T4 = Warm/Hot
    {
        if (humidLevel >= 3)
            return BiomeType::JUNGLE;
        else if (humidLevel <= 2)
            return BiomeType::SAVANNA;
        else
            return BiomeType::PLAINS;
    }

    // Default: Plains (T2 Neutral temperature, medium conditions)
    return BiomeType::PLAINS;
}

//----------------------------------------------------------------------------------------------------
void Chunk::GenerateTerrain()
{
    // Establish world-space position and bounds of this chunk
    Vec3 chunkPosition((float)(m_chunkCoords.x) * CHUNK_SIZE_X, (float)(m_chunkCoords.y) * CHUNK_SIZE_Y, 0.f);
    Vec3 chunkBounds = chunkPosition + Vec3((float)CHUNK_SIZE_X, (float)CHUNK_SIZE_Y, (float)CHUNK_SIZE_Z);

    // Phase 0, Task 0.4: Check for debug visualization mode
    // If in visualization mode, generate simple flat terrain showing noise values as colored blocks
    DebugVisualizationMode vizMode = DebugVisualizationMode::NORMAL_TERRAIN;
    if (g_game && g_game->GetWorld())
    {
        vizMode = g_game->GetWorld()->GetDebugVisualizationMode();
    }

    if (vizMode != DebugVisualizationMode::NORMAL_TERRAIN)
    {
        // Generate debug visualization terrain
        // Create a flat layer at Y=80 (sea level) showing the selected noise layer as colored blocks

        // Derive seed for the noise layer being visualized
        unsigned int visualizationSeed = GAME_SEED;
        switch (vizMode)
        {
            case DebugVisualizationMode::TEMPERATURE:    visualizationSeed = GAME_SEED + 2; break;
            case DebugVisualizationMode::HUMIDITY:       visualizationSeed = GAME_SEED + 1; break;
            case DebugVisualizationMode::CONTINENTALNESS: visualizationSeed = GAME_SEED + 5; break;
            case DebugVisualizationMode::EROSION:        visualizationSeed = GAME_SEED + 6; break;
            case DebugVisualizationMode::WEIRDNESS:      visualizationSeed = GAME_SEED + 7; break;
            case DebugVisualizationMode::PEAKS_VALLEYS:  visualizationSeed = GAME_SEED + 8; break;
            case DebugVisualizationMode::BIOME_TYPE:     visualizationSeed = GAME_SEED + 9; break;  // Phase 1, Task 1.4
            default: break;
        }

        // Fill blocks with visualization data
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
        {
            for (int x = 0; x < CHUNK_SIZE_X; x++)
            {
                int globalX = m_chunkCoords.x * CHUNK_SIZE_X + x;
                int globalY = m_chunkCoords.y * CHUNK_SIZE_Y + y;

                // Sample the appropriate noise layer based on visualization mode
                float noiseValue = 0.f;
                switch (vizMode)
                {
                    case DebugVisualizationMode::TEMPERATURE:
                    {
                        float rawTemp = Get2dNoiseNegOneToOne(globalX, globalY, visualizationSeed) * TEMPERATURE_RAW_NOISE_SCALE;
                        noiseValue = rawTemp + 0.5f + 0.5f * Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            g_worldGenConfig->biomeNoise.temperatureScale,
                            g_worldGenConfig->biomeNoise.temperatureOctaves,
                            g_worldGenConfig->biomeNoise.temperaturePersistence,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        // Remap from [0,1] to [-1,1]
                        noiseValue = (noiseValue * 2.f) - 1.f;
                        break;
                    }

                    case DebugVisualizationMode::HUMIDITY:
                        noiseValue = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            g_worldGenConfig->biomeNoise.humidityScale,
                            g_worldGenConfig->biomeNoise.humidityOctaves,
                            g_worldGenConfig->biomeNoise.humidityPersistence,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        break;

                    case DebugVisualizationMode::CONTINENTALNESS:
                        noiseValue = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            g_worldGenConfig->biomeNoise.continentalnessScale,
                            g_worldGenConfig->biomeNoise.continentalnessOctaves,
                            g_worldGenConfig->biomeNoise.continentalnessPersistence,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        break;

                    case DebugVisualizationMode::EROSION:
                        noiseValue = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            g_worldGenConfig->biomeNoise.erosionScale,
                            g_worldGenConfig->biomeNoise.erosionOctaves,
                            g_worldGenConfig->biomeNoise.erosionPersistence,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        break;

                    case DebugVisualizationMode::WEIRDNESS:
                        noiseValue = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            g_worldGenConfig->biomeNoise.weirdnessScale,
                            g_worldGenConfig->biomeNoise.weirdnessOctaves,
                            g_worldGenConfig->biomeNoise.weirdnessPersistence,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        break;

                    case DebugVisualizationMode::PEAKS_VALLEYS:
                    {
                        // PV is derived from weirdness: PV = 1 - |( 3 * abs(W) ) - 2|
                        float weirdness = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            g_worldGenConfig->biomeNoise.weirdnessScale,
                            g_worldGenConfig->biomeNoise.weirdnessOctaves,
                            g_worldGenConfig->biomeNoise.weirdnessPersistence,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        noiseValue = 1.f - fabsf((3.f * fabsf(weirdness)) - 2.f);
                        break;
                    }

                    case DebugVisualizationMode::BIOME_TYPE:
                    {
                        // Phase 1, Task 1.4: Sample all 6 biome noise layers and determine biome type
                        // CRITICAL FIX: Use the SAME seeds as normal terrain generation (lines 599-610)
                        // Otherwise visualization shows different biomes than actual terrain!

                        // Derive seeds matching normal terrain generation
                        unsigned int humiditySeed = GAME_SEED + 1;
                        unsigned int temperatureSeed = GAME_SEED + 2;
                        unsigned int continentalnessSeed = GAME_SEED + 6;
                        unsigned int erosionSeed = GAME_SEED + 7;
                        unsigned int weirdnessSeed = GAME_SEED + 8;

                        // Sample Temperature (matches Pass 1 lines 638-647)
                        float rawTemp = Get2dNoiseNegOneToOne(globalX, globalY, temperatureSeed) * TEMPERATURE_RAW_NOISE_SCALE;
                        float temperature = rawTemp + 0.5f + 0.5f * Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            g_worldGenConfig->biomeNoise.temperatureScale,
                            g_worldGenConfig->biomeNoise.temperatureOctaves,
                            g_worldGenConfig->biomeNoise.temperaturePersistence,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            temperatureSeed
                        );
                        // Normalize temperature from [0,1] to [-1,1] for biome selection
                        float temperatureNormalized = RangeMap(temperature, 0.f, 1.f, -1.f, 1.f);

                        // Sample Humidity (matches Pass 1 lines 627-635)
                        float humidity = 0.5f + 0.5f * Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            g_worldGenConfig->biomeNoise.humidityScale,
                            g_worldGenConfig->biomeNoise.humidityOctaves,
                            g_worldGenConfig->biomeNoise.humidityPersistence,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            humiditySeed
                        );
                        // Normalize humidity from [0,1] to [-1,1] for biome selection
                        float humidityNormalized = RangeMap(humidity, 0.f, 1.f, -1.f, 1.f);

                        // Sample Continentalness (matches Pass 1 lines 653-661)
                        float continentalness = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            g_worldGenConfig->biomeNoise.continentalnessScale,
                            g_worldGenConfig->biomeNoise.continentalnessOctaves,
                            g_worldGenConfig->biomeNoise.continentalnessPersistence,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            continentalnessSeed
                        );

                        // Sample Erosion (matches Pass 1 lines 664-672)
                        float erosion = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            g_worldGenConfig->biomeNoise.erosionScale,
                            g_worldGenConfig->biomeNoise.erosionOctaves,
                            g_worldGenConfig->biomeNoise.erosionPersistence,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            erosionSeed
                        );

                        // Sample Weirdness (matches Pass 1 lines 675-683)
                        float weirdness = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            g_worldGenConfig->biomeNoise.weirdnessScale,
                            g_worldGenConfig->biomeNoise.weirdnessOctaves,
                            g_worldGenConfig->biomeNoise.weirdnessPersistence,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            weirdnessSeed
                        );

                        // Calculate Peaks & Valleys from Weirdness (matches Pass 1 line 687)
                        float peaksValleys = 1.f - fabsf((3.f * fabsf(weirdness)) - 2.f);

                        // Determine biome type using lookup table (from Task 1.3)
                        BiomeType biome = SelectBiome(temperatureNormalized, humidityNormalized,
                                                       continentalness, erosion, peaksValleys);

                        // DEBUG: Log detailed biome noise values for investigation
                        if (globalX % 16 == 0 && globalY % 16 == 0)  // Log every 16th position to reduce spam
                        {
                            const char* biomeName = "UNKNOWN";
                            switch (biome) {
                                case BiomeType::OCEAN: biomeName = "OCEAN"; break;
                                case BiomeType::DEEP_OCEAN: biomeName = "DEEP_OCEAN"; break;
                                case BiomeType::FROZEN_OCEAN: biomeName = "FROZEN_OCEAN"; break;
                                case BiomeType::BEACH: biomeName = "BEACH"; break;
                                case BiomeType::SNOWY_BEACH: biomeName = "SNOWY_BEACH"; break;
                                case BiomeType::DESERT: biomeName = "DESERT"; break;
                                case BiomeType::SAVANNA: biomeName = "SAVANNA"; break;
                                case BiomeType::PLAINS: biomeName = "PLAINS"; break;
                                case BiomeType::SNOWY_PLAINS: biomeName = "SNOWY_PLAINS"; break;
                                case BiomeType::FOREST: biomeName = "FOREST"; break;
                                case BiomeType::JUNGLE: biomeName = "JUNGLE"; break;
                                case BiomeType::TAIGA: biomeName = "TAIGA"; break;
                                case BiomeType::SNOWY_TAIGA: biomeName = "SNOWY_TAIGA"; break;
                                case BiomeType::STONY_PEAKS: biomeName = "STONY_PEAKS"; break;
                                case BiomeType::SNOWY_PEAKS: biomeName = "SNOWY_PEAKS"; break;
                            }


                        }

                        // Cast biome enum to float for color mapping
                        noiseValue = static_cast<float>(static_cast<int>(biome));
                        break;
                    }

                    default: break;
                }

                // Get the appropriate colored block for this noise value
                uint8_t visualizationBlock = GetDebugVisualizationBlock(noiseValue, vizMode);

                // Create a flat visualization surface at Y=80 (sea level)
                // Phase 0, Task 0.5: Updated to match new sea level (was Y=64 for 128-block chunks)
                constexpr int VISUALIZATION_HEIGHT = 80;

                // BUGFIX: Use direct array assignment instead of SetBlock() to avoid triggering saves
                // Debug visualization is temporary and should NOT mark chunks as needing save
                // Fill bottom with stone (foundation)
                for (int z = 0; z < VISUALIZATION_HEIGHT; z++)
                {
                    int blockIdx = LocalCoordsToIndex(x, y, z);
                    m_blocks[blockIdx].m_typeIndex = BLOCK_STONE;
                }

                // Place the visualization block on top
                int topBlockIdx = LocalCoordsToIndex(x, y, VISUALIZATION_HEIGHT);
                m_blocks[topBlockIdx].m_typeIndex = visualizationBlock;

                // Leave everything above as air
                for (int z = VISUALIZATION_HEIGHT + 1; z < CHUNK_SIZE_Z; z++)
                {
                    int blockIdx = LocalCoordsToIndex(x, y, z);
                    m_blocks[blockIdx].m_typeIndex = BLOCK_AIR;
                }
            }
        }

        // Mark chunk as needing mesh rebuild (but NOT save - debug visualization is temporary)
        // BUGFIX: Debug visualization is NOT a player modification, so don't save to disk
        // Saving debug visualization would write temporary colored blocks to .chunk files
        SetIsMeshDirty(true);
        // NOTE: Do NOT call SetNeedsSaving(true) - debug viz is temporary and shouldn't persist
        return;  // Skip normal terrain generation
    }

    // Derive deterministic seeds for each noise channel
    unsigned int terrainSeed     = GAME_SEED;
    unsigned int humiditySeed    = GAME_SEED + 1;
    unsigned int temperatureSeed = humiditySeed + 1;
    unsigned int hillSeed        = temperatureSeed + 1;
    unsigned int oceanSeed       = hillSeed + 1;
    unsigned int dirtSeed        = oceanSeed + 1;

    // Assignment 4: Biome noise seeds (Phase 1, Task 1.3)
    unsigned int continentalnessSeed = dirtSeed + 1;
    unsigned int erosionSeed         = continentalnessSeed + 1;
    unsigned int weirdnessSeed       = erosionSeed + 1;

    // Allocate per-(x,y) maps
    int   heightMapXY[CHUNK_SIZE_X * CHUNK_SIZE_Y];
    int   dirtDepthXY[CHUNK_SIZE_X * CHUNK_SIZE_Y];
    float humidityMapXY[CHUNK_SIZE_X * CHUNK_SIZE_Y];
    float temperatureMapXY[CHUNK_SIZE_X * CHUNK_SIZE_Y];

    // --- Pass 1: compute surface & biome fields per (x,y) pillar ---
    for (int y = 0; y < CHUNK_SIZE_Y; y++)
    {
        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            int globalX = m_chunkCoords.x * CHUNK_SIZE_X + x;
            int globalY = m_chunkCoords.y * CHUNK_SIZE_Y + y;

            // Humidity calculation (0.5 + 0.5 * Perlin2D(...))
            float humidity = 0.5f + 0.5f * Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                g_worldGenConfig->biomeNoise.humidityScale,
                g_worldGenConfig->biomeNoise.humidityOctaves,
                g_worldGenConfig->biomeNoise.humidityPersistence,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize
                humiditySeed
            );

            // Temperature calculation (raw noise + Perlin)
            float temperature = Get2dNoiseNegOneToOne(globalX, globalY, temperatureSeed) * TEMPERATURE_RAW_NOISE_SCALE;
            temperature       = temperature + 0.5f + 0.5f * Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                g_worldGenConfig->biomeNoise.temperatureScale,
                g_worldGenConfig->biomeNoise.temperatureOctaves,
                g_worldGenConfig->biomeNoise.temperaturePersistence,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize
                temperatureSeed
            );

            // Assignment 4: Biome noise sampling (Phase 1, Task 1.3)
            // Sample 4 additional noise layers for biome determination

            // Continentalness - Ocean to inland distance (C: [-1.2, 1.0])
            float continentalness = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                g_worldGenConfig->biomeNoise.continentalnessScale,
                g_worldGenConfig->biomeNoise.continentalnessOctaves,
                g_worldGenConfig->biomeNoise.continentalnessPersistence,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize to [-1, 1]
                continentalnessSeed
            );

            // Erosion - Flat to mountainous (E: [-1, 1])
            float erosion = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                g_worldGenConfig->biomeNoise.erosionScale,
                g_worldGenConfig->biomeNoise.erosionOctaves,
                g_worldGenConfig->biomeNoise.erosionPersistence,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize to [-1, 1]
                erosionSeed
            );

            // Weirdness - Terrain variation (W: [-1, 1])
            float weirdness = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                g_worldGenConfig->biomeNoise.weirdnessScale,
                g_worldGenConfig->biomeNoise.weirdnessOctaves,
                g_worldGenConfig->biomeNoise.weirdnessPersistence,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize to [-1, 1]
                weirdnessSeed
            );

            // Peaks & Valleys - Calculated from Weirdness (PV: [-1, 1])
            // Formula: PV = 1 - |(3 * abs(W)) - 2|
            float peaksValleys = 1.f - fabsf((3.f * fabsf(weirdness)) - 2.f);

            // Hilliness calculation
            float rawHill = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                HILLINESS_NOISE_SCALE,
                HILLINESS_NOISE_OCTAVES,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize
                hillSeed
            );
            float hill = SmoothStep3(RangeMap(rawHill, -1.f, 1.f, 0.f, 1.f));

            // Ocean calculation
            float ocean = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                OCEANESS_NOISE_SCALE,
                OCEANESS_NOISE_OCTAVES,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize
                oceanSeed
            );

            // Terrain height calculation
            float rawTerrain = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                TERRAIN_NOISE_SCALE,
                TERRAIN_NOISE_OCTAVES,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize
                terrainSeed
            );

            // Base terrain height with river/hill shaping
            float terrainHeightF = DEFAULT_TERRAIN_HEIGHT + hill * RangeMap(fabsf(rawTerrain), 0.f, 1.f, -RIVER_DEPTH, DEFAULT_TERRAIN_HEIGHT);

            // Ocean depressions
            if (ocean > OCEAN_START_THRESHOLD)
            {
                float oceanBlend = RangeMapClamped(ocean, OCEAN_START_THRESHOLD, OCEAN_END_THRESHOLD, 0.f, 1.f);
                terrainHeightF   = terrainHeightF - Interpolate(0.f, OCEAN_DEPTH, oceanBlend);
            }

            // Dirt layer thickness driven by noise
            float dirtDepthPct = Get2dNoiseZeroToOne(globalX, globalY, dirtSeed);
            int   dirtDepth    = MIN_DIRT_OFFSET_Z + (int)roundf(dirtDepthPct * (float)(MAX_DIRT_OFFSET_Z - MIN_DIRT_OFFSET_Z));

            int idxXY               = y * CHUNK_SIZE_X + x;
            humidityMapXY[idxXY]    = humidity;
            temperatureMapXY[idxXY] = temperature;
            heightMapXY[idxXY]      = (int)floorf(terrainHeightF);
            dirtDepthXY[idxXY]      = dirtDepth;

            // Assignment 4: Store biome data for this column (Phase 1, Task 1.3)
            // Note: Temperature and Humidity ranges need adjustment from [0,1] to [-1,1]
            // Current calculation produces [0,1], but biome selection expects [-1,1]
            float temperatureNormalized = RangeMap(temperature, 0.f, 1.f, -1.f, 1.f);
            float humidityNormalized    = RangeMap(humidity, 0.f, 1.f, -1.f, 1.f);

            // Select biome type using lookup table approach
            BiomeType biomeType = SelectBiome(temperatureNormalized, humidityNormalized,
                                             continentalness, erosion, peaksValleys);

            // Store all biome data
            m_biomeData[idxXY].temperature     = temperatureNormalized;
            m_biomeData[idxXY].humidity        = humidityNormalized;
            m_biomeData[idxXY].continentalness = continentalness;
            m_biomeData[idxXY].erosion         = erosion;
            m_biomeData[idxXY].weirdness       = weirdness;
            m_biomeData[idxXY].peaksValleys    = peaksValleys;
            m_biomeData[idxXY].biomeType       = biomeType;
        }
    }

    // --- Pass 2: assign block types for every (x,y,z) using 3D density formula ---
    // Assignment 4: Phase 2, Task 2.1 - Replace heightmap with 3D density terrain
    // Formula: D(x,y,z) = N(x,y,z,s) + B(z)
    // where N = 3D Perlin noise, B = vertical bias
    // CRITICAL: Negative density = MORE dense (solid blocks), positive = air

    for (int z = 0; z < CHUNK_SIZE_Z; z++)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
        {
            for (int x = 0; x < CHUNK_SIZE_X; x++)
            {
                IntVec3 localCoords(x, y, z);
                IntVec3 globalCoords = GetGlobalCoords(m_chunkCoords, localCoords);
                int     idx          = LocalCoordsToIndex(localCoords);
                int     idxXY        = y * CHUNK_SIZE_X + x;

                // Retrieve cached per-column data from Pass 1
                float temperature   = temperatureMapXY[idxXY]; // Used in ice formation logic

                // --- Assignment 4: 3D Density Formula (Phase 2, Task 2.1) ---

                // Sample 3D noise at this global position
                // CRITICAL: Use global coordinates for cross-chunk consistency
                // Use Compute3dPerlinNoise for smooth continuous interpolated noise
                // Returns [-1, 1], scale controls frequency, octaves add detail
                unsigned int densitySeed = GAME_SEED + 10; // Separate seed for density noise
                float noise = Compute3dPerlinNoise(
                    (float)globalCoords.x,
                    (float)globalCoords.y,
                    (float)globalCoords.z,
                    g_worldGenConfig->density.densityNoiseScale,           // Scale: 200.0 (lower freq = smoother terrain)
                    g_worldGenConfig->density.densityNoiseOctaves,         // Octaves: 3 (adds fractal detail)
                    DEFAULT_OCTAVE_PERSISTANCE,    // Persistence: 0.5 (amplitude falloff)
                    DEFAULT_NOISE_OCTAVE_SCALE,    // Octave scale: 2.0 (frequency multiplier)
                    true,                          // Renormalize to [-1, 1]
                    densitySeed
                ); // Returns N(x,y,z,s) in [-1, 1]

                // Note: Original bias calculation is now unused because we use shaped bias
                // This is kept for reference and potential future modifications
                // float bias = DENSITY_BIAS_PER_BLOCK * (float)(globalCoords.z - (int)DEFAULT_TERRAIN_HEIGHT);

                // --- Assignment 4: Top and Bottom Slides (Phase 2, Task 2.2) ---

                // Top slide: Smooth transition near surface (z=100-120)
                // Forces density toward positive (air) near world top to prevent sharp cutoffs
                float topSlide = 0.0f;
                if (globalCoords.z >= g_worldGenConfig->density.topSlideStart && globalCoords.z <= g_worldGenConfig->density.topSlideEnd)
                {
                    // Calculate slide progress: 0.0 at start, 1.0 at end
                    float slideProgress = (float)(globalCoords.z - g_worldGenConfig->density.topSlideStart) / (float)(g_worldGenConfig->density.topSlideEnd - g_worldGenConfig->density.topSlideStart);

                    // Use SmoothStep3 for smooth transition curve
                    float smoothedProgress = SmoothStep3(slideProgress);

                    // Apply positive bias to force terrain toward air
                    // Strength increases as we approach world top
                    topSlide = smoothedProgress * 2.0f; // Range: 0.0 to 2.0 (positive = more air)
                }

                // Bottom slide: Flatten terrain near bedrock (z=0-20)
                // Forces density toward negative (solid) near world bottom for stable base
                float bottomSlide = 0.0f;
                if (globalCoords.z >= g_worldGenConfig->density.bottomSlideStart && globalCoords.z <= g_worldGenConfig->density.bottomSlideEnd)
                {
                    // Calculate slide progress: 1.0 at bottom, 0.0 at end
                    float slideProgress = 1.0f - ((float)(globalCoords.z - g_worldGenConfig->density.bottomSlideStart) / (float)(g_worldGenConfig->density.bottomSlideEnd - g_worldGenConfig->density.bottomSlideStart));

                    // Use SmoothStep3 for smooth transition curve
                    float smoothedProgress = SmoothStep3(slideProgress);

                    // Apply negative bias to force terrain toward solid
                    // Strength increases as we approach bedrock
                    bottomSlide = -smoothedProgress * 3.0f; // Range: -3.0 to 0.0 (negative = more solid)
                }

                // --- Assignment 4: Terrain Shaping Curves (Phase 2, Task 2.3) ---

                // Retrieve biome data for this column (from Pass 1)
                BiomeData& biomeData = m_biomeData[idxXY];

                // Continentalness Curve: Height offset based on ocean/inland distance
                // Uses PiecewiseCurve1D for non-linear terrain shaping (Assignment 4: Phase 5B.4)
                // Input: C noise [-1.2, 1.0], Output: normalized [-1, 1] from curve
                // Scale output by [continentalnessHeightMin, continentalnessHeightMax]
                // Ocean areas get negative offset (deeper), inland gets positive (higher)
                float continentalnessNormalized = g_worldGenConfig->continentalnessCurve.Evaluate(biomeData.continentalness);
                float continentalnessOffset = RangeMap(continentalnessNormalized,
                                                       -1.0f, 1.0f,
                                                       g_worldGenConfig->curves.continentalnessHeightMin,
                                                       g_worldGenConfig->curves.continentalnessHeightMax);

                // Erosion Curve: Terrain wildness/scale based on flat/mountainous
                // Uses PiecewiseCurve1D for non-linear terrain shaping (Assignment 4: Phase 5B.4)
                // Input: E noise [-1, 1], Output: scale multiplier directly from curve
                // Curve output range: [erosionScaleMin, erosionScaleMax] = [0.3, 2.5]
                // Flat terrain (low E) gets less noise amplification
                // Mountainous (high E) gets more noise amplification
                float erosionScale = g_worldGenConfig->erosionCurve.Evaluate(biomeData.erosion);

                // Peaks & Valleys Curve: Additional height variation
                // Uses PiecewiseCurve1D for non-linear terrain shaping (Assignment 4: Phase 5B.4)
                // Input: PV noise [-1, 1], Output: normalized [-1, 1] from curve
                // Scale output by [pvHeightMin, pvHeightMax]
                // Valleys (low PV) get negative modifier, peaks (high PV) get positive
                float pvNormalized = g_worldGenConfig->peaksValleysCurve.Evaluate(biomeData.peaksValleys);
                float pvOffset = RangeMap(pvNormalized,
                                         -1.0f, 1.0f,
                                         g_worldGenConfig->curves.pvHeightMin,
                                         g_worldGenConfig->curves.pvHeightMax);

                // Apply terrain shaping:
                // 1. Calculate total height offset from biome parameters
                float heightOffset = continentalnessOffset + pvOffset;

                // 2. Calculate effective terrain height for this location
                //    - Start with DEFAULT_TERRAIN_HEIGHT (80)
                //    - Add continentalness and PV height offsets
                float effectiveTerrainHeight = (float)DEFAULT_TERRAIN_HEIGHT + heightOffset;

                // 3. Calculate bias relative to the SHAPED terrain height (not default height)
                //    - This makes terrain follow the biome-specific height curves
                float shapedBias = g_worldGenConfig->density.densityBiasPerBlock * (float)((float)globalCoords.z - effectiveTerrainHeight);

                // 4. Scale noise by erosion factor (controls terrain wildness)
                float shapedNoise = noise * erosionScale;

                // Combine noise, shaped bias, and slides to get final density
                // D(x,y,z) = N(x,y,z,s)*erosionScale + B_shaped(z) + topSlide(z) + bottomSlide(z)
                float density = shapedNoise + shapedBias + topSlide + bottomSlide;

                // Density threshold: negative = solid, positive = air
                bool isSolid = (density < 0.0f);

                // --- Assignment 4: Phase 4, Task 4.1 - Cheese Cave Carving ---
                //
                // Carve large open caverns using 3D noise with large scale (60 units).
                // Cheese caves use high threshold (0.6) to create fewer but bigger caves.
                //
                // Safety checks:
                // 1. Only carve underground (below surface - MIN_CAVE_DEPTH_FROM_SURFACE)
                // 2. Don't carve too close to lava layer (above LAVA_Z + MIN_CAVE_HEIGHT_ABOVE_LAVA)
                // 3. Only carve solid blocks (isSolid == true)
                //
                // Algorithm: If cheeseNoise > CHEESE_THRESHOLD AND depth checks pass:
                //   - Override isSolid to false (convert solid block to air)
                //   - This "carves out" the cavern from the terrain

                if (isSolid) // Only check caves for solid blocks (no need to carve air)
                {
                    // CRITICAL FIX: m_surfaceHeight[] is calculated AFTER this loop
                    // We need to check surface height dynamically by looking upward
                    // Find the surface by checking if the block above has positive density (is air)

                    bool isNearSurface = false;

                    // Check blocks above this position (up to MIN_CAVE_DEPTH_FROM_SURFACE blocks)
                    for (int checkZ = globalCoords.z + 1;
                         checkZ <= globalCoords.z + MIN_CAVE_DEPTH_FROM_SURFACE && checkZ < CHUNK_SIZE_Z;
                         checkZ++)
                    {
                        // Calculate density at check position
                        IntVec3 checkCoords(globalCoords.x, globalCoords.y, checkZ);

                        float checkNoise = Compute3dPerlinNoise(
                            (float)checkCoords.x, (float)checkCoords.y, (float)checkCoords.z,
                            g_worldGenConfig->density.densityNoiseScale, g_worldGenConfig->density.densityNoiseOctaves,
                            DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE,
                            true, GAME_SEED + 10
                        );

                        // Get biome data for shaping
                        BiomeData& checkBiomeData = m_biomeData[idxXY];
                        float checkContinentalnessOffset = RangeMap(checkBiomeData.continentalness, -1.2f, 1.0f,
                                                                    CONTINENTALNESS_HEIGHT_MIN, CONTINENTALNESS_HEIGHT_MAX);
                        float checkErosionScale = RangeMap(checkBiomeData.erosion, -1.0f, 1.0f,
                                                           EROSION_SCALE_MIN, EROSION_SCALE_MAX);
                        float checkPvOffset = RangeMap(checkBiomeData.peaksValleys, -1.0f, 1.0f,
                                                       PV_HEIGHT_MIN, PV_HEIGHT_MAX);

                        float checkEffectiveHeight = (float)DEFAULT_TERRAIN_HEIGHT + checkContinentalnessOffset + checkPvOffset;
                        float checkShapedBias = g_worldGenConfig->density.densityBiasPerBlock * ((float)checkCoords.z - checkEffectiveHeight);
                        float checkDensity = (checkNoise * checkErosionScale) + checkShapedBias;

                        // If we found air above us within MIN_CAVE_DEPTH_FROM_SURFACE, we're too close to surface
                        if (checkDensity >= 0.0f) // Positive density = air
                        {
                            isNearSurface = true;
                            break;
                        }
                    }

                    // Check if we're high enough above lava to carve caves
                    bool highEnough = (globalCoords.z > LAVA_Z + MIN_CAVE_HEIGHT_ABOVE_LAVA);

                    // Only carve if we're NOT near surface AND high enough above lava
                    if (!isNearSurface && highEnough)
                    {
                        // Sample 3D cheese cave noise at this global position
                        // Use different seed from terrain density noise
                        unsigned int cheeseSeed = GAME_SEED + CHEESE_NOISE_SEED_OFFSET;

                        float cheeseNoise = Compute3dPerlinNoise(
                            (float)globalCoords.x,
                            (float)globalCoords.y,
                            (float)globalCoords.z,
                            g_worldGenConfig->caves.cheeseNoiseScale,           // Large scale: 60.0 (big smooth caverns)
                            g_worldGenConfig->caves.cheeseNoiseOctaves,         // Low octaves: 2 (smooth shapes)
                            DEFAULT_OCTAVE_PERSISTANCE,   // 0.5
                            DEFAULT_NOISE_OCTAVE_SCALE,   // 2.0
                            true,                         // Renormalize to [-1, 1]
                            cheeseSeed
                        ); // Returns [-1, 1]

                        // Convert to [0, 1] range for threshold comparison
                        float cheeseValue = (cheeseNoise + 1.0f) * 0.5f; // Maps [-1,1] → [0,1]

                        // Carve cavern if noise exceeds threshold
                        // Higher threshold = fewer/smaller caves
                        if (cheeseValue > g_worldGenConfig->caves.cheeseThreshold)
                        {
                            isSolid = false; // Carve out this block (convert to air)
                        }

                        // --- Assignment 4: Phase 4, Task 4.2 - Spaghetti Cave Carving ---
                        //
                        // Carve winding tunnels using 3D noise with smaller scale (30 units).
                        // Spaghetti caves use even higher threshold (0.65) to create narrow tunnels.
                        //
                        // These combine with cheese caves using OR logic:
                        // - If EITHER cheese OR spaghetti threshold is exceeded, carve the block
                        // - This creates interesting intersections and complex cave networks
                        //
                        // Only check spaghetti if block is still solid (not already carved by cheese)

                        if (isSolid) // Only check if cheese didn't already carve this block
                        {
                            // Sample 3D spaghetti cave noise at this global position
                            // Use different seed from both terrain and cheese noise
                            unsigned int spaghettiSeed = GAME_SEED + SPAGHETTI_NOISE_SEED_OFFSET;

                            float spaghettiNoise = Compute3dPerlinNoise(
                                (float)globalCoords.x,
                                (float)globalCoords.y,
                                (float)globalCoords.z,
                                g_worldGenConfig->caves.spaghettiNoiseScale,        // Smaller scale: 30.0 (winding tunnels)
                                g_worldGenConfig->caves.spaghettiNoiseOctaves,      // More octaves: 3 (complex paths)
                                DEFAULT_OCTAVE_PERSISTANCE,   // 0.5
                                DEFAULT_NOISE_OCTAVE_SCALE,   // 2.0
                                true,                         // Renormalize to [-1, 1]
                                spaghettiSeed
                            ); // Returns [-1, 1]

                            // Convert to [0, 1] range for threshold comparison
                            float spaghettiValue = (spaghettiNoise + 1.0f) * 0.5f; // Maps [-1,1] → [0,1]

                            // Carve tunnel if noise exceeds threshold
                            // Higher threshold = fewer/narrower tunnels
                            if (spaghettiValue > g_worldGenConfig->caves.spaghettiThreshold)
                            {
                                isSolid = false; // Carve out this block (convert to air)
                            }
                        }
                    }
                }

                // --- Assignment 4: Phase 5, Task 5A.1 - Ravine Carver ---
                //
                // Carve dramatic vertical slices through terrain using 2D noise paths.
                // Ravines are VERY RARE (threshold 0.85) but create impressive geological features.
                //
                // Algorithm:
                // 1. Sample 2D ravine path noise at (x,y) to determine if on ravine path
                // 2. If on ravine path, sample secondary width noise for variable width
                // 3. Calculate distance from ravine center (0 = center, 1 = edge)
                // 4. Use distance to determine depth (deeper at center, shallower at edges)
                // 5. Carve vertical slice from surface down to calculated depth

                if (isSolid) // Only carve ravines through solid terrain
                {
                    // Sample 2D ravine path noise at this (x,y) column position
                    unsigned int ravineSeed = GAME_SEED + RAVINE_NOISE_SEED_OFFSET;

                    float ravinePathNoise = Compute2dPerlinNoise(
                        (float)globalCoords.x,
                        (float)globalCoords.y,
                        g_worldGenConfig->carvers.ravinePathNoiseScale,        // Very large scale (800) for rare, long ravines
                        g_worldGenConfig->carvers.ravinePathNoiseOctaves,      // Multiple octaves (3) for natural meandering
                        DEFAULT_OCTAVE_PERSISTANCE,     // 0.5
                        DEFAULT_NOISE_OCTAVE_SCALE,     // 2.0
                        true,                           // Renormalize to [-1, 1]
                        ravineSeed
                    ); // Returns [-1, 1]

                    // Convert to [0, 1] range for threshold comparison
                    float ravinePathValue = (ravinePathNoise + 1.0f) * 0.5f; // Maps [-1,1] → [0,1]

                    // Check if this (x,y) position is on a ravine path
                    // Very high threshold (0.85) means ravines are rare (1-2 per ~10 chunks)
                    if (ravinePathValue > g_worldGenConfig->carvers.ravinePathThreshold)
                    {
                        // We're on a ravine path! Now determine width and depth

                        // Sample secondary width noise for variable ravine width (3-7 blocks)
                        unsigned int widthSeed = ravineSeed + 10; // Offset seed for width variation

                        float widthNoise = Compute2dPerlinNoise(
                            (float)globalCoords.x,
                            (float)globalCoords.y,
                            g_worldGenConfig->carvers.ravineWidthNoiseScale,       // Smaller scale (50) for local variation
                            g_worldGenConfig->carvers.ravineWidthNoiseOctaves,     // Low octaves (2) for smooth changes
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            widthSeed
                        ); // Returns [-1, 1]

                        // Convert width noise to [0, 1] and map to width range [3, 7]
                        float widthNoiseNormalized = (widthNoise + 1.0f) * 0.5f;
                        int ravineFullWidth = (int)(g_worldGenConfig->carvers.ravineWidthMin + widthNoiseNormalized * (g_worldGenConfig->carvers.ravineWidthMax - g_worldGenConfig->carvers.ravineWidthMin));
                        int ravineHalfWidth = ravineFullWidth / 2;

                        // Option A: Radius-based carving with center finding
                        // Sample nearby positions to find ravine center (highest pathValue = center)
                        float maxPathValue = ravinePathValue;
                        int centerOffsetX = 0;
                        int centerOffsetY = 0;

                        // Sample in a small radius to find local maximum (ravine center)
                        int searchRadius = ravineHalfWidth + 2; // Search slightly beyond half-width
                        for (int dy = -searchRadius; dy <= searchRadius; ++dy)
                        {
                            for (int dx = -searchRadius; dx <= searchRadius; ++dx)
                            {
                                if (dx == 0 && dy == 0) continue; // Skip current position

                                float samplePathNoise = Compute2dPerlinNoise(
                                    (float)(globalCoords.x + dx),
                                    (float)(globalCoords.y + dy),
                                    g_worldGenConfig->carvers.ravinePathNoiseScale,
                                    g_worldGenConfig->carvers.ravinePathNoiseOctaves,
                                    DEFAULT_OCTAVE_PERSISTANCE,
                                    DEFAULT_NOISE_OCTAVE_SCALE,
                                    true,
                                    ravineSeed
                                );
                                float samplePathValue = (samplePathNoise + 1.0f) * 0.5f;

                                if (samplePathValue > maxPathValue)
                                {
                                    maxPathValue = samplePathValue;
                                    centerOffsetX = dx;
                                    centerOffsetY = dy;
                                }
                            }
                        }

                        // Calculate distance from current position to estimated ravine center
                        float distanceToCenter = sqrtf((float)(centerOffsetX * centerOffsetX + centerOffsetY * centerOffsetY));

                        // Only carve if within ravine half-width radius
                        if (distanceToCenter <= (float)ravineHalfWidth)
                        {
                            // Calculate falloff based on distance from center
                            float radialFalloff = 1.0f - (distanceToCenter / (float)ravineHalfWidth);
                            radialFalloff = GetClamped(radialFalloff, 0.0f, 1.0f);

                            // Apply edge falloff for smooth transitions
                            float depthMultiplier = radialFalloff * (1.0f - g_worldGenConfig->carvers.ravineEdgeFalloff) + g_worldGenConfig->carvers.ravineEdgeFalloff;

                            // Calculate ravine depth: deeper at center, shallower at edges
                            int ravineDepth = (int)(g_worldGenConfig->carvers.ravineDepthMin + depthMultiplier * (g_worldGenConfig->carvers.ravineDepthMax - g_worldGenConfig->carvers.ravineDepthMin));

                            // Estimate surface height using biome parameters
                            BiomeData& ravBiomeData = m_biomeData[idxXY];
                            float ravContinentalnessOffset = RangeMap(ravBiomeData.continentalness, -1.2f, 1.0f,
                                                                      CONTINENTALNESS_HEIGHT_MIN, CONTINENTALNESS_HEIGHT_MAX);
                            float ravPvOffset = RangeMap(ravBiomeData.peaksValleys, -1.0f, 1.0f,
                                                         PV_HEIGHT_MIN, PV_HEIGHT_MAX);
                            float estimatedSurfaceHeight = (float)DEFAULT_TERRAIN_HEIGHT + ravContinentalnessOffset + ravPvOffset;

                            // Calculate ravine bottom Z coordinate
                            int ravineBottomZ = (int)(estimatedSurfaceHeight - ravineDepth);
                            ravineBottomZ = (std::max)(ravineBottomZ, LAVA_Z + 1); // Don't carve below lava

                            // Carve this block if we're between surface and ravine bottom
                            if (globalCoords.z >= ravineBottomZ && globalCoords.z <= (int)estimatedSurfaceHeight)
                            {
                                isSolid = false; // Carve out this block (convert to air)
                            }
                        }
                    }
                }

                // --- Block Type Assignment ---

                uint8_t blockType = BLOCK_AIR; // Default to air
                bool isRiverWater = false;      // Track if this block should be river water

                // --- Assignment 4: Phase 5, Task 5A.2 - River Carver ---
                //
                // Carve shallow water channels through terrain using 2D noise paths.
                // Rivers are more common than ravines (threshold 0.70) and fill with water.
                //
                // Algorithm:
                // 1. Sample 2D river path noise at (x,y) to determine if on river path
                // 2. If on river path, sample secondary width noise for variable width
                // 3. Calculate shallow channel depth (3-8 blocks from surface)
                // 4. Carve channel and fill with water
                // 5. Use sand/gravel for riverbed

                if (isSolid) // Only carve rivers through solid terrain
                {
                    // Sample 2D river path noise at this (x,y) column position
                    unsigned int riverSeed = GAME_SEED + RIVER_NOISE_SEED_OFFSET;

                    float riverPathNoise = Compute2dPerlinNoise(
                        (float)globalCoords.x,
                        (float)globalCoords.y,
                        g_worldGenConfig->carvers.riverPathNoiseScale,         // Large scale (600) for long, winding rivers
                        g_worldGenConfig->carvers.riverPathNoiseOctaves,       // Multiple octaves (3) for natural meandering
                        DEFAULT_OCTAVE_PERSISTANCE,     // 0.5
                        DEFAULT_NOISE_OCTAVE_SCALE,     // 2.0
                        true,                           // Renormalize to [-1, 1]
                        riverSeed
                    ); // Returns [-1, 1]

                    // Convert to [0, 1] range for threshold comparison
                    float riverPathValue = (riverPathNoise + 1.0f) * 0.5f; // Maps [-1,1] → [0,1]

                    // Check if this (x,y) position is on a river path
                    // Moderate threshold (0.70) means rivers are more common than ravines
                    if (riverPathValue > g_worldGenConfig->carvers.riverPathThreshold)
                    {
                        // We're on a river path! Now determine width and depth

                        // Sample secondary width noise for variable river width (5-12 blocks)
                        unsigned int widthSeed = riverSeed + 10; // Offset seed for width variation

                        float widthNoise = Compute2dPerlinNoise(
                            (float)globalCoords.x,
                            (float)globalCoords.y,
                            g_worldGenConfig->carvers.riverWidthNoiseScale,        // Smaller scale (40) for local variation
                            g_worldGenConfig->carvers.riverWidthNoiseOctaves,      // Low octaves (2) for smooth changes
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            widthSeed
                        ); // Returns [-1, 1]

                        // Convert width noise to [0, 1] and map to width range [5, 12]
                        float widthNoiseNormalized = (widthNoise + 1.0f) * 0.5f;
                        int riverFullWidth = (int)(g_worldGenConfig->carvers.riverWidthMin + widthNoiseNormalized * (g_worldGenConfig->carvers.riverWidthMax - g_worldGenConfig->carvers.riverWidthMin));
                        int riverHalfWidth = riverFullWidth / 2;

                        // Option A: Radius-based carving with center finding
                        // Sample nearby positions to find river center (highest pathValue = center)
                        float maxPathValue = riverPathValue;
                        int centerOffsetX = 0;
                        int centerOffsetY = 0;

                        // Sample in a small radius to find local maximum (river center)
                        int searchRadius = riverHalfWidth + 2; // Search slightly beyond half-width
                        for (int dy = -searchRadius; dy <= searchRadius; ++dy)
                        {
                            for (int dx = -searchRadius; dx <= searchRadius; ++dx)
                            {
                                if (dx == 0 && dy == 0) continue; // Skip current position

                                float samplePathNoise = Compute2dPerlinNoise(
                                    (float)(globalCoords.x + dx),
                                    (float)(globalCoords.y + dy),
                                    g_worldGenConfig->carvers.riverPathNoiseScale,
                                    g_worldGenConfig->carvers.riverPathNoiseOctaves,
                                    DEFAULT_OCTAVE_PERSISTANCE,
                                    DEFAULT_NOISE_OCTAVE_SCALE,
                                    true,
                                    riverSeed
                                );
                                float samplePathValue = (samplePathNoise + 1.0f) * 0.5f;

                                if (samplePathValue > maxPathValue)
                                {
                                    maxPathValue = samplePathValue;
                                    centerOffsetX = dx;
                                    centerOffsetY = dy;
                                }
                            }
                        }

                        // Calculate distance from current position to estimated river center
                        float distanceToCenter = sqrtf((float)(centerOffsetX * centerOffsetX + centerOffsetY * centerOffsetY));

                        // Only carve if within river half-width radius
                        if (distanceToCenter <= (float)riverHalfWidth)
                        {
                            // Calculate falloff based on distance from center
                            float radialFalloff = 1.0f - (distanceToCenter / (float)riverHalfWidth);
                            radialFalloff = GetClamped(radialFalloff, 0.0f, 1.0f);

                            // Apply edge falloff for smooth transitions
                            float depthMultiplier = radialFalloff * (1.0f - g_worldGenConfig->carvers.riverEdgeFalloff) + g_worldGenConfig->carvers.riverEdgeFalloff;

                            // Calculate river depth: shallow channels (3-8 blocks from surface)
                            int riverDepth = (int)(g_worldGenConfig->carvers.riverDepthMin + depthMultiplier * (g_worldGenConfig->carvers.riverDepthMax - g_worldGenConfig->carvers.riverDepthMin));

                            // Estimate surface height using biome parameters
                            BiomeData& rivBiomeData = m_biomeData[idxXY];
                            float rivContinentalnessOffset = RangeMap(rivBiomeData.continentalness, -1.2f, 1.0f,
                                                                      CONTINENTALNESS_HEIGHT_MIN, CONTINENTALNESS_HEIGHT_MAX);
                            float rivPvOffset = RangeMap(rivBiomeData.peaksValleys, -1.0f, 1.0f,
                                                         PV_HEIGHT_MIN, PV_HEIGHT_MAX);
                            float estimatedSurfaceHeight = (float)DEFAULT_TERRAIN_HEIGHT + rivContinentalnessOffset + rivPvOffset;

                            // Calculate river bottom Z coordinate
                            int riverBottomZ = (int)(estimatedSurfaceHeight - riverDepth);
                            riverBottomZ = (std::max)(riverBottomZ, SEA_LEVEL_Z - 5); // Don't carve too far below sea level

                            // Determine if this block should be carved out or filled with water
                            if (globalCoords.z >= riverBottomZ && globalCoords.z <= (int)estimatedSurfaceHeight)
                            {
                                // Carve the channel
                                isSolid = false;

                                // Fill with water above the riverbed
                                // Leave bottom 1-2 blocks as carved terrain (will become sand/gravel in block assignment)
                                if (globalCoords.z > riverBottomZ + 1)
                                {
                                    // This block should be water
                                    blockType = BLOCK_WATER;
                                    isRiverWater = true;
                                }
                                else
                                {
                                    // Riverbed - will be assigned sand or gravel later
                                    blockType = BLOCK_SAND; // Default riverbed material
                                    isRiverWater = false;
                                }
                            }
                        }
                    }
                }

                if (isSolid && !isRiverWater)
                {
                    // --- Solid terrain blocks ---

                    // Special bottom layers (always placed regardless of density)
                    if (globalCoords.z == OBSIDIAN_Z)
                    {
                        blockType = BLOCK_OBSIDIAN;
                    }
                    else if (globalCoords.z == LAVA_Z)
                    {
                        blockType = BLOCK_LAVA;
                    }
                    else
                    {
                        // Default solid block is stone, with ore generation
                        float oreNoise = Get3dNoiseZeroToOne(globalCoords.x, globalCoords.y, globalCoords.z, GAME_SEED);
                        if (oreNoise < DIAMOND_CHANCE)
                        {
                            blockType = BLOCK_DIAMOND;
                        }
                        else if (oreNoise < GOLD_CHANCE)
                        {
                            blockType = BLOCK_GOLD;
                        }
                        else if (oreNoise < IRON_CHANCE)
                        {
                            blockType = BLOCK_IRON;
                        }
                        else if (oreNoise < COAL_CHANCE)
                        {
                            blockType = BLOCK_COAL;
                        }
                        else
                        {
                            blockType = BLOCK_STONE;

                            // --- Assignment 4: Phase 2, Task 2.4 - Surface Block Replacement ---
                            // Check if this block is at the surface (has air above it)
                            bool isSurface = false;
                            if (globalCoords.z < CHUNK_SIZE_Z - 1) // Not at world top
                            {
                                // Calculate density of block above this one
                                IntVec3 aboveCoords(globalCoords.x, globalCoords.y, globalCoords.z + 1);
                                unsigned int aboveDensitySeed = GAME_SEED + 10;
                                float aboveNoise = Compute3dPerlinNoise(
                                    (float)aboveCoords.x,
                                    (float)aboveCoords.y,
                                    (float)aboveCoords.z,
                                    g_worldGenConfig->density.densityNoiseScale,
                                    g_worldGenConfig->density.densityNoiseOctaves,
                                    DEFAULT_OCTAVE_PERSISTANCE,
                                    DEFAULT_NOISE_OCTAVE_SCALE,
                                    true,
                                    aboveDensitySeed
                                );

                                // Get biome data for this column (same as current block)
                                BiomeData& aboveBiomeData = m_biomeData[idxXY];

                                // Apply terrain shaping to above block
                                float aboveContinentalnessOffset = RangeMap(aboveBiomeData.continentalness,
                                                                           -1.2f, 1.0f,
                                                                           CONTINENTALNESS_HEIGHT_MIN, CONTINENTALNESS_HEIGHT_MAX);
                                float aboveErosionScale = RangeMap(aboveBiomeData.erosion,
                                                                   -1.0f, 1.0f,
                                                                   EROSION_SCALE_MIN, EROSION_SCALE_MAX);
                                float abovePvOffset = RangeMap(aboveBiomeData.peaksValleys,
                                                               -1.0f, 1.0f,
                                                               PV_HEIGHT_MIN, PV_HEIGHT_MAX);

                                // Calculate effective terrain height for above block (same as current block)
                                float aboveHeightOffset = aboveContinentalnessOffset + abovePvOffset;
                                float aboveEffectiveTerrainHeight = (float)DEFAULT_TERRAIN_HEIGHT + aboveHeightOffset;

                                // Calculate shaped bias relative to effective terrain height
                                float aboveShapedBias = g_worldGenConfig->density.densityBiasPerBlock * (float)((float)aboveCoords.z - aboveEffectiveTerrainHeight);

                                // Scale noise by erosion factor
                                float aboveShapedNoise = aboveNoise * aboveErosionScale;

                                // Apply slides to above block
                                float aboveTopSlide = 0.0f;
                                if (aboveCoords.z >= g_worldGenConfig->density.topSlideStart && aboveCoords.z <= g_worldGenConfig->density.topSlideEnd)
                                {
                                    float slideProgress = (float)(aboveCoords.z - g_worldGenConfig->density.topSlideStart) / (float)(g_worldGenConfig->density.topSlideEnd - g_worldGenConfig->density.topSlideStart);
                                    float smoothedProgress = SmoothStep3(slideProgress);
                                    aboveTopSlide = smoothedProgress * 2.0f;
                                }

                                float aboveBottomSlide = 0.0f;
                                if (aboveCoords.z >= g_worldGenConfig->density.bottomSlideStart && aboveCoords.z <= g_worldGenConfig->density.bottomSlideEnd)
                                {
                                    float slideProgress = 1.0f - ((float)(aboveCoords.z - g_worldGenConfig->density.bottomSlideStart) / (float)(g_worldGenConfig->density.bottomSlideEnd - g_worldGenConfig->density.bottomSlideStart));
                                    float smoothedProgress = SmoothStep3(slideProgress);
                                    aboveBottomSlide = -smoothedProgress * 3.0f;
                                }

                                // Combine components for above block density
                                float aboveDensity = aboveShapedNoise + aboveShapedBias + aboveTopSlide + aboveBottomSlide;
                                isSurface = (aboveDensity >= 0.0f); // Above block is air/water
                            }

                            // Apply biome-specific surface blocks
                            if (isSurface)
                            {
                                BiomeType biome = m_biomeData[idxXY].biomeType;

                                switch (biome)
                                {
                                    // Ocean biomes - sandy ocean floor or gravel
                                    case BiomeType::OCEAN:
                                    case BiomeType::DEEP_OCEAN:
                                    case BiomeType::FROZEN_OCEAN:
                                        blockType = BLOCK_SAND; // Sandy ocean floor
                                        break;

                                    // Beach biomes - sand
                                    case BiomeType::BEACH:
                                    case BiomeType::SNOWY_BEACH:
                                        blockType = BLOCK_SAND;
                                        break;

                                    // Desert biome - sand with possible dirt underneath
                                    case BiomeType::DESERT:
                                        blockType = BLOCK_SAND;
                                        break;

                                    // Savanna biome - dirt with grassy surface
                                    case BiomeType::SAVANNA:
                                        blockType = BLOCK_GRASS_YELLOW; // Dry yellow grass
                                        break;

                                    // Plains biomes - standard grass
                                    case BiomeType::PLAINS:
                                        blockType = BLOCK_GRASS;
                                        break;

                                    // Snowy biomes - snow or ice blocks
                                    case BiomeType::SNOWY_PLAINS:
                                        blockType = BLOCK_SNOW; // Snow surface
                                        break;

                                    // Forest biomes - grass with different shades
                                    case BiomeType::FOREST:
                                        blockType = BLOCK_GRASS_DARK; // Dark green grass
                                        break;

                                    case BiomeType::JUNGLE:
                                        blockType = BLOCK_GRASS_LIGHT; // Light tropical grass
                                        break;

                                    // Taiga biomes - grass with snow possibilities
                                    case BiomeType::TAIGA:
                                        blockType = BLOCK_GRASS; // Standard grass
                                        break;

                                    case BiomeType::SNOWY_TAIGA:
                                        blockType = BLOCK_SNOW; // Snow-covered grass
                                        break;

                                    // Mountain peaks - stone or snow
                                    case BiomeType::STONY_PEAKS:
                                        blockType = BLOCK_COBBLESTONE; // Rocky mountain surface
                                        break;

                                    case BiomeType::SNOWY_PEAKS:
                                        blockType = BLOCK_SNOW; // Snowy mountain peaks
                                        break;

                                    default:
                                        blockType = BLOCK_GRASS; // Default to grass
                                        break;
                                }

                                // Special temperature-based surface modifications
                                float surfaceTemp = m_biomeData[idxXY].temperature;
                                if (surfaceTemp <= ICE_TEMPERATURE_MAX && globalCoords.z <= ICE_DEPTH_MAX)
                                {
                                    // Very cold areas get ice formation near surface
                                    if (blockType == BLOCK_WATER || (blockType == BLOCK_SAND && globalCoords.z < SEA_LEVEL_Z))
                                    {
                                        blockType = BLOCK_ICE;
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    // --- Air/fluid blocks ---

                    // Fill below sea level with water
                    if (globalCoords.z < SEA_LEVEL_Z)
                    {
                        blockType = BLOCK_WATER;

                        // Temperature-driven ice ceiling
                        float iceDepth = DEFAULT_TERRAIN_HEIGHT - floorf(RangeMapClamped(temperature,
                                                                                         ICE_TEMPERATURE_MAX, ICE_TEMPERATURE_MIN,
                                                                                         ICE_DEPTH_MIN, ICE_DEPTH_MAX));
                        if (temperature < 0.38f && globalCoords.z > iceDepth)
                        {
                            blockType = BLOCK_ICE;
                        }
                    }
                    // else: remains BLOCK_AIR
                }

                m_blocks[idx].m_typeIndex = blockType;
            }
        }
    }

    // --- Assignment 4: Find Surface Blocks (Phase 3, Task 3A.1) ---
    //
    // After terrain generation is complete, identify the top solid block in each (x,y) column.
    // This surface height map is used for:
    // - Task 3A.2: Biome-based surface block replacement
    // - Task 3B.2: Tree placement logic (trees need to know ground level)
    //
    // Algorithm: For each column, scan from top (z=CHUNK_SIZE_Z-1) down to bottom (z=0)
    // and find the first solid block (non-air, non-water).
    //
    // Performance: O(CHUNK_SIZE_X × CHUNK_SIZE_Y × CHUNK_SIZE_Z) worst case
    // In practice: O(CHUNK_SIZE_X × CHUNK_SIZE_Y × average_height) ≈ 32×32×80 = 81,920 checks

    for (int y = 0; y < CHUNK_SIZE_Y; y++)
    {
        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            int columnIdx = x + y * CHUNK_SIZE_X;

            // Find surface: scan from top down for first solid block
            int surfaceZ = -1; // -1 indicates no solid surface found (all air/water column)

            for (int searchZ = CHUNK_SIZE_Z - 1; searchZ >= 0; searchZ--)
            {
                int blockIdx = x + y * CHUNK_SIZE_X + searchZ * CHUNK_SIZE_X * CHUNK_SIZE_Y;
                uint8_t blockType = m_blocks[blockIdx].m_typeIndex;

                // Check if this block is solid (not air or water)
                // Surface blocks are the top-most solid blocks that can have grass, sand, etc.
                if (blockType != BLOCK_AIR && blockType != BLOCK_WATER)
                {
                    surfaceZ = searchZ;
                    break; // Found surface, stop scanning this column
                }
            }

            m_surfaceHeight[columnIdx] = surfaceZ;
        }
    }

    // --- Assignment 4: Biome-Based Surface (Phase 3, Task 3A.2) ---
    //
    // After finding surface blocks, replace them with biome-appropriate materials
    // and add subsurface layers (dirt beneath grass, sand beneath beaches, etc.)
    //
    // This enhances the Phase 2 Task 2.4 basic surface replacement with:
    // - Multi-layer subsurface blocks (3-4 dirt layers standard, 5+ sand for deserts)
    // - "No dirt layers" rule for specified biomes (Deep Ocean, Frozen Ocean, Stony Peaks, Snowy Peaks)
    // - Temperature-based variations (Deep Ocean uses temperature for sand/snow choice)
    //
    // Performance: O(CHUNK_SIZE_X × CHUNK_SIZE_Y × subsurface_depth) ≈ 32×32×4 = 4,096 replacements

    for (int y = 0; y < CHUNK_SIZE_Y; y++)
    {
        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            int columnIdx = x + y * CHUNK_SIZE_X;
            int surfaceZ = m_surfaceHeight[columnIdx];

            // Skip columns with no surface (all air/water)
            if (surfaceZ < 0) continue;

            // Get biome type for this column
            BiomeType biome = m_biomeData[columnIdx].biomeType;
            float temperature = m_biomeData[columnIdx].temperature;

            // Determine surface and subsurface block types based on biome
            uint8_t surfaceBlock = BLOCK_GRASS;  // Default
            uint8_t subsurfaceBlock = BLOCK_DIRT; // Default
            int subsurfaceDepth = 3;  // Default 3-4 layers
            bool hasSubsurfaceLayers = true;

            switch (biome)
            {
                // --- Ocean Biomes ---
                case BiomeType::OCEAN:
                    surfaceBlock = BLOCK_SAND;  // Sandy ocean floor
                    subsurfaceBlock = BLOCK_DIRT;
                    subsurfaceDepth = 3;
                    hasSubsurfaceLayers = true;
                    break;

                case BiomeType::DEEP_OCEAN:
                    // Temperature-based surface: cold = snow, warm = sand
                    surfaceBlock = (temperature < 0.0f) ? BLOCK_SNOW : BLOCK_SAND;
                    hasSubsurfaceLayers = false;  // "No dirt layers" specification
                    break;

                case BiomeType::FROZEN_OCEAN:
                    surfaceBlock = BLOCK_SNOW;
                    hasSubsurfaceLayers = false;  // "No dirt layers" specification
                    break;

                // --- Beach Biomes ---
                case BiomeType::BEACH:
                    surfaceBlock = BLOCK_SAND;
                    subsurfaceBlock = BLOCK_SAND;  // Sand extends below
                    subsurfaceDepth = 4;
                    hasSubsurfaceLayers = true;
                    break;

                case BiomeType::SNOWY_BEACH:
                    surfaceBlock = BLOCK_SNOW;
                    subsurfaceBlock = BLOCK_SAND;  // Sand beneath snow
                    subsurfaceDepth = 3;
                    hasSubsurfaceLayers = true;
                    break;

                // --- Dry Biomes ---
                case BiomeType::DESERT:
                    surfaceBlock = BLOCK_SAND;
                    subsurfaceBlock = BLOCK_SAND;  // "Several sand layers" before dirt
                    subsurfaceDepth = 5;  // 5+ sand layers
                    hasSubsurfaceLayers = true;
                    break;

                case BiomeType::SAVANNA:
                    surfaceBlock = BLOCK_GRASS_YELLOW;  // Dry yellow grass
                    subsurfaceBlock = BLOCK_DIRT;
                    subsurfaceDepth = 3;
                    hasSubsurfaceLayers = true;
                    break;

                // --- Plains Biomes ---
                case BiomeType::PLAINS:
                    surfaceBlock = BLOCK_GRASS;  // Light grass variant
                    subsurfaceBlock = BLOCK_DIRT;
                    subsurfaceDepth = 3;
                    hasSubsurfaceLayers = true;
                    break;

                case BiomeType::SNOWY_PLAINS:
                    surfaceBlock = BLOCK_SNOW;
                    subsurfaceBlock = BLOCK_DIRT;
                    subsurfaceDepth = 3;
                    hasSubsurfaceLayers = true;
                    break;

                // --- Forest Biomes ---
                case BiomeType::FOREST:
                    surfaceBlock = BLOCK_GRASS_DARK;  // Dark green grass
                    subsurfaceBlock = BLOCK_DIRT;
                    subsurfaceDepth = 4;
                    hasSubsurfaceLayers = true;
                    break;

                case BiomeType::JUNGLE:
                    surfaceBlock = BLOCK_GRASS_LIGHT;  // Light tropical grass
                    subsurfaceBlock = BLOCK_DIRT;
                    subsurfaceDepth = 4;
                    hasSubsurfaceLayers = true;
                    break;

                case BiomeType::TAIGA:
                    surfaceBlock = BLOCK_GRASS;  // Standard grass
                    subsurfaceBlock = BLOCK_DIRT;
                    subsurfaceDepth = 3;
                    hasSubsurfaceLayers = true;
                    break;

                case BiomeType::SNOWY_TAIGA:
                    surfaceBlock = BLOCK_SNOW;
                    subsurfaceBlock = BLOCK_DIRT;
                    subsurfaceDepth = 3;
                    hasSubsurfaceLayers = true;
                    break;

                // --- Mountain Biomes ---
                case BiomeType::STONY_PEAKS:
                    surfaceBlock = BLOCK_STONE;  // Rocky mountain surface
                    hasSubsurfaceLayers = false;  // "No dirt layers" specification
                    break;

                case BiomeType::SNOWY_PEAKS:
                    surfaceBlock = BLOCK_SNOW;
                    hasSubsurfaceLayers = false;  // "No dirt layers" specification
                    break;

                default:
                    surfaceBlock = BLOCK_GRASS;
                    subsurfaceBlock = BLOCK_DIRT;
                    subsurfaceDepth = 3;
                    hasSubsurfaceLayers = true;
                    break;
            }

            // Apply surface block replacement
            int surfaceBlockIdx = x + y * CHUNK_SIZE_X + surfaceZ * CHUNK_SIZE_X * CHUNK_SIZE_Y;
            m_blocks[surfaceBlockIdx].m_typeIndex = surfaceBlock;

            // Apply subsurface layers (if biome allows)
            if (hasSubsurfaceLayers)
            {
                for (int depth = 1; depth <= subsurfaceDepth; depth++)
                {
                    int subsurfaceZ = surfaceZ - depth;

                    // Stop if we hit bedrock or run out of blocks
                    if (subsurfaceZ < 0) break;

                    int subsurfaceBlockIdx = x + y * CHUNK_SIZE_X + subsurfaceZ * CHUNK_SIZE_X * CHUNK_SIZE_Y;

                    // Only replace if this block is currently stone (from density generation)
                    // Don't replace ore blocks or other special blocks
                    if (m_blocks[subsurfaceBlockIdx].m_typeIndex == BLOCK_STONE)
                    {
                        m_blocks[subsurfaceBlockIdx].m_typeIndex = subsurfaceBlock;
                    }
                }

                // After subsurface layers, remaining stone continues as-is
                // This creates natural transitions: grass → dirt → dirt → dirt → stone
            }
        }
    }

    // --- Assignment 4: Tree Placement (Phase 3, Task 3B.2) ---
    //
    // After surface generation and subsurface layers, place trees based on:
    // 1. Biome type (each biome has specific tree types)
    // 2. Noise-based sampling (creates natural scattered distribution)
    // 3. Surface suitability (only place on grass/dirt/sand)
    // 4. Cross-chunk support (trees can extend into neighboring chunks)
    //
    // Tree placement algorithm:
    // - Sample tree noise at each surface location
    // - If noise exceeds threshold AND biome allows trees:
    //   - Select appropriate tree stamp for biome
    //   - Check if tree extends beyond chunk bounds
    //   - Store cross-chunk data if needed
    //   - Place tree portion that fits within current chunk

    // Cross-Chunk Enhancement: Reduced edge safety margin from 4 to 1 block
    // This allows trees much closer to chunk edges while preventing out-of-bounds access
    int constexpr CROSS_CHUNK_SAFETY_MARGIN = 1;

    // DEBUG: Count biomes in this chunk (focus on tree-bearing biomes only)
    {
        int desertCount = 0, snowyPlainsCount = 0, snowyTaigaCount = 0;
        for (int i = 0; i < CHUNK_SIZE_X * CHUNK_SIZE_Y; i++)
        {
            BiomeType b = m_biomeData[i].biomeType;
            if (b == BiomeType::DESERT) desertCount++;
            else if (b == BiomeType::SNOWY_PLAINS) snowyPlainsCount++;
            else if (b == BiomeType::SNOWY_TAIGA) snowyTaigaCount++;
        }
    }

    for (int y = CROSS_CHUNK_SAFETY_MARGIN; y < CHUNK_SIZE_Y - CROSS_CHUNK_SAFETY_MARGIN; y++)
    {
        for (int x = CROSS_CHUNK_SAFETY_MARGIN; x < CHUNK_SIZE_X - CROSS_CHUNK_SAFETY_MARGIN; x++)
        {
            int columnIdx = x + y * CHUNK_SIZE_X;
            int surfaceZ = m_surfaceHeight[columnIdx];

            // Get biome type for this column
            BiomeType biome = m_biomeData[columnIdx].biomeType;

            // Skip if no surface found in this column
            if (surfaceZ < 0) continue;

            // Don't place trees underwater (below sea level)
            // This prevents trees from spawning on ocean floors or submerged beaches
            // EXCEPTION: Allow desert cactus and snow trees even underwater for better visibility
            if (surfaceZ < SEA_LEVEL_Z &&
                biome != BiomeType::DESERT &&
                biome != BiomeType::SNOWY_PLAINS &&
                biome != BiomeType::SNOWY_TAIGA) continue;

            // Check if this biome should have trees
            TreeStamp* treeStamp = nullptr;

            switch (biome)
            {
                case BiomeType::PLAINS:
                    treeStamp = &g_oakTreeSmall;  // Oak trees in plains
                    break;

                case BiomeType::FOREST:
                    treeStamp = &g_oakTreeSmall;  // Oak trees in forests
                    break;

                case BiomeType::TAIGA:
                    treeStamp = &g_spruceTreeSmall;  // Regular spruce in taiga
                    break;

                case BiomeType::SNOWY_TAIGA:
                    treeStamp = &g_snowySpruceTree;  // Snowy spruce in snowy taiga
                    break;

                case BiomeType::JUNGLE:
                    treeStamp = &g_jungleTreeBush;  // Jungle bushes in jungle
                    break;

                case BiomeType::DESERT:
                    treeStamp = &g_cactus;  // Cactus in desert
                    break;

                case BiomeType::SAVANNA:
                    treeStamp = &g_acaciaTree;  // Acacia in savanna
                    break;

                case BiomeType::SNOWY_PLAINS:
                    treeStamp = &g_snowySpruceTree;  // Snowy spruce in snowy plains
                    break;

                // Other biomes don't have trees
                case BiomeType::OCEAN:
                case BiomeType::DEEP_OCEAN:
                case BiomeType::FROZEN_OCEAN:
                case BiomeType::BEACH:
                case BiomeType::SNOWY_BEACH:
                case BiomeType::STONY_PEAKS:
                case BiomeType::SNOWY_PEAKS:
                default:
                    continue;  // No trees in these biomes
            }

            // Check if tree stamp is valid
            if (treeStamp == nullptr) continue;

            // Calculate global coordinates for tree noise sampling
            int globalX = m_chunkCoords.x * CHUNK_SIZE_X + x;
            int globalY = m_chunkCoords.y * CHUNK_SIZE_Y + y;

            // DEBUG: Log ALL biome types to understand distribution
            const char* biomeName = "UNKNOWN";
            switch (biome) {
                case BiomeType::OCEAN: biomeName = "OCEAN"; break;
                case BiomeType::DEEP_OCEAN: biomeName = "DEEP_OCEAN"; break;
                case BiomeType::FROZEN_OCEAN: biomeName = "FROZEN_OCEAN"; break;
                case BiomeType::BEACH: biomeName = "BEACH"; break;
                case BiomeType::SNOWY_BEACH: biomeName = "SNOWY_BEACH"; break;
                case BiomeType::DESERT: biomeName = "DESERT"; break;
                case BiomeType::SAVANNA: biomeName = "SAVANNA"; break;
                case BiomeType::PLAINS: biomeName = "PLAINS"; break;
                case BiomeType::SNOWY_PLAINS: biomeName = "SNOWY_PLAINS"; break;
                case BiomeType::FOREST: biomeName = "FOREST"; break;
                case BiomeType::JUNGLE: biomeName = "JUNGLE"; break;
                case BiomeType::TAIGA: biomeName = "TAIGA"; break;
                case BiomeType::SNOWY_TAIGA: biomeName = "SNOWY_TAIGA"; break;
                case BiomeType::STONY_PEAKS: biomeName = "STONY_PEAKS"; break;
                case BiomeType::SNOWY_PEAKS: biomeName = "SNOWY_PEAKS"; break;
            }

            // Sample tree noise at this location
            // Use deterministic seed based on GAME_SEED for consistent trees
            unsigned int treeSeed = GAME_SEED + 12345;  // Different seed from terrain noise
            float treeNoise = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                g_worldGenConfig->trees.treeNoiseScale,
                g_worldGenConfig->trees.treeNoiseOctaves,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize to -1..1
                treeSeed
            );

            // Convert noise from [-1, 1] to [0, 1] for threshold comparison
            float treeNoise01 = (treeNoise + 1.0f) * 0.5f;

            // Assignment 5 Phase 12: Tree Height/Radius Variation using Perlin noise
            // Generate height variation noise (scale 0.05, 3 octaves)
            unsigned int heightNoiseSeed = GAME_SEED + 54321;
            float heightNoise = Compute2dPerlinNoise(
                (float)globalX * 0.05f, (float)globalY * 0.05f,
                1.0f,  // scale
                3,     // octaves
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize to -1..1
                heightNoiseSeed
            );
            // Normalize to [0, 1] for range mapping
            float heightNoise01 = (heightNoise + 1.0f) * 0.5f;

            // Generate radius variation noise (scale 0.07, different channel via offset)
            unsigned int radiusNoiseSeed = GAME_SEED + 98765;
            float radiusNoise = Compute2dPerlinNoise(
                (float)globalX * 0.07f + 1000.0f, (float)globalY * 0.07f + 1000.0f,
                1.0f,  // scale
                3,     // octaves
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize to -1..1
                radiusNoiseSeed
            );
            // Normalize to [0, 1] for range mapping
            float radiusNoise01 = (radiusNoise + 1.0f) * 0.5f;

            // Calculate biome-specific height and radius ranges using noise
            // These values represent what the tree WOULD be if procedurally generated
            // Current TreeStamp system uses hardcoded sizes, but noise is calculated
            // for future procedural tree generation implementation.
            //
            // Biome-specific tree variation ranges:
            // - FOREST/PLAINS (Oak): height 4-7 blocks, radius 2-4 blocks
            // - TAIGA/SNOWY_TAIGA (Spruce): height 6-10 blocks, radius 1-3 blocks
            // - JUNGLE (Jungle Bush): height 3-5 blocks, radius 2-3 blocks
            // - DESERT (Cactus): height 2-5 blocks, no canopy
            // - SAVANNA (Acacia): height 5-8 blocks, radius 2-4 blocks
            //
            // NOTE: TreeStamp system currently uses fixed sizes.
            // This noise-based variation is ready for integration when procedural
            // tree generation is implemented (e.g., dynamic TreeStamp creation).
            int treeHeight = 0;
            int canopyRadius = 0;

            switch (biome)
            {
                case BiomeType::FOREST:
                case BiomeType::PLAINS:
                    // Oak trees: 3-9 blocks tall, 1-5 radius canopy (EXPANDED RANGES)
                    treeHeight = 3 + (int)(heightNoise01 * 6.0f);      // 3-9
                    canopyRadius = 1 + (int)(radiusNoise01 * 4.0f);   // 1-5
                    break;

                case BiomeType::TAIGA:
                case BiomeType::SNOWY_TAIGA:
                case BiomeType::SNOWY_PLAINS:
                    // Spruce trees: 5-13 blocks tall, 1-4 radius narrow canopy (EXPANDED RANGES)
                    treeHeight = 5 + (int)(heightNoise01 * 8.0f);      // 5-13
                    canopyRadius = 1 + (int)(radiusNoise01 * 3.0f);   // 1-4
                    break;

                case BiomeType::JUNGLE:
                    // Jungle bushes: 2-7 blocks tall, 1-4 radius (EXPANDED RANGES)
                    treeHeight = 2 + (int)(heightNoise01 * 5.0f);      // 2-7
                    canopyRadius = 1 + (int)(radiusNoise01 * 3.0f);   // 1-4
                    break;

                case BiomeType::DESERT:
                    // Cactus: 1-7 blocks tall, no canopy (EXPANDED RANGE)
                    treeHeight = 1 + (int)(heightNoise01 * 6.0f);      // 1-7
                    canopyRadius = 0;  // Cactus has no canopy
                    break;

                case BiomeType::SAVANNA:
                    // Acacia trees: 4-11 blocks tall, 1-5 radius canopy (EXPANDED RANGES)
                    treeHeight = 4 + (int)(heightNoise01 * 7.0f);      // 4-11
                    canopyRadius = 1 + (int)(radiusNoise01 * 4.0f);   // 1-5
                    break;

                default:
                    treeHeight = 5;
                    canopyRadius = 2;
                    break;
            }

            // Assignment 5 Phase 12: Use heightNoise01 to select tree variant (Small/Medium/Large)
            // This provides visible tree size variation using existing TreeStamp variants
            // Threshold: < 0.33 = Small, < 0.67 = Medium, >= 0.67 = Large
            switch (biome)
            {
                case BiomeType::FOREST:
                case BiomeType::PLAINS:
                    if (heightNoise01 < 0.33f)
                        treeStamp = &g_oakTreeSmall;
                    else if (heightNoise01 < 0.67f)
                        treeStamp = &g_oakTreeMedium;
                    else
                        treeStamp = &g_oakTreeLarge;
                    break;

                case BiomeType::TAIGA:
                    if (heightNoise01 < 0.33f)
                        treeStamp = &g_spruceTreeSmall;
                    else if (heightNoise01 < 0.67f)
                        treeStamp = &g_spruceTreeMedium;
                    else
                        treeStamp = &g_spruceTreeLarge;
                    break;

                case BiomeType::SNOWY_TAIGA:
                case BiomeType::SNOWY_PLAINS:
                    // Snowy spruces only have one variant currently
                    treeStamp = &g_snowySpruceTree;
                    break;

                case BiomeType::JUNGLE:
                    if (heightNoise01 < 0.33f)
                        treeStamp = &g_jungleTreeBush;
                    else if (heightNoise01 < 0.67f)
                        treeStamp = &g_jungleTreeMedium;
                    else
                        treeStamp = &g_jungleTreeLarge;
                    break;

                case BiomeType::DESERT:
                    if (heightNoise01 < 0.33f)
                        treeStamp = &g_cactus;
                    else if (heightNoise01 < 0.67f)
                        treeStamp = &g_cactusMedium;
                    else
                        treeStamp = &g_cactusLarge;
                    break;

                case BiomeType::SAVANNA:
                    if (heightNoise01 < 0.33f)
                        treeStamp = &g_acaciaTree;
                    else if (heightNoise01 < 0.67f)
                        treeStamp = &g_acaciaTreeMedium;
                    else
                        treeStamp = &g_acaciaTreeLarge;
                    break;

                default:
                    // Keep the already-selected stamp (from initial selection)
                    break;
            }

            // Check if we should place a tree here
            if (treeNoise01 < g_worldGenConfig->trees.treePlacementThreshold) continue;

            // Check surface block type - only place trees on suitable blocks
            int surfaceBlockIdx = x + y * CHUNK_SIZE_X + surfaceZ * CHUNK_SIZE_X * CHUNK_SIZE_Y;
            uint8_t surfaceBlockType = m_blocks[surfaceBlockIdx].m_typeIndex;


            // Trees can only grow on grass, dirt, sand, or snow
            bool isSuitableSurface = (
                surfaceBlockType == BLOCK_GRASS ||
                surfaceBlockType == BLOCK_GRASS_LIGHT ||
                surfaceBlockType == BLOCK_GRASS_DARK ||
                surfaceBlockType == BLOCK_GRASS_YELLOW ||
                surfaceBlockType == BLOCK_DIRT ||
                surfaceBlockType == BLOCK_SAND ||
                surfaceBlockType == BLOCK_SNOW  // Allow trees on snow (for SNOWY_TAIGA)
            );


            if (!isSuitableSurface) continue;



            // Place tree stamp at this location
            // Tree origin is at trunk center-bottom, so offset by trunk offset values
            int treeBaseX = x - treeStamp->trunkOffsetX;
            int treeBaseY = y - treeStamp->trunkOffsetY;
            int treeBaseZ = surfaceZ + 1;  // Place trunk 1 block above surface

            // Cross-Chunk Tree Detection: Check if tree extends beyond current chunk
            bool extendsNorth = false;
            bool extendsSouth = false;
            bool extendsEast = false;
            bool extendsWest = false;

            int treeMinX = treeBaseX;
            int treeMaxX = treeBaseX + treeStamp->sizeX - 1;
            int treeMinY = treeBaseY;
            int treeMaxY = treeBaseY + treeStamp->sizeY - 1;

            if (treeMinX < 0) extendsWest = true;
            if (treeMaxX >= CHUNK_SIZE_X) extendsEast = true;
            if (treeMinY < 0) extendsSouth = true;
            if (treeMaxY >= CHUNK_SIZE_Y) extendsNorth = true;

            // Store cross-chunk tree data if this tree extends beyond chunk boundaries
            if (extendsNorth || extendsSouth || extendsEast || extendsWest)
            {
                CrossChunkTreeData crossChunkTree;
                crossChunkTree.localX = x;
                crossChunkTree.localY = y;
                crossChunkTree.localZ = treeBaseZ;
                crossChunkTree.treeStamp = treeStamp;
                crossChunkTree.extendsNorth = extendsNorth;
                crossChunkTree.extendsSouth = extendsSouth;
                crossChunkTree.extendsEast = extendsEast;
                crossChunkTree.extendsWest = extendsWest;

                m_crossChunkTrees.push_back(crossChunkTree);
            }

            // Copy tree stamp blocks into chunk (only portions within current chunk)
            for (int stampZ = 0; stampZ < treeStamp->sizeZ; stampZ++)
            {
                for (int stampY = 0; stampY < treeStamp->sizeY; stampY++)
                {
                    for (int stampX = 0; stampX < treeStamp->sizeX; stampX++)
                    {
                        // Calculate world position for this stamp block
                        int worldX = treeBaseX + stampX;
                        int worldY = treeBaseY + stampY;
                        int worldZ = treeBaseZ + stampZ;

                        // Bounds check: only place blocks within current chunk
                        if (worldX < 0 || worldX >= CHUNK_SIZE_X) continue;
                        if (worldY < 0 || worldY >= CHUNK_SIZE_Y) continue;
                        if (worldZ < 0 || worldZ >= CHUNK_SIZE_Z) continue;

                        // Get block type from stamp
                        int stampIdx = stampX + stampY * treeStamp->sizeX + stampZ * treeStamp->sizeX * treeStamp->sizeY;
                        uint8_t stampBlockType = treeStamp->blocks[stampIdx];

                        // Skip air blocks (0 in stamp means don't place anything)
                        if (stampBlockType == BLOCK_AIR) continue;

                        // Calculate chunk block index
                        int chunkBlockIdx = worldX + worldY * CHUNK_SIZE_X + worldZ * CHUNK_SIZE_X * CHUNK_SIZE_Y;

                        // Only place tree blocks in air (don't overwrite existing solid blocks)
                        // BUGFIX: Direct array assignment instead of SetBlock() to avoid marking chunk as needing save
                        // Procedurally generated trees shouldn't trigger chunk saves
                        if (m_blocks[chunkBlockIdx].m_typeIndex == BLOCK_AIR)
                        {
                            // BUG HUNT: Before placing tree block, log if it has unexpected lighting
                            static int treeBlockLogCount = 0;
                            uint8_t oldOutdoor = m_blocks[chunkBlockIdx].GetOutdoorLight();
                            uint8_t oldIndoor = m_blocks[chunkBlockIdx].GetIndoorLight();

                            if (treeBlockLogCount < 20 && (oldOutdoor > 0 || oldIndoor > 0))
                            {
                                DebuggerPrintf("[TREE PLACE] Chunk(%d,%d) replacing AIR at (%d,%d,%d) with tree block type=%d, AIR had outdoor=%d indoor=%d!\n",
                                              m_chunkCoords.x, m_chunkCoords.y, worldX, worldY, worldZ,
                                              stampBlockType, oldOutdoor, oldIndoor);
                                treeBlockLogCount++;
                            }

                            m_blocks[chunkBlockIdx].m_typeIndex = stampBlockType;

                            // BUG HUNT: After placing, verify lighting is still what we expect
                            uint8_t newOutdoor = m_blocks[chunkBlockIdx].GetOutdoorLight();
                            uint8_t newIndoor = m_blocks[chunkBlockIdx].GetIndoorLight();

                            if (treeBlockLogCount < 20 && (newOutdoor != oldOutdoor || newIndoor != oldIndoor))
                            {
                                DebuggerPrintf("[TREE PLACE BUG] Lighting changed! outdoor %d->%d, indoor %d->%d\n",
                                              oldOutdoor, newOutdoor, oldIndoor, newIndoor);
                                treeBlockLogCount++;
                            }
                        }
                    }
                }
            }
        }
    }

    // Assignment 5 Phase 3: Initialize lighting after terrain generation
    InitializeLighting();

    // NOTE: Do NOT mark mesh as dirty here!
    // The mesh will be marked dirty AFTER lighting propagates via ProcessDirtyChunkMeshes()
    // This ensures the mesh is built with correct lighting data, not stale outdoor=0 values
    // m_isMeshDirty = true;  // DISABLED - mesh will be marked dirty by lighting system
}

//----------------------------------------------------------------------------------------------------
void Chunk::RebuildMesh()
{
    // Clear existing mesh data
    m_vertices.clear();
    m_indices.clear();

    // Cache-coherent iteration: iterate blocks in memory order for optimal cache performance
    // Memory layout is: index = x + (y << CHUNK_BITS_X) + (z << (CHUNK_BITS_X + CHUNK_BITS_Y))
    for (int blockIndex = 0; blockIndex < BLOCKS_PER_CHUNK; blockIndex++)
    {
        Block&            block = m_blocks[blockIndex];
        sBlockDefinition* def   = sBlockDefinition::GetDefinitionByIndex(block.m_typeIndex);

        // Skip invisible blocks (air, transparent blocks)
        if (!def || !def->IsVisible()) continue;

        // Create block iterator for efficient neighbor access
        BlockIterator blockIter(this, blockIndex);

        // Use hidden surface removal for only visible faces
        AddBlockFacesWithHiddenSurfaceRemoval(blockIter, def);
    }

    // Add debug wireframe for chunk bounds
    AddVertsForWireframeAABB3D(m_debugVertices, m_worldBounds, 0.1f);

    // Update GPU buffers from CPU arrays
    UpdateVertexBuffer();

    // Mark mesh as no longer dirty
    m_isMeshDirty = false;
}

//----------------------------------------------------------------------------------------------------
Block* Chunk::GetBlock(int const localBlockIndexX,
                       int const localBlockIndexY,
                       int const localBlockIndexZ)
{
    if (localBlockIndexX < 0 || localBlockIndexX > CHUNK_MAX_X ||
        localBlockIndexY < 0 || localBlockIndexY > CHUNK_MAX_Y ||
        localBlockIndexZ < 0 || localBlockIndexZ > CHUNK_MAX_Z)
    {
        return nullptr;
    }

    int const index = LocalCoordsToIndex(localBlockIndexX, localBlockIndexY, localBlockIndexZ);

    return &m_blocks[index];
}

//----------------------------------------------------------------------------------------------------
void Chunk::SetBlock(int localBlockIndexX, int localBlockIndexY, int localBlockIndexZ, uint8_t blockTypeIndex, World* world)
{
    // SAFETY CHECK: Validate chunk is in a valid state before modification
    // This prevents writing to chunks being deleted/deactivated during RegenerateAllChunks()
    ChunkState currentState = m_state.load();
    if (currentState != ChunkState::COMPLETE &&
        currentState != ChunkState::TERRAIN_GENERATING)
    {
        // Chunk is being deleted or not fully initialized - do not modify
        return;
    }

    // Validate coordinates
    if (localBlockIndexX < 0 || localBlockIndexX > CHUNK_MAX_X ||
        localBlockIndexY < 0 || localBlockIndexY > CHUNK_MAX_Y ||
        localBlockIndexZ < 0 || localBlockIndexZ > CHUNK_MAX_Z)
    {
        return; // Invalid coordinates
    }

    // Calculate block index
    int index = LocalCoordsToIndex(localBlockIndexX, localBlockIndexY, localBlockIndexZ);

    // Check if block type is actually changing
    if (m_blocks[index].m_typeIndex != blockTypeIndex)
    {

        // Set the new block type
        m_blocks[index].m_typeIndex = blockTypeIndex;

        // Mark chunk as modified - needs saving and mesh regeneration
        SetNeedsSaving(true);
        SetIsMeshDirty(true);

        // Assignment 5 Phase 7 FIX: Recalculate lighting when block changes
        // This fixes the oscillating lighting bug where placed blocks alternate between bright/dark
        if (world)
        {
            // Create BlockIterator for this block
            IntVec3 localCoords(localBlockIndexX, localBlockIndexY, localBlockIndexZ);
            BlockIterator blockIter(this, LocalCoordsToIndex(localCoords), world);

            // Add this block to dirty light queue for recalculation
            world->AddToDirtyLightQueue(blockIter);

            // Also add all 6 neighbors to propagate lighting changes
            // This ensures smooth lighting transitions around the modified block
            IntVec3 neighborOffsets[6] = {
                IntVec3(1, 0, 0),   // East
                IntVec3(-1, 0, 0),  // West
                IntVec3(0, 1, 0),   // North
                IntVec3(0, -1, 0),  // South
                IntVec3(0, 0, 1),   // Up
                IntVec3(0, 0, -1)   // Down
            };

            for (int i = 0; i < 6; i++)
            {
                BlockIterator neighborIter = blockIter.GetNeighbor(neighborOffsets[i]);
                if (neighborIter.IsValid())
                {
                    world->AddToDirtyLightQueue(neighborIter);
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------
// Private helper methods
//----------------------------------------------------------------------------------------------------
void Chunk::AddBlockFacesIfVisible(Vec3 const& blockCenter, sBlockDefinition* def, IntVec3 const& coords)
{
    UNUSED(coords)
    // For now, add all 6 faces - you can optimize with face culling later
    AddBlockFace(blockCenter, Vec3::Z_BASIS, def->GetTopUVs(), Rgba8::WHITE);        // Top
    AddBlockFace(blockCenter, -Vec3::Z_BASIS, def->GetBottomUVs(), Rgba8::WHITE);       // Bottom
    AddBlockFace(blockCenter, Vec3::X_BASIS, def->GetSideUVs(), Rgba8(230, 230, 230)); // East
    AddBlockFace(blockCenter, -Vec3::X_BASIS, def->GetSideUVs(), Rgba8(230, 230, 230)); // West
    AddBlockFace(blockCenter, Vec3::Y_BASIS, def->GetSideUVs(), Rgba8(200, 200, 200)); // North
    AddBlockFace(blockCenter, -Vec3::Y_BASIS, def->GetSideUVs(), Rgba8(200, 200, 200)); // South
}

//----------------------------------------------------------------------------------------------------
void Chunk::AddBlockFace(Vec3 const&  blockCenter,
                         Vec3 const&  faceNormal,
                         Vec2 const&  uvs,
                         Rgba8 const& tint)
{
    Vec3 right, up;
    faceNormal.GetOrthonormalBasis(faceNormal, &right, &up);

    Vec3 faceCenter = blockCenter + faceNormal * 0.5f;

    // Convert sprite coordinates to UV coordinates
    // Assuming 8x8 sprite atlas (64 total sprites in an 8x8 grid)
    constexpr float ATLAS_SIZE  = 8.f;  // 8x8 grid of sprites
    constexpr float SPRITE_SIZE = 1.f / ATLAS_SIZE;  // Each sprite is 1/8 of the texture

    Vec2 const  uvMins    = uvs;
    Vec2 const  uvMaxs    = uvs + Vec2::ONE;
    Vec2 const  realMins  = Vec2(uvMins.x, uvMaxs.y) * SPRITE_SIZE;
    Vec2 const  realMaxs  = Vec2(uvMaxs.x, uvMins.y) * SPRITE_SIZE;
    AABB2 const spriteUVs = AABB2(Vec2(realMins.x, 1.f - realMins.y), Vec2(realMaxs.x, 1.f - realMaxs.y));

    // Add vertices using the corrected UV coordinates
    AddVertsForQuad3D(m_vertices, m_indices,
                      faceCenter - right * 0.5f - up * 0.5f,    // Bottom Left
                      faceCenter + right * 0.5f - up * 0.5f,    // Bottom Right
                      faceCenter - right * 0.5f + up * 0.5f,    // Top Left
                      faceCenter + right * 0.5f + up * 0.5f,    // Top Right
                      tint, spriteUVs);
}

//----------------------------------------------------------------------------------------------------
void Chunk::UpdateVertexBuffer()
{
    if (m_vertices.empty())
    {
        return;
    }

    // CRITICAL FIX: Use atomic buffer swapping to prevent rendering race condition
    // Create new buffers FIRST, then swap atomically to prevent flashing during buffer updates

    // Create new main vertex buffer
    VertexBuffer* newVertexBuffer = g_renderer->CreateVertexBuffer(
        (int)m_vertices.size() * sizeof(Vertex_PCU),  // Total buffer size in bytes
        sizeof(Vertex_PCU)                       // Size of each vertex
    );
    g_renderer->CopyCPUToGPU(m_vertices.data(),
                             (unsigned int)(m_vertices.size() * sizeof(Vertex_PCU)),
                             newVertexBuffer);

    // Create new index buffer
    IndexBuffer* newIndexBuffer = g_renderer->CreateIndexBuffer(
        (int)m_indices.size() * sizeof(unsigned int),  // Total buffer size in bytes
        sizeof(unsigned int)                      // Size of each index
    );
    g_renderer->CopyCPUToGPU(m_indices.data(),
                             (unsigned int)(m_indices.size() * sizeof(unsigned int)),
                             newIndexBuffer);

    // Create new debug buffer if needed
    VertexBuffer* newDebugVertexBuffer = nullptr;
    if (!m_debugVertices.empty())
    {
        newDebugVertexBuffer = g_renderer->CreateVertexBuffer(
            (int)m_debugVertices.size() * sizeof(Vertex_PCU),  // Total buffer size
            sizeof(Vertex_PCU)                            // Size of each vertex
        );
        g_renderer->CopyCPUToGPU(m_debugVertices.data(),
                                 (unsigned int)(m_debugVertices.size() * sizeof(Vertex_PCU)),
                                 newDebugVertexBuffer);
    }



    // Store old buffers for deletion
    VertexBuffer* oldVertexBuffer = m_vertexBuffer;
    IndexBuffer*  oldIndexBuffer  = m_indexBuffer;
    VertexBuffer* oldDebugBuffer  = m_debugVertexBuffer;



    // ATOMIC SWAP: Replace all buffer pointers simultaneously
    // This prevents Render() from seeing inconsistent buffer state
    m_vertexBuffer      = newVertexBuffer;
    m_indexBuffer       = newIndexBuffer;
    m_debugVertexBuffer = newDebugVertexBuffer;



    // Delete old buffers AFTER the swap
    // Render() now uses new buffers, safe to delete old ones
    GAME_SAFE_RELEASE(oldVertexBuffer);
    GAME_SAFE_RELEASE(oldIndexBuffer);
    GAME_SAFE_RELEASE(oldDebugBuffer);


}

void Chunk::SetMeshClean()
{
    m_isMeshDirty = false;
}

//----------------------------------------------------------------------------------------------------
void Chunk::SetMeshData(VertexList_PCU const& vertices, IndexList const& indices,
                        VertexList_PCU const& debugVertices, IndexList const& debugIndices)
{
    // This method is called by ChunkMeshJob on the main thread to apply
    // mesh data that was generated on worker threads
    m_vertices = vertices;
    m_indices = indices;
    m_debugVertices = debugVertices;
    m_debugIndices = debugIndices;

    // The chunk mesh is now dirty and needs GPU buffer updates
    // This will be handled by the main thread calling UpdateVertexBuffer()
    m_isMeshDirty = true;
}

//----------------------------------------------------------------------------------------------------
// Static Utility Functions
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
int Chunk::LocalCoordsToIndex(const IntVec3& localCoords)
{
    return LocalCoordsToIndex(localCoords.x, localCoords.y, localCoords.z);
}

//----------------------------------------------------------------------------------------------------
int Chunk::LocalCoordsToIndex(int x, int y, int z)
{
    return x + (y << CHUNK_BITS_X) + (z << (CHUNK_BITS_X + CHUNK_BITS_Y));
}

//----------------------------------------------------------------------------------------------------
int Chunk::IndexToLocalX(int index)
{
    return index & CHUNK_MASK_X;
}

//----------------------------------------------------------------------------------------------------
int Chunk::IndexToLocalY(int index)
{
    return (index & CHUNK_MASK_Y) >> CHUNK_BITS_X;
}

//----------------------------------------------------------------------------------------------------
int Chunk::IndexToLocalZ(int index)
{
    return (index & CHUNK_MASK_Z) >> (CHUNK_BITS_X + CHUNK_BITS_Y);
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::IndexToLocalCoords(int index)
{
    return IntVec3(IndexToLocalX(index), IndexToLocalY(index), IndexToLocalZ(index));
}

//----------------------------------------------------------------------------------------------------
int Chunk::GlobalCoordsToIndex(const IntVec3& globalCoords)
{
    return GlobalCoordsToIndex(globalCoords.x, globalCoords.y, globalCoords.z);
}

//----------------------------------------------------------------------------------------------------
int Chunk::GlobalCoordsToIndex(int x, int y, int z)
{
    IntVec3 localCoords = GlobalCoordsToLocalCoords(IntVec3(x, y, z));
    return LocalCoordsToIndex(localCoords);
}

//----------------------------------------------------------------------------------------------------
IntVec2 Chunk::GetChunkCoords(const IntVec3& globalCoords)
{
    // Use proper integer division with floor behavior for negative coordinates
    int chunkX = static_cast<int>(floorf(static_cast<float>(globalCoords.x) / CHUNK_SIZE_X));
    int chunkY = static_cast<int>(floorf(static_cast<float>(globalCoords.y) / CHUNK_SIZE_Y));

    return IntVec2(chunkX, chunkY);
}

//----------------------------------------------------------------------------------------------------
IntVec2 Chunk::GetChunkCenter(const IntVec2& chunkCoords)
{
    return IntVec2(
        chunkCoords.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2,
        chunkCoords.y * CHUNK_SIZE_Y + CHUNK_SIZE_Y / 2
    );
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::GlobalCoordsToLocalCoords(const IntVec3& globalCoords)
{
    // Use modulo operation for local coordinates with proper handling of negative numbers
    int localX = globalCoords.x % CHUNK_SIZE_X;
    int localY = globalCoords.y % CHUNK_SIZE_Y;
    int localZ = globalCoords.z;

    // Handle negative modulo results (C++ modulo can return negative values)
    if (localX < 0) localX += CHUNK_SIZE_X;
    if (localY < 0) localY += CHUNK_SIZE_Y;

    return IntVec3(localX, localY, localZ);
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetGlobalCoords(const IntVec2& chunkCoords, int blockIndex)
{
    IntVec3 localCoords = IndexToLocalCoords(blockIndex);
    return GetGlobalCoords(chunkCoords, localCoords);
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetGlobalCoords(const IntVec2& chunkCoords, const IntVec3& localCoords)
{
    return IntVec3(
        chunkCoords.x * CHUNK_SIZE_X + localCoords.x,
        chunkCoords.y * CHUNK_SIZE_Y + localCoords.y,
        localCoords.z
    );
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetGlobalCoords(const Vec3& position)
{
    return IntVec3(
        static_cast<int>(position.x),
        static_cast<int>(position.y),
        static_cast<int>(position.z)
    );
}

//----------------------------------------------------------------------------------------------------
// Neighbor Chunk Management
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
void Chunk::SetNeighborChunks(Chunk* north, Chunk* south, Chunk* east, Chunk* west)
{
    m_northNeighbor = north;
    m_southNeighbor = south;
    m_eastNeighbor  = east;
    m_westNeighbor  = west;
}

//----------------------------------------------------------------------------------------------------
void Chunk::ClearNeighborPointers()
{
    m_northNeighbor = nullptr;
    m_southNeighbor = nullptr;
    m_eastNeighbor  = nullptr;
    m_westNeighbor  = nullptr;
}

//----------------------------------------------------------------------------------------------------
// Advanced Mesh Generation with Hidden Surface Removal
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
void Chunk::AddBlockFacesWithHiddenSurfaceRemoval(BlockIterator const& blockIter,
                                                  sBlockDefinition*    def)
{
    IntVec3 localCoords = blockIter.GetLocalCoords();
    Vec3    blockCenter = Vec3((float)localCoords.x + 0.5f, (float)localCoords.y + 0.5f, (float)localCoords.z + 0.5f) +
    Vec3(m_chunkCoords.x * CHUNK_SIZE_X, m_chunkCoords.y * CHUNK_SIZE_Y, 0);

    // Check each face direction and only add visible faces
    // Face directions as offset vectors
    IntVec3 faceDirections[6] = {
        IntVec3(0, 0, 1),   // Top face (+Z)
        IntVec3(0, 0, -1),  // Bottom face (-Z)
        IntVec3(1, 0, 0),   // East face (+X)
        IntVec3(-1, 0, 0),  // West face (-X)
        IntVec3(0, 1, 0),   // North face (+Y)
        IntVec3(0, -1, 0)   // South face (-Y)
    };

    Vec3 faceNormals[6] = {
        Vec3::Z_BASIS,      // Top
        -Vec3::Z_BASIS,     // Bottom
        Vec3::X_BASIS,      // East
        -Vec3::X_BASIS,     // West
        Vec3::Y_BASIS,      // North
        -Vec3::Y_BASIS      // South
    };

    // Assignment 5 Phase 7: Directional shading values for b channel
    // Top = 1.0 (255), Sides = 0.8 (204), Bottom = 0.6 (153)
    float directionalShading[6] = {
        1.0f,    // Top
        0.6f,    // Bottom
        0.8f,    // East
        0.8f,    // West
        0.8f,    // North
        0.8f     // South
    };

    // For each face, check if it's visible before adding it
    for (int faceIndex = 0; faceIndex < 6; faceIndex++)
    {
        if (IsFaceVisible(blockIter, faceDirections[faceIndex]))
        {
            Vec2 uvs;
            if (faceIndex == 0) uvs = def->GetTopUVs();     // Top
            else if (faceIndex == 1) uvs = def->GetBottomUVs();  // Bottom
            else uvs                     = def->GetSideUVs();    // Sides

            // Assignment 5 Phase 7 FIX: Get lighting from NEIGHBOR block in face direction
            // This is the correct Minecraft behavior - each face uses the light from the neighbor
            // For example, the top face of grass uses the light from the air block above
            BlockIterator neighborIter = blockIter.GetNeighbor(faceDirections[faceIndex]);
            Block* neighborBlock = neighborIter.IsValid() ? neighborIter.GetBlock() : nullptr;

            // Assignment 5: Read lighting from neighbor block
            // FIX: If neighbor is unavailable (unloaded chunk), use default skylight value
            // This prevents completely black faces on chunk edges where neighbors aren't loaded yet
            uint8_t outdoorLight = neighborBlock ? neighborBlock->GetOutdoorLight() : 15;  // Default to full skylight
            uint8_t indoorLight = neighborBlock ? neighborBlock->GetIndoorLight() : 0;

            // FIX: Apply minimum ambient light as INDOOR light (not outdoor) to prevent black faces
            // Indoor light is NOT modulated by day/night cycle, providing constant ambient illumination
            // This matches Minecraft's behavior where shadowed areas remain visible even at night
            constexpr uint8_t MIN_AMBIENT_LIGHT = 4;  // Minimum light level (4/15 = ~27% brightness)
            if (outdoorLight < MIN_AMBIENT_LIGHT && indoorLight == 0)
            {
                indoorLight = MIN_AMBIENT_LIGHT;  // Use indoor channel to avoid day/night modulation
            }

            // BUG HUNT: Log if we're reading HIGH outdoor light from UNDERGROUND neighbors
            // Works in both Debug and Release builds
            static int logCount = 0;
            Block* block = blockIter.GetBlock();
            IntVec3 blockCoords = blockIter.GetLocalCoords();

            // Log if UNDERGROUND block (z < 100) is reading outdoor > 5 from ANY neighbor
            if (logCount < 50 && blockCoords.z < 100 && outdoorLight > 5)
            {
                IntVec3 neighborCoords = neighborIter.IsValid() ? neighborIter.GetLocalCoords() : IntVec3::ZERO;
                IntVec2 chunkCoords = m_chunkCoords;
                IntVec2 neighborChunkCoords = neighborIter.IsValid() && neighborIter.GetChunk() ? neighborIter.GetChunk()->GetChunkCoords() : IntVec2::ZERO;
                uint8_t neighborType = neighborBlock ? neighborBlock->m_typeIndex : 255;
                uint8_t blockType = block ? block->m_typeIndex : 255;
                bool neighborIsSkyVisible = neighborBlock ? neighborBlock->IsSkyVisible() : false;

                DebuggerPrintf("[MESH BRIGHT] Block type=%d Chunk(%d,%d) Pos(%d,%d,%d) Face#%d reading outdoor=%d from neighbor type=%d at Chunk(%d,%d) Pos(%d,%d,%d) isSkyVisible=%d\n",
                              blockType, chunkCoords.x, chunkCoords.y, blockCoords.x, blockCoords.y, blockCoords.z,
                              faceIndex, outdoorLight, neighborType,
                              neighborChunkCoords.x, neighborChunkCoords.y, neighborCoords.x, neighborCoords.y, neighborCoords.z,
                              neighborIsSkyVisible ? 1 : 0);
                logCount++;
            }

            // BUG HUNT: Log if TOP FACE of SURFACE block reads LOW outdoor light (should be 15)
            // This is the critical bug - surface block's top face should read from sky-visible air block
            static int lowLightLogCount = 0;
            if (lowLightLogCount < 20 && faceIndex == 0 && outdoorLight < 10 && blockCoords.z >= 80 && blockCoords.z <= 150)
            {
                IntVec3 neighborCoords = neighborIter.IsValid() ? neighborIter.GetLocalCoords() : IntVec3::ZERO;
                IntVec2 chunkCoords = m_chunkCoords;
                uint8_t neighborType = neighborBlock ? neighborBlock->m_typeIndex : 255;
                uint8_t blockType = block ? block->m_typeIndex : 255;
                bool neighborIsSkyVisible = neighborBlock ? neighborBlock->IsSkyVisible() : false;

                DebuggerPrintf("[MESH LOW LIGHT BUG] Block type=%d Chunk(%d,%d) Pos(%d,%d,%d) TOP FACE reading outdoor=%d from neighbor type=%d at Pos(%d,%d,%d) isSkyVisible=%d\n",
                              blockType, chunkCoords.x, chunkCoords.y, blockCoords.x, blockCoords.y, blockCoords.z,
                              outdoorLight, neighborType,
                              neighborCoords.x, neighborCoords.y, neighborCoords.z,
                              neighborIsSkyVisible ? 1 : 0);
                lowLightLogCount++;
            }

            // Normalize lighting to 0.0-1.0 range
            float outdoorNormalized = (float)outdoorLight / 15.0f;
            float indoorNormalized = (float)indoorLight / 15.0f;

            // Assignment 5 Phase 7: Encode lighting in vertex colors
            // r = outdoor light (0-255), g = indoor light (0-255), b = directional shading (0-255)
            uint8_t r = (uint8_t)(outdoorNormalized * 255.0f);
            uint8_t g = (uint8_t)(indoorNormalized * 255.0f);
            uint8_t b = (uint8_t)(directionalShading[faceIndex] * 255.0f);
            Rgba8 vertexColor = Rgba8(r, g, b, 255);

            AddBlockFace(blockCenter, faceNormals[faceIndex], uvs, vertexColor);
        }
    }
}

//----------------------------------------------------------------------------------------------------
bool Chunk::IsFaceVisible(BlockIterator const& blockIter, IntVec3 const& faceDirection) const
{
    // Get neighboring block in the face direction
    BlockIterator neighborIter = blockIter.GetNeighbor(faceDirection);

    // Assignment 5 Phase 0 FIX: Conservative hidden surface removal
    // CRITICAL: If neighbor is invalid, assume it's OPAQUE (hidden face)
    // This prevents rendering chunk boundary faces when neighbor chunks aren't fully loaded
    // Only render faces at true world boundaries (z < 0 or z >= CHUNK_SIZE_Z)
    if (!neighborIter.IsValid())
    {
        // Check if we're at vertical world boundaries (top/bottom of world)
        IntVec3 currentCoords = blockIter.GetLocalCoords();
        IntVec3 neighborCoords = currentCoords + faceDirection;

        // Convert to world Z coordinate
        int worldZ = neighborCoords.z;

        // Only render face if at vertical world boundaries
        if (worldZ < 0 || worldZ >= CHUNK_SIZE_Z)
        {
            return true;  // At vertical world boundaries, render face
        }

        // ✅ A7 FIX: Assume unloaded neighbor is AIR (transparent) - show face at chunk boundaries
        // This prevents missing faces when neighboring chunks are unloaded
        return true;
    }

    // Get the neighboring block
    Block* neighborBlock = neighborIter.GetBlock();
    if (!neighborBlock)
    {
        // No block data means assume opaque (safer for hidden surface removal)
        return false;
    }

    // Get neighbor block definition
    sBlockDefinition* neighborDef = sBlockDefinition::GetDefinitionByIndex(neighborBlock->m_typeIndex);
    if (!neighborDef)
    {
        // Invalid definition - assume opaque
        return false;
    }

    // Assignment 5: Hidden Surface Removal Logic
    // Face is VISIBLE if neighbor is NOT opaque (air, transparent, etc.)
    // Face is HIDDEN if neighbor is opaque (solid block)
    // Pattern matches reference: if (!neighborBlock->IsOpaque()) { AddQuadToMesh(); }
    return !neighborDef->IsOpaque();
}

//----------------------------------------------------------------------------------------------------
// Thread-safe chunk state management methods
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
bool Chunk::SetState(ChunkState newState)
{
    m_state.store(newState);
    return true;
}

//----------------------------------------------------------------------------------------------------
bool Chunk::CompareAndSetState(ChunkState expected, ChunkState desired)
{
    return m_state.compare_exchange_strong(expected, desired);
}

//----------------------------------------------------------------------------------------------------
bool Chunk::IsStateOneOf(ChunkState state1, ChunkState state2, ChunkState state3, ChunkState state4) const
{
    ChunkState currentState = m_state.load();
    return (currentState == state1) || (currentState == state2) || (currentState == state3) || (currentState == state4);
}

//----------------------------------------------------------------------------------------------------
// Disk I/O operations - Thread-safe, called by I/O worker thread
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
bool Chunk::LoadFromDisk()
{
    std::string filename = StringFormat("Saves/Chunk({0},{1}).chunk", m_chunkCoords.x, m_chunkCoords.y);

    std::vector<uint8_t> buffer;
    if (!FileReadToBuffer(buffer, filename))
    {
        return false;
    }

    // Verify minimum file size (header + at least one RLE entry)
    if (buffer.size() < sizeof(ChunkFileHeader) + sizeof(ChunkRLEEntry))
    {
        return false; // File too small
    }

    // Read and validate header
    ChunkFileHeader header;
    memcpy(&header, buffer.data(), sizeof(ChunkFileHeader));

    // Validate header
    if (header.fourCC[0] != 'G' || header.fourCC[1] != 'C' ||
        header.fourCC[2] != 'H' || header.fourCC[3] != 'K')
    {
        return false; // Invalid 4CC
    }

    if (header.version != 1 || header.chunkBitsX != CHUNK_BITS_X ||
        header.chunkBitsY != CHUNK_BITS_Y || header.chunkBitsZ != CHUNK_BITS_Z)
    {
        return false; // Incompatible format
    }

    // Decompress RLE data
    size_t dataOffset = sizeof(ChunkFileHeader);
    int    blockIndex = 0;

    while (dataOffset < buffer.size() && blockIndex < BLOCKS_PER_CHUNK)
    {
        // Read RLE entry
        if (dataOffset + sizeof(ChunkRLEEntry) > buffer.size())
        {
            return false; // Incomplete RLE entry
        }

        ChunkRLEEntry entry;
        memcpy(&entry, buffer.data() + dataOffset, sizeof(ChunkRLEEntry));
        dataOffset += sizeof(ChunkRLEEntry);

        // Apply run to blocks
        for (int i = 0; i < entry.count && blockIndex < BLOCKS_PER_CHUNK; i++)
        {
            IntVec3 localCoords = IndexToLocalCoords(blockIndex);
            Block*  block       = GetBlock(localCoords.x, localCoords.y, localCoords.z);
            if (block != nullptr)
            {
                block->m_typeIndex = entry.value;
            }
            blockIndex++;
        }
    }

    // Verify we loaded exactly the right number of blocks
    if (blockIndex != BLOCKS_PER_CHUNK)
    {
        return false; // RLE data doesn't match expected block count
    }

    return true;
}

//----------------------------------------------------------------------------------------------------
bool Chunk::SaveToDisk() const
{
    // Ensure save directory exists (relative to executable in Run/ directory)
    std::string saveDir = "Saves/";
    std::filesystem::create_directories(saveDir);

    std::string filename = StringFormat("{0}Chunk({1},{2}).chunk", saveDir, m_chunkCoords.x, m_chunkCoords.y);

    // Collect block data in order for RLE compression
    std::vector<uint8_t> blockData(BLOCKS_PER_CHUNK);
    for (int i = 0; i < BLOCKS_PER_CHUNK; i++)
    {
        IntVec3 localCoords = IndexToLocalCoords(i);
        Block*  block       = const_cast<Chunk*>(this)->GetBlock(localCoords.x, localCoords.y, localCoords.z);
        if (block != nullptr)
        {
            blockData[i] = block->m_typeIndex;
        }
        else
        {
            blockData[i] = 0; // Air block if invalid
        }
    }

    // Compress using RLE
    std::vector<ChunkRLEEntry> rleEntries;
    uint8_t                    currentType = blockData[0];
    uint8_t                    runLength   = 1;

    for (int i = 1; i < BLOCKS_PER_CHUNK; i++)
    {
        if (blockData[i] == currentType && runLength < 255)
        {
            runLength++;
        }
        else
        {
            // End current run
            rleEntries.push_back({currentType, runLength});
            currentType = blockData[i];
            runLength   = 1;
        }
    }
    // Don't forget the last run
    rleEntries.push_back({currentType, runLength});

    // Create file header
    ChunkFileHeader header;
    header.fourCC[0]  = 'G';
    header.fourCC[1]  = 'C';
    header.fourCC[2]  = 'H';
    header.fourCC[3]  = 'K';
    header.version    = 1;
    header.chunkBitsX = CHUNK_BITS_X;
    header.chunkBitsY = CHUNK_BITS_Y;
    header.chunkBitsZ = CHUNK_BITS_Z;

    // Calculate total file size
    size_t               fileSize = sizeof(ChunkFileHeader) + rleEntries.size() * sizeof(ChunkRLEEntry);
    std::vector<uint8_t> fileBuffer(fileSize);

    // Write header
    memcpy(fileBuffer.data(), &header, sizeof(ChunkFileHeader));

    // Write RLE entries
    size_t offset = sizeof(ChunkFileHeader);
    for (const ChunkRLEEntry& entry : rleEntries)
    {
        memcpy(fileBuffer.data() + offset, &entry, sizeof(ChunkRLEEntry));
        offset += sizeof(ChunkRLEEntry);
    }

    // Write to file using safe fopen_s
    FILE*   file = nullptr;
    errno_t err  = fopen_s(&file, filename.c_str(), "wb");
    if (err != 0 || file == nullptr) return false;

    size_t written = fwrite(fileBuffer.data(), 1, fileBuffer.size(), file);
    fclose(file);

    return written == fileBuffer.size();
}

//----------------------------------------------------------------------------------------------------
// Cross-Chunk Tree Placement (Option 1: Post-Processing Pass)
//----------------------------------------------------------------------------------------------------
void Chunk::PlaceCrossChunkTrees()
{
    // Only run on main thread when chunk is complete and neighbors are available
    if (GetState() != ChunkState::COMPLETE) return;

    // Process each tree that extends into neighboring chunks
    for (CrossChunkTreeData const& treeData : m_crossChunkTrees)
    {
        TreeStamp* treeStamp = treeData.treeStamp;
        if (!treeStamp) continue;

        // Calculate tree base position
        int treeBaseX = treeData.localX - treeStamp->trunkOffsetX;
        int treeBaseY = treeData.localY - treeStamp->trunkOffsetY;
        int treeBaseZ = treeData.localZ;

        // Place tree portions that extend into each neighbor chunk
        if (treeData.extendsNorth && m_northNeighbor && m_northNeighbor->IsComplete())
        {
            PlaceTreeInNeighborChunk(m_northNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   0, CHUNK_SIZE_Y, 0);
        }

        if (treeData.extendsSouth && m_southNeighbor && m_southNeighbor->IsComplete())
        {
            PlaceTreeInNeighborChunk(m_southNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   0, -CHUNK_SIZE_Y, 0);
        }

        if (treeData.extendsEast && m_eastNeighbor && m_eastNeighbor->IsComplete())
        {
            PlaceTreeInNeighborChunk(m_eastNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   CHUNK_SIZE_X, 0, 0);
        }

        if (treeData.extendsWest && m_westNeighbor && m_westNeighbor->IsComplete())
        {
            PlaceTreeInNeighborChunk(m_westNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   -CHUNK_SIZE_X, 0, 0);
        }

        // Handle corner cases (trees extending into diagonal neighbors)
        // Note: Simplified diagonal neighbor access to avoid potential null pointer issues
        if (treeData.extendsNorth && treeData.extendsEast && m_northNeighbor && m_eastNeighbor)
        {
            // The northeast diagonal chunk would be handled by both north and east chunks
            // We'll place portions in both north and east chunks, avoiding direct diagonal access
            PlaceTreeInNeighborChunk(m_northNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   0, CHUNK_SIZE_Y, 0);
            PlaceTreeInNeighborChunk(m_eastNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   CHUNK_SIZE_X, 0, 0);
        }

        if (treeData.extendsNorth && treeData.extendsWest && m_northNeighbor && m_westNeighbor)
        {
            // The northwest diagonal chunk would be handled by both north and west chunks
            PlaceTreeInNeighborChunk(m_northNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   0, CHUNK_SIZE_Y, 0);
            PlaceTreeInNeighborChunk(m_westNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   -CHUNK_SIZE_X, 0, 0);
        }

        if (treeData.extendsSouth && treeData.extendsEast && m_southNeighbor && m_eastNeighbor)
        {
            // The southeast diagonal chunk would be handled by both south and east chunks
            PlaceTreeInNeighborChunk(m_southNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   0, -CHUNK_SIZE_Y, 0);
            PlaceTreeInNeighborChunk(m_eastNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   CHUNK_SIZE_X, 0, 0);
        }

        if (treeData.extendsSouth && treeData.extendsWest && m_southNeighbor && m_westNeighbor)
        {
            // The southwest diagonal chunk would be handled by both south and west chunks
            PlaceTreeInNeighborChunk(m_southNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   0, -CHUNK_SIZE_Y, 0);
            PlaceTreeInNeighborChunk(m_westNeighbor, treeStamp, treeBaseX, treeBaseY, treeBaseZ,
                                   -CHUNK_SIZE_X, 0, 0);
        }
    }

    // Clear cross-chunk tree data after processing
    if (!m_crossChunkTrees.empty())
    {
        m_crossChunkTrees.clear();
        // NOTE: Do NOT call SetIsMeshDirty(true) here!
        // Tree blocks trigger lighting changes via RecalculateBlockLighting(),
        // which adds chunk to m_chunksNeedingMeshRebuild automatically.
        // ProcessDirtyChunkMeshes() will mark it dirty AFTER lighting stabilizes.
    }
}

//----------------------------------------------------------------------------------------------------------------------
void Chunk::PlaceTreeInNeighborChunk(Chunk* neighborChunk, TreeStamp* treeStamp,
                                    int treeBaseX, int treeBaseY, int treeBaseZ,
                                    int offsetX, int offsetY, int offsetZ)
{
    if (!neighborChunk || !treeStamp) return;

    // Copy tree stamp blocks into neighbor chunk with coordinate transformation
    for (int stampZ = 0; stampZ < treeStamp->sizeZ; stampZ++)
    {
        for (int stampY = 0; stampY < treeStamp->sizeY; stampY++)
        {
            for (int stampX = 0; stampX < treeStamp->sizeX; stampX++)
            {
                // Calculate world position relative to this chunk
                int worldX = treeBaseX + stampX;
                int worldY = treeBaseY + stampY;
                int worldZ = treeBaseZ + stampZ;

                // Transform to neighbor chunk coordinates
                int neighborX = worldX + offsetX;
                int neighborY = worldY + offsetY;
                int neighborZ = worldZ + offsetZ;

                // Check if block is within neighbor chunk bounds
                if (neighborX < 0 || neighborX >= CHUNK_SIZE_X) continue;
                if (neighborY < 0 || neighborY >= CHUNK_SIZE_Y) continue;
                if (neighborZ < 0 || neighborZ >= CHUNK_SIZE_Z) continue;

                // Get stamp block type
                int stampIdx = stampX + stampY * treeStamp->sizeX + stampZ * treeStamp->sizeX * treeStamp->sizeY;
                uint8_t stampBlockType = treeStamp->blocks[stampIdx];

                if (stampBlockType == BLOCK_AIR) continue;

                // Place block in neighbor chunk
                int neighborBlockIdx = neighborX + neighborY * CHUNK_SIZE_X + neighborZ * CHUNK_SIZE_X * CHUNK_SIZE_Y;
                if (neighborChunk->m_blocks[neighborBlockIdx].m_typeIndex == BLOCK_AIR)
                {
                    neighborChunk->m_blocks[neighborBlockIdx].m_typeIndex = stampBlockType;
                }
            }
        }
    }

    // NOTE: Do NOT call SetIsMeshDirty(true) on neighbor chunk here!
    // Tree blocks trigger lighting changes via RecalculateBlockLighting(),
    // which adds neighbor chunk to m_chunksNeedingMeshRebuild automatically.
    // ProcessDirtyChunkMeshes() will mark it dirty AFTER lighting stabilizes.
    //
    // NOTE: We also don't call SetNeedsSaving(true) because cross-chunk trees are
    // procedurally generated and will be regenerated when the neighbor chunk loads.
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 3: Initialize lighting after terrain generation
//----------------------------------------------------------------------------------------------------
void Chunk::InitializeLighting()
{
    // DEBUG: Log that InitializeLighting is being called
    static int callCount = 0;
    callCount++;

    // Log first few calls for debugging
    if (callCount <= 3)
    {
        DebuggerPrintf("[INIT LIGHTING #%d] Chunk(%d,%d) InitializeLighting() called\n",
                      callCount, m_chunkCoords.x, m_chunkCoords.y);
    }

    int airBlocksSetToSky = 0;  // Count how many air blocks we set to sky visible

    // CRITICAL FIX (2025-11-16): ALWAYS update m_surfaceHeight[] to reflect ACTUAL top solid block
    // GenerateTerrain() sets m_surfaceHeight to ground level BEFORE tree placement,
    // so tree leaves are not accounted for. We must rescan to find true surface (including trees).
    // This ensures OnActivate() adds air ABOVE trees to dirty queue, not leaves inside canopy.
    bool needsSurfaceHeightInit = true;  // Always update to account for trees

    // Scan each (x,y) column from top to bottom
    for (int x = 0; x < CHUNK_SIZE_X; x++)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
        {
            int surfaceHeightForColumn = -1;  // Track surface height for m_surfaceHeight[] initialization (-1 = not found yet)

            // CRITICAL FIX (2025-11-16): Two-pass approach to correctly identify sky-visible blocks
            // Pass 1: Scan from top to bottom, mark blocks as sky-visible if no opaque block above
            // Pass 2: Scan from top to bottom, set outdoor=15 ONLY for sky-visible blocks until first opaque

            // Pass 1: Find the TRUE surface height (HIGHEST SOLID TERRAIN, not trees/leaves)
            // CORRECT ALGORITHM: Find the LAST transition from AIR → OPAQUE when descending from sky
            // This handles all cases: trees touching ground, trees with air gaps, no trees at all
            int lastAirZ = -1;  // Track the last AIR block we saw

            // First scan: Descend from sky and find the last AIR → OPAQUE transition
            for (int z = CHUNK_SIZE_Z - 1; z >= 0; z--)
            {
                Block* block = GetBlock(x, y, z);
                if (!block) continue;

                sBlockDefinition* blockDef = sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex);
                if (!blockDef) continue;

                if (blockDef->IsOpaque())
                {
                    // Found an opaque block. If we just saw AIR above, this is a surface transition.
                    if (lastAirZ != -1 && lastAirZ == z + 1)
                    {
                        // This is an AIR → OPAQUE transition, update surface height
                        surfaceHeightForColumn = z;
                        // Don't break! Keep searching for deeper transitions (caves, overhangs, etc.)
                    }
                    else if (surfaceHeightForColumn == -1)
                    {
                        // First opaque from top with no air above (e.g., bedrock column)
                        surfaceHeightForColumn = z;
                    }
                }
                else  // Non-opaque (AIR, WATER, etc.)
                {
                    lastAirZ = z;  // Remember this AIR block
                }
            }

            // If we never found any opaque blocks, surface is at -1 (all air column)
            // This is handled by the sky-visible scan below

            // Second scan: Mark sky-visible blocks (only blocks ABOVE surface get sky-visible=true)
            for (int z = CHUNK_SIZE_Z - 1; z >= 0; z--)
            {
                Block* block = GetBlock(x, y, z);
                if (!block) continue;

                sBlockDefinition* blockDef = sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex);
                if (!blockDef) continue;

                // Blocks ABOVE the true surface (including tree leaves/logs and air) can see sky
                if (z > surfaceHeightForColumn)
                {
                    if (!blockDef->IsOpaque())  // Only non-opaque blocks get sky-visible flag
                    {
                        block->SetIsSkyVisible(true);
                    }
                    else
                    {
                        block->SetIsSkyVisible(false);  // Tree leaves/logs above surface don't see sky
                    }
                }
                else
                {
                    // Blocks AT or BELOW surface cannot see sky
                    block->SetIsSkyVisible(false);
                }
            }

            // Pass 2: Set outdoor light based on sky visibility
            // CRITICAL FIX: Don't break at FIRST opaque - break at SURFACE HEIGHT from Pass 1!
            // This handles tree leaves above the terrain surface correctly.
            for (int z = CHUNK_SIZE_Z - 1; z >= 0; z--)
            {
                Block* block = GetBlock(x, y, z);
                if (!block)
                    continue;

                sBlockDefinition* blockDef = sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex);
                if (!blockDef)
                    continue;

                // Process blocks ABOVE and AT the surface
                if (z >= surfaceHeightForColumn || surfaceHeightForColumn == -1)
                {
                    if (blockDef->IsOpaque())
                    {
                        // A5 SPEC COMPLIANT: ALL opaque blocks get outdoor=0 (NO EXCEPTIONS)
                        // Per spec Task 3 (lines 154-155): "if (!foundSolid) outdoor=15 else outdoor=0"
                        // Water/ice/leaves are marked isOpaque="true" for gameplay collision,
                        // so InitializeLighting() treats them as opaque → outdoor=0
                        // RecalculateBlockLighting() will handle light propagation THROUGH them
                        block->SetOutdoorLight(0);

                        // Check if emissive (glowstone, lava, etc.)
                        if (blockDef->IsEmissive())
                        {
                            block->SetIndoorLight(blockDef->GetEmissiveValue());
                        }
                        else
                        {
                            block->SetIndoorLight(0);
                        }
                    }
                    else  // Non-opaque block (AIR, WATER, etc.)
                    {
                        // Only sky-visible blocks get outdoor=15
                        if (block->IsSkyVisible())
                        {
                            block->SetOutdoorLight(15);
                            block->SetIndoorLight(0);
                            airBlocksSetToSky++;
                        }
                        else
                        {
                            // Non-sky-visible air (shouldn't happen above surface, but handle it)
                            block->SetOutdoorLight(0);
                            block->SetIndoorLight(0);
                        }
                    }
                }
                else
                {
                    // Blocks BELOW surface - set to darkness
                    // A5 SPEC COMPLIANT: ALL blocks below surface get outdoor=0 (NO EXCEPTIONS)
                    block->SetOutdoorLight(0);

                    // Check if emissive (glowstone, lava, etc.)
                    if (blockDef->IsEmissive())
                    {
                        block->SetIndoorLight(blockDef->GetEmissiveValue());
                    }
                    else
                    {
                        block->SetIndoorLight(0);
                    }
                }
            }

            // CRITICAL FIX: Populate m_surfaceHeight[] for chunks loaded from disk
            // This is essential for OnActivate() to add correct blocks to dirty light queue
            if (needsSurfaceHeightInit)
            {
                m_surfaceHeight[x + y * CHUNK_SIZE_X] = surfaceHeightForColumn;
            }
        }
    }

    if (needsSurfaceHeightInit && callCount <= 3)
    {
        DebuggerPrintf("  [INIT LIGHTING] Populated m_surfaceHeight[] - sample: (0,0)=%d, (15,15)=%d\n",
                      m_surfaceHeight[0], m_surfaceHeight[15 + 15 * CHUNK_SIZE_X]);
    }

    // DEBUG: Log surface blocks with high lighting for debugging bright chunks
    static int chunkLightingDebugCount = 0;
    if (chunkLightingDebugCount < 3)
    {
        int brightBlockCount = 0;
        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            for (int y = 0; y < CHUNK_SIZE_Y; y++)
            {
                int surfaceZ = m_surfaceHeight[x + y * CHUNK_SIZE_X];
                if (surfaceZ >= 0)
                {
                    Block* surfaceBlock = GetBlock(x, y, surfaceZ);
                    if (surfaceBlock)
                    {
                        uint8_t outdoor = surfaceBlock->GetOutdoorLight();
                        uint8_t indoor = surfaceBlock->GetIndoorLight();

                        // Log if block has ANY light (should help identify bright chunks)
                        if (outdoor > 0 || indoor > 0)
                        {
                            brightBlockCount++;
                            if (brightBlockCount <= 5)  // Log first 5 bright surface blocks
                            {
                                DebuggerPrintf("[BRIGHT BLOCK] Chunk(%d,%d) Surface(%d,%d,%d) Type=%d outdoor=%d indoor=%d\n",
                                              m_chunkCoords.x, m_chunkCoords.y, x, y, surfaceZ,
                                              surfaceBlock->m_typeIndex, outdoor, indoor);
                            }
                        }
                    }
                }
            }
        }
        DebuggerPrintf("[CHUNK LIGHTING] Chunk(%d,%d) has %d surface blocks with light\n",
                      m_chunkCoords.x, m_chunkCoords.y, brightBlockCount);
        chunkLightingDebugCount++;
    }

    // NOTE: We don't queue all sky-visible blocks here because:
    // 1. InitializeLighting() doesn't have access to World* parameter
    // 2. OnActivate() will queue edge blocks which propagates lighting naturally
    // 3. ProcessDirtyLighting() will see IsSkyVisible flag and set outdoor=15
    // This approach matches the reference implementation


    // DEBUG: Log results AND check for inconsistencies
    if (callCount <= 10)  // Log first 10 chunks instead of 3
    {
        DebuggerPrintf("[INIT #%d] Chunk(%d,%d) Set %d air blocks to outdoor=15\n",
                      callCount, m_chunkCoords.x, m_chunkCoords.y, airBlocksSetToSky);

        // CRITICAL: Sample a few blocks to verify correct initialization
        int sampleX = 8, sampleY = 8;  // Center column
        int surfaceZ = m_surfaceHeight[sampleX + sampleY * CHUNK_SIZE_X];
        if (surfaceZ >= 0 && surfaceZ < CHUNK_SIZE_Z - 1)
        {
            Block* surfaceBlock = GetBlock(sampleX, sampleY, surfaceZ);
            Block* airAbove = GetBlock(sampleX, sampleY, surfaceZ + 1);

            if (surfaceBlock && airAbove)
            {
                DebuggerPrintf("  Sample(%d,%d): Surface block type=%d outdoor=%d indoor=%d, Air above outdoor=%d indoor=%d isSky=%d\n",
                              sampleX, sampleY,
                              surfaceBlock->m_typeIndex, surfaceBlock->GetOutdoorLight(), surfaceBlock->GetIndoorLight(),
                              airAbove->GetOutdoorLight(), airAbove->GetIndoorLight(), airAbove->IsSkyVisible() ? 1 : 0);
            }
        }
    }

    // CRITICAL BUG HUNT: Check if ANY blocks have INCORRECT indoor light values
    // We expect indoor=0 for all blocks except emissive ones
    if (callCount <= 3)
    {
        int indoorBugCount = 0;
        for (int z = 80; z < CHUNK_SIZE_Z; z++)  // Check high blocks (surface level)
        {
            for (int y = 0; y < CHUNK_SIZE_Y; y++)
            {
                for (int x = 0; x < CHUNK_SIZE_X; x++)
                {
                    Block* block = GetBlock(x, y, z);
                    if (block)
                    {
                        uint8_t outdoor = block->GetOutdoorLight();
                        uint8_t indoor = block->GetIndoorLight();

                        // If block has indoor>0 and it's NOT emissive, that's a bug!
                        sBlockDefinition* def = sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex);
                        bool isEmissive = def ? def->IsEmissive() : false;

                        if (indoor > 0 && !isEmissive && indoorBugCount < 10)
                        {
                            DebuggerPrintf("[INIT BUG] Chunk(%d,%d) Block(%d,%d,%d) type=%d has indoor=%d but NOT emissive! (outdoor=%d)\n",
                                          m_chunkCoords.x, m_chunkCoords.y, x, y, z, block->m_typeIndex, indoor, outdoor);
                            indoorBugCount++;
                        }
                    }
                }
            }
        }

        if (indoorBugCount > 0)
        {
            DebuggerPrintf("  [INIT BUG] Found %d blocks with incorrect indoor light!\n", indoorBugCount);
        }
    }

    // CRITICAL BUG HUNT: Verify that air blocks above surface ACTUALLY have outdoor=15
    // Check first few columns to see if they were set correctly
    if (callCount <= 3)
    {
        int bugCount = 0;
        for (int y = 0; y < 4; y++)  // Check first 4 rows
        {
            for (int x = 0; x < 4; x++)  // Check first 4 columns
            {
                int surfaceZ = m_surfaceHeight[x + y * CHUNK_SIZE_X];
                if (surfaceZ >= 0 && surfaceZ < CHUNK_SIZE_Z - 1)
                {
                    Block* airAbove = GetBlock(x, y, surfaceZ + 1);
                    if (airAbove && airAbove->GetOutdoorLight() == 0)
                    {
                        DebuggerPrintf("  [POST-INIT BUG] Column(%d,%d) surfaceZ=%d: AIR at z=%d has outdoor=%d! (Expected 15)\n",
                                      x, y, surfaceZ, surfaceZ + 1, airAbove->GetOutdoorLight());
                        bugCount++;
                    }
                }
            }
        }
        if (bugCount == 0)
        {
            DebuggerPrintf("  [POST-INIT OK] All checked air blocks above surface have outdoor=15\n");
        }
    }
}


//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 6: Populate dirty light queue with edge blocks on chunk activation
// RE-ENABLED: Phase 5 algorithm fixed to only propagate through non-opaque neighbors
//----------------------------------------------------------------------------------------------------
void Chunk::OnActivate(World* world)
{
    if (!world)
        return;

    // Assignment 5 Phase 6 FIX: Queue ONE surface block per column to trigger lighting propagation
    // CRITICAL: Only queue the air block directly above surface, not all sky-visible blocks
    int blocksQueued = 0;
    for (int x = 0; x < CHUNK_SIZE_X; x++)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
        {
            int surfaceZ = m_surfaceHeight[x + y * CHUNK_SIZE_X];
            if (surfaceZ >= 0 && surfaceZ < CHUNK_SIZE_Z - 1)
            {
                // Queue ONLY the air block just above the surface (surfaceZ + 1)
                // Don't check IsSkyVisible - just queue it
                int airBlockIndex = LocalCoordsToIndex(x, y, surfaceZ + 1);
                BlockIterator airIter(this, airBlockIndex, world);
                if (airIter.IsValid())
                {
                    world->AddToDirtyLightQueue(airIter);
                    blocksQueued++;
                }
            }
        }
    }

    // DEBUG: Log how many blocks were queued
    static int activationCount = 0;
    activationCount++;
    if (activationCount <= 5)
    {
        DebuggerPrintf("[ONACTIVATE] Chunk(%d,%d) queued %d surface blocks for lighting\n",
                      m_chunkCoords.x, m_chunkCoords.y, blocksQueued);
    }

    // CRITICAL FIX: Force mark this chunk for mesh rebuild
    // InitializeLighting() set outdoor=15 directly without going through RecalculateBlockLighting()
    // So the chunk wasn't added to m_chunksNeedingMeshRebuild
    // We need to ensure the mesh rebuilds with the correct lighting data
    world->MarkChunkForMeshRebuild(this);

    // Assignment 5 Phase 6: Mark edge blocks as dirty ONLY if neighbor chunks exist
    // Per spec line 147: "Mark non-opaque boundary blocks touching any existing neighboring chunk as light dirty"
    // This prevents underground blocks from brightening when there's no light source

    int const SURFACE_RANGE = 16;  // Distance above/below surface to check

    //----------------------------------------------------------------------------------------------------
    // North edge (y = CHUNK_SIZE_Y - 1) - ONLY if north neighbor exists
    //----------------------------------------------------------------------------------------------------
    if (m_northNeighbor != nullptr)
    {
        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            // Find surface height for this column
            int surfaceZ = m_surfaceHeight[x + 0 * CHUNK_SIZE_X];  // y=0 for north edge lookup

            // Only add blocks near surface
            int minZ = surfaceZ - SURFACE_RANGE;
            int maxZ = surfaceZ + SURFACE_RANGE;
            if (minZ < 0) minZ = 0;
            if (maxZ >= CHUNK_SIZE_Z) maxZ = CHUNK_SIZE_Z - 1;

            for (int z = minZ; z <= maxZ; z++)
            {
                int const blockIndex = LocalCoordsToIndex(x, CHUNK_SIZE_Y - 1, z);
                BlockIterator iter(this, blockIndex, world);
                world->AddToDirtyLightQueue(iter);
            }
        }
    }

    //----------------------------------------------------------------------------------------------------
    // South edge (y = 0) - ONLY if south neighbor exists
    //----------------------------------------------------------------------------------------------------
    if (m_southNeighbor != nullptr)
    {
        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            // Find surface height for this column
            int surfaceZ = m_surfaceHeight[x + 0 * CHUNK_SIZE_X];

            // Only add blocks near surface
            int minZ = surfaceZ - SURFACE_RANGE;
            int maxZ = surfaceZ + SURFACE_RANGE;
            if (minZ < 0) minZ = 0;
            if (maxZ >= CHUNK_SIZE_Z) maxZ = CHUNK_SIZE_Z - 1;

            for (int z = minZ; z <= maxZ; z++)
            {
                int const blockIndex = LocalCoordsToIndex(x, 0, z);
                BlockIterator iter(this, blockIndex, world);
                world->AddToDirtyLightQueue(iter);
            }
        }
    }

    //----------------------------------------------------------------------------------------------------
    // East edge (x = CHUNK_SIZE_X - 1) - ONLY if east neighbor exists
    //----------------------------------------------------------------------------------------------------
    if (m_eastNeighbor != nullptr)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
        {
            // Find surface height for this column
            int surfaceZ = m_surfaceHeight[(CHUNK_SIZE_X - 1) + y * CHUNK_SIZE_X];

            // Only add blocks near surface
            int minZ = surfaceZ - SURFACE_RANGE;
            int maxZ = surfaceZ + SURFACE_RANGE;
            if (minZ < 0) minZ = 0;
            if (maxZ >= CHUNK_SIZE_Z) maxZ = CHUNK_SIZE_Z - 1;

            for (int z = minZ; z <= maxZ; z++)
            {
                int const blockIndex = LocalCoordsToIndex(CHUNK_SIZE_X - 1, y, z);
                BlockIterator iter(this, blockIndex, world);
                world->AddToDirtyLightQueue(iter);
            }
        }
    }

    //----------------------------------------------------------------------------------------------------
    // West edge (x = 0) - ONLY if west neighbor exists
    //----------------------------------------------------------------------------------------------------
    if (m_westNeighbor != nullptr)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
        {
            // Find surface height for this column
            int surfaceZ = m_surfaceHeight[0 + y * CHUNK_SIZE_X];

            // Only add blocks near surface
            int minZ = surfaceZ - SURFACE_RANGE;
            int maxZ = surfaceZ + SURFACE_RANGE;
            if (minZ < 0) minZ = 0;
            if (maxZ >= CHUNK_SIZE_Z) maxZ = CHUNK_SIZE_Z - 1;

            for (int z = minZ; z <= maxZ; z++)
            {
                int const blockIndex = LocalCoordsToIndex(0, y, z);
                BlockIterator iter(this, blockIndex, world);
                world->AddToDirtyLightQueue(iter);
            }
        }
    }
}
