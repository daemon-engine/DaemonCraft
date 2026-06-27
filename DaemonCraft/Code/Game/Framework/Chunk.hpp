//----------------------------------------------------------------------------------------------------
// Chunk.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <vector>
#include <atomic>

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Renderer/VertexUtils.hpp"
#include "Game/Framework/Block.hpp"
#include "Game/Framework/GameCommon.hpp"  // For BiomeType enum

//-Forward-Declaration--------------------------------------------------------------------------------
struct Rgba8;
struct Vec2;
struct Vec3;
struct sBlockDefinition;
class IndexBuffer;
class VertexBuffer;
class BlockIterator;
class World;  // Assignment 5 Phase 6: For OnActivate() method

//----------------------------------------------------------------------------------------------------
// Phase 0, Task 0.5: Larger chunk sizes for Assignment 4 (World Generation)
// Increased from 16x16x128 to 32x32x256 for:
//  - Better support for 3D density terrain with vertical variety
//  - Stress testing system limits (8x more blocks per chunk)
//  - Support for caves, mountains, and overhangs
//----------------------------------------------------------------------------------------------------
int constexpr CHUNK_BITS_X     = 5;                                                 // X coordinates need 5 bits (can represent 0-31)
int constexpr CHUNK_BITS_Y     = 5;                                                 // Y coordinates need 5 bits (can represent 0-31)
int constexpr CHUNK_BITS_Z     = 8;                                                 // Z coordinates need 8 bits (can represent 0-255)
int constexpr CHUNK_SIZE_X     = 1 << CHUNK_BITS_X;                                 // 1 shifted left 5 positions = 2^5 = 32 blocks wide (east-west)
int constexpr CHUNK_SIZE_Y     = 1 << CHUNK_BITS_Y;                                 // 1 shifted left 5 positions = 2^5 = 32 blocks deep (north-south)
int constexpr CHUNK_SIZE_Z     = 1 << CHUNK_BITS_Z;                                 // 1 shifted left 8 positions = 2^8 = 256 blocks tall (vertical)
int constexpr CHUNK_MAX_X      = CHUNK_SIZE_X - 1;                                  // Maximum valid X index (31) for bounds checking
int constexpr CHUNK_MAX_Y      = CHUNK_SIZE_Y - 1;                                  // Maximum valid Y index (31) for bounds checking
int constexpr CHUNK_MAX_Z      = CHUNK_SIZE_Z - 1;                                  // Maximum valid Z index (255) for bounds checking
int constexpr CHUNK_MASK_X     = CHUNK_MAX_X;                                       // Bit mask (0x001F) to extract X bits from block index
int constexpr CHUNK_MASK_Y     = CHUNK_MAX_Y << CHUNK_BITS_X;                       // Bit mask (0x03E0) to extract Y bits from block index
int constexpr CHUNK_MASK_Z     = CHUNK_MAX_Z << (CHUNK_BITS_X + CHUNK_BITS_Y);      // Bit mask (0xFFC00) to extract Z bits from block index
int constexpr BLOCKS_PER_CHUNK = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;        // Total blocks per chunk (32×32×256 = 262,144)

//----------------------------------------------------------------------------------------------------
// ChunkState - Thread-safe chunk lifecycle management
//
// This enum tracks the current state of a chunk throughout its lifecycle,
// enabling thread-safe coordination between main thread and worker threads.
// Uses atomic operations to prevent race conditions during state transitions.
//
// State Flow:
// CONSTRUCTING -> ACTIVATING -> [LOADING or TERRAIN_GENERATING] -> LIGHTING_INITIALIZING -> COMPLETE
//              -> DEACTIVATING -> SAVING -> SAVE_COMPLETE -> DECONSTRUCTING
//
// Thread Safety Rules:
// - Only main thread can transition: CONSTRUCTING, ACTIVATING, COMPLETE, DEACTIVATING, DECONSTRUCTING
// - Only I/O worker threads can transition: LOADING, LOAD_COMPLETE, SAVING, SAVE_COMPLETE
// - Only generic worker threads can transition: TERRAIN_GENERATING, LIGHTING_INITIALIZING
// - All state changes must use atomic compare-and-swap operations
//----------------------------------------------------------------------------------------------------
enum class ChunkState : uint8_t
{
    CONSTRUCTING = 0,       // Initial state during memory allocation (main thread)
    ACTIVATING,             // Being added to world systems (main thread)

    LOADING,                // I/O worker thread loading chunk from disk file
    LOAD_COMPLETE,          // Load finished, main thread can activate chunk

    TERRAIN_GENERATING,     // Generic worker thread generating block data
    LIGHTING_INITIALIZING,  // Setting up lighting data (worker thread or main thread)

    COMPLETE,               // Ready for rendering and interaction (main thread)

    DEACTIVATING,           // Being removed from world systems (main thread)
    SAVING,                 // I/O worker thread saving chunk to disk file
    SAVE_COMPLETE,          // Save finished, main thread can delete chunk

    DECONSTRUCTING          // Final cleanup before deletion (main thread)
};

//----------------------------------------------------------------------------------------------------
// TreeStamp - Hardcoded tree pattern data (Phase 3, Task 3B.1 - Assignment 4)
//----------------------------------------------------------------------------------------------------
// Trees are placed using "stamps" - pre-defined 3D patterns of wood and leaf blocks.
// Each stamp is a 3D grid of block types that can be copied into the world.
// Based on Minecraft wiki tree structures, simplified for available assets.
//
// Coordinate system for tree stamps:
//   - Origin (0,0,0) is at the center-bottom of the trunk
//   - X axis: West (-) to East (+)
//   - Y axis: South (-) to North (+)
//   - Z axis: Down (-) to Up (+)
//
// Usage: During tree placement (Task 3B.2), stamp blocks are copied into chunk
// with proper bounds checking to handle trees near chunk edges.
//----------------------------------------------------------------------------------------------------
struct TreeStamp
{
    int sizeX;              // Width (east-west) of the stamp bounding box
    int sizeY;              // Depth (north-south) of the stamp bounding box
    int sizeZ;              // Height (vertical) of the stamp bounding box
    int trunkOffsetX;       // X offset from stamp origin to trunk center
    int trunkOffsetY;       // Y offset from stamp origin to trunk center
    std::vector<uint8_t> blocks;  // Flattened 3D array [x + y*sizeX + z*sizeX*sizeY]
                                  // 0 = air (skip this block during placement)
                                  // non-zero = block type to place
};

//----------------------------------------------------------------------------------------------------
// BiomeData - Per-column biome information (Phase 1, Task 1.2 - Assignment 4)
//----------------------------------------------------------------------------------------------------
// Stores biome noise parameters for each (x,y) column in a chunk.
// According to task-pointer.md: "6 noise values only - no depth, no river"
// - Temperature, Humidity, Continentalness, Erosion, Weirdness are sampled from noise
// - Peaks & Valleys (PV) is calculated from Weirdness: PV = 1 - |(3 * abs(W)) - 2|
// - BiomeType is determined via lookup tables based on these 6 parameters
//----------------------------------------------------------------------------------------------------
struct BiomeData
{
    float     temperature;      // T: [-1, 1] range (5 levels: T0-T4)
    float     humidity;         // H: [-1, 1] range (5 levels: H0-H4)
    float     continentalness;  // C: [-1.2, 1.0] range (7 categories)
    float     erosion;          // E: [-1, 1] range (7 levels: E0-E6)
    float     weirdness;        // W: [-1, 1] range (used to calculate PV)
    float     peaksValleys;     // PV: [-1, 1] range (5 levels: Valleys to Peaks)
                                // PV = 1 - |(3 * abs(W)) - 2|
    BiomeType biomeType;        // Determined biome (via lookup tables)
};

//----------------------------------------------------------------------------------------------------
// Cross-Chunk Tree Placement Data (Option 1: Post-Processing Pass)
//----------------------------------------------------------------------------------------------------
// Stores trees that need to extend into neighboring chunks
struct CrossChunkTreeData
{
    int localX;           // X coordinate within this chunk (0-31)
    int localY;           // Y coordinate within this chunk (0-31)
    int localZ;           // Z coordinate within this chunk (0-255)
    TreeStamp* treeStamp; // Pointer to tree stamp definition
    // Direction indicates which neighbor chunks this tree extends into
    bool extendsNorth;    // +Y direction
    bool extendsSouth;    // -Y direction
    bool extendsEast;     // +X direction
    bool extendsWest;     // -X direction
};

//----------------------------------------------------------------------------------------------------
class Chunk
{
public:
    explicit Chunk(IntVec2 const& chunkCoords);
    ~Chunk();

    void Update(float deltaSeconds);
    void Render();

    IntVec2 GetChunkCoords() const { return m_chunkCoords; }
    AABB3   GetWorldBounds() const { return m_worldBounds; }

    // Debug information getters
    int GetVertexCount() const { return (int)m_vertices.size(); }
    int GetIndexCount() const { return (int)m_indices.size(); }

    // Core methods
    void GenerateTerrain();
    void RebuildMesh();

    // Assignment 5 Phase 6: Chunk activation lighting
    void OnActivate(World* world);

    // Thread-safe mesh data operations for ChunkMeshJob
    void SetMeshData(VertexList_PCU const& vertices, IndexList const& indices,
                     VertexList_PCU const& debugVertices, IndexList const& debugIndices);
    void UpdateVertexBuffer();
    void SetMeshClean();


    // Make ChunkMeshJob a friend class so it can access private mesh generation methods
    friend class ChunkMeshJob;

    Block* GetBlock(int localBlockIndexX, int localBlockIndexY, int localBlockIndexZ);
    void   SetBlock(int localBlockIndexX, int localBlockIndexY, int localBlockIndexZ, uint8_t blockTypeIndex, World* world = nullptr);

    // Static utility functions for chunk coordinate management
    static int     LocalCoordsToIndex(IntVec3 const& localCoords);
    static int     LocalCoordsToIndex(int x, int y, int z);
    static int     IndexToLocalX(int index);
    static int     IndexToLocalY(int index);
    static int     IndexToLocalZ(int index);
    static IntVec3 IndexToLocalCoords(int index);
    static int     GlobalCoordsToIndex(IntVec3 const& globalCoords);
    static int     GlobalCoordsToIndex(int x, int y, int z);
    static IntVec2 GetChunkCoords(IntVec3 const& globalCoords);
    static IntVec2 GetChunkCenter(IntVec2 const& chunkCoords);
    static IntVec3 GlobalCoordsToLocalCoords(IntVec3 const& globalCoords);
    static IntVec3 GetGlobalCoords(IntVec2 const& chunkCoords, int blockIndex);
    static IntVec3 GetGlobalCoords(IntVec2 const& chunkCoords, IntVec3 const& localCoords);
    static IntVec3 GetGlobalCoords(Vec3 const& position);

    // Chunk management methods for persistent world
    bool GetNeedsSaving() const { return m_needsSaving; }
    void SetNeedsSaving(bool const needsSaving) { m_needsSaving = needsSaving; }
    bool GetIsMeshDirty() const { return m_isMeshDirty; }
    void SetIsMeshDirty(bool const isDirty) { m_isMeshDirty = isDirty; }

    // Debug rendering control
    bool GetDebugDraw() const { return m_drawDebug; }
    void SetDebugDraw(bool const drawDebug) { m_drawDebug = drawDebug; }

    // Neighbor chunk management
    void   SetNeighborChunks(Chunk* north, Chunk* south, Chunk* east, Chunk* west);
    void   ClearNeighborPointers();
    Chunk* GetNorthNeighbor() const { return m_northNeighbor; }
    Chunk* GetSouthNeighbor() const { return m_southNeighbor; }
    Chunk* GetEastNeighbor() const { return m_eastNeighbor; }
    Chunk* GetWestNeighbor() const { return m_westNeighbor; }

    // Thread-safe chunk state management
    ChunkState GetState() const { return m_state.load(); }
    bool SetState(ChunkState newState);
    bool CompareAndSetState(ChunkState expected, ChunkState desired);
    bool IsStateOneOf(ChunkState state1, ChunkState state2 = ChunkState::CONSTRUCTING,
                     ChunkState state3 = ChunkState::CONSTRUCTING, ChunkState state4 = ChunkState::CONSTRUCTING) const;
    bool IsReadyForWork() const { return GetState() == ChunkState::TERRAIN_GENERATING; }
    bool IsComplete() const { return GetState() == ChunkState::COMPLETE; }
    bool CanBeModified() const { return GetState() == ChunkState::COMPLETE; }

    // Cross-Chunk Tree Placement (Option 1: Post-Processing Pass)
    void PlaceCrossChunkTrees();
    void PlaceTreeInNeighborChunk(Chunk* neighborChunk, TreeStamp* treeStamp,
                                int treeBaseX, int treeBaseY, int treeBaseZ,
                                int offsetX, int offsetY, int offsetZ);

    // Disk I/O operations (thread-safe, called by I/O worker thread)
    bool LoadFromDisk();
    bool SaveToDisk() const;

    // Assignment 5 Phase 3: Initial lighting setup (called after terrain generation or disk load)
    // Made public so ChunkLoadJob can call it after loading from disk
    void InitializeLighting();

private:
    /// @brief 6. Chunk coordinates: 2D, IntVec2 (int x,y), with x and y axes aligned with world axes (above).
    ///           Adjacent chunks have adjacent chunk coordinates; for example, chunk (4,7) is the immediate eastern neighbor of chunk (3,7), and chunk (3,7)'s easternmost edge lines up exactly with chunk (4,7)'s westernmost edge.
    ///           There is no chunk Z coordinate since each chunk extends fully from the bottom of the world to the top of the world (i.e. Chunks do not stack vertically).
    IntVec2 m_chunkCoords = IntVec2::ZERO;
    /// @brief
    AABB3 m_worldBounds = AABB3::ZERO;
    Block m_blocks[BLOCKS_PER_CHUNK]; // 1D array for cache efficiency

    // Assignment 4: Biome data per (x,y) column (Phase 1, Task 1.2)
    // Stores 6 noise parameters and biome type for each horizontal column
    BiomeData m_biomeData[CHUNK_SIZE_X * CHUNK_SIZE_Y];

    // Assignment 4: Surface height per (x,y) column (Phase 3, Task 3A.1)
    // Stores the z-coordinate of the top solid block in each column
    // Value of -1 indicates no solid blocks found in column (all air)
    int m_surfaceHeight[CHUNK_SIZE_X * CHUNK_SIZE_Y];

    std::vector<CrossChunkTreeData> m_crossChunkTrees;

    // Rendering
    VertexList_PCU m_vertices;
    IndexList      m_indices;
    VertexBuffer*  m_vertexBuffer = nullptr;
    IndexBuffer*   m_indexBuffer  = nullptr;
    VertexList_PCU m_debugVertices;
    IndexList      m_debugIndices;
    VertexBuffer*  m_debugVertexBuffer = nullptr;
    IndexBuffer*   m_debugBuffer       = nullptr;
    bool           m_drawDebug         = false;

    // Chunk management flags for persistent world
    bool m_needsSaving = false;        // true if chunk has been modified and needs to be saved to disk
    bool m_isMeshDirty = true;         // true if mesh needs regeneration

    // Thread-safe chunk state (atomic for multi-threaded access)
    mutable std::atomic<ChunkState> m_state{ChunkState::CONSTRUCTING};

    // Neighbor chunk pointers for efficient access
    Chunk* m_northNeighbor = nullptr;   // +Y direction
    Chunk* m_southNeighbor = nullptr;   // -Y direction
    Chunk* m_eastNeighbor  = nullptr;   // +X direction
    Chunk* m_westNeighbor  = nullptr;   // -X direction

    // Helper methods
    void AddBlockFacesIfVisible(Vec3 const& blockCenter, sBlockDefinition* def, IntVec3 const& coords);
    void AddBlockFace(Vec3 const& blockCenter, Vec3 const& faceNormal, Vec2 const& uvs, Rgba8 const& tint);


    // Advanced mesh generation with hidden surface removal (accessible to ChunkMeshJob)
    void AddBlockFacesWithHiddenSurfaceRemoval(BlockIterator const& blockIter, sBlockDefinition* def);
    bool IsFaceVisible(BlockIterator const& blockIter, IntVec3 const& faceDirection) const;

    // Make BlockIterator constructor accessible for ChunkMeshJob
    friend class BlockIterator;
};
