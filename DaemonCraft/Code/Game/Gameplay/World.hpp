//----------------------------------------------------------------------------------------------------
// World.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <map>
#include <deque>
#include <mutex>

#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Game/Framework/GameCommon.hpp"  // For DebugVisualizationMode
#include "Game/Framework/BlockIterator.hpp"  // Assignment 5 Phase 4: Required for std::deque<BlockIterator>

struct IntVec2;
struct IntVec3;
struct Vec3;
struct Rgba8;
class Camera;
class Chunk;
class Entity;
class ChunkGenerateJob;
class ChunkLoadJob;
class ChunkMeshJob;
class ChunkSaveJob;
class Shader;
class ConstantBuffer;

//----------------------------------------------------------------------------------------------------
// Hash function for IntVec2 to enable std::unordered_map usage
//----------------------------------------------------------------------------------------------------
namespace std
{
    template <>
    struct hash<IntVec2>
    {
        size_t operator()(IntVec2 const& vec) const noexcept;
    };
}

//----------------------------------------------------------------------------------------------------
// Chunk Management Constants - Reduced for Intel graphics compatibility
//----------------------------------------------------------------------------------------------------
// DEBUG MODE: Enable fixed world generation for testing (Task 5B.4)
// Set to true to generate a fixed 16×16 chunk world (256 chunks) that never expands
// Set to false for infinite world generation with dynamic loading
constexpr bool DEBUG_FIXED_WORLD_GEN = false;

// Debug mode: Generate fixed 16×16 chunk grid centered at origin (256 chunks total)
// This creates a static world from chunk (-8,-8) to (7,7)
// Chunks will NEVER be generated/deactivated as player moves
constexpr int DEBUG_FIXED_WORLD_HALF_SIZE = 8;  // 8 chunks in each direction = 16×16 grid

// Normal mode settings
constexpr int FULL_CHUNK_ACTIVATION_RANGE = 480;

// Use extremely large range in debug mode to keep all 256 chunks active
// In normal mode, use standard activation range
constexpr int CHUNK_ACTIVATION_RANGE    = DEBUG_FIXED_WORLD_GEN ? 10000 : FULL_CHUNK_ACTIVATION_RANGE;
constexpr int CHUNK_DEACTIVATION_RANGE  = CHUNK_ACTIVATION_RANGE + 16 + 16; // CHUNK_SIZE_X + CHUNK_SIZE_Y
constexpr int CHUNK_ACTIVATION_RADIUS_X = 1 + (CHUNK_ACTIVATION_RANGE / 16); // CHUNK_SIZE_X
constexpr int CHUNK_ACTIVATION_RADIUS_Y = 1 + (CHUNK_ACTIVATION_RANGE / 16); // CHUNK_SIZE_Y
// NOTE: MAX_ACTIVE_CHUNKS removed - deactivation at CHUNK_DEACTIVATION_RANGE provides natural memory limit

//----------------------------------------------------------------------------------------------------
// Chunk Preloading Constants (Phase 0, Task 0.7) - Smart directional preloading
//----------------------------------------------------------------------------------------------------
constexpr float PRELOAD_VELOCITY_THRESHOLD = 1.0f;    // Minimum velocity (m/s) to trigger directional preloading
constexpr int   PRELOAD_LOOKAHEAD_CHUNKS   = 3;       // Number of chunks to preload ahead of movement direction
constexpr float PRELOAD_PRIORITY_BOOST     = 100.0f;  // Priority boost for chunks in movement direction

//----------------------------------------------------------------------------------------------------
// Fixed World Bounds (Phase 0, Task 0.6) - For predictable testing and experimentation
//----------------------------------------------------------------------------------------------------
// World extends from MIN_CHUNK_COORD to MAX_CHUNK_COORD (inclusive) in both X and Y
// Example: [-8, 7] creates a 16x16 chunk world (256x256 blocks with CHUNK_SIZE_X=16)
// With CHUNK_SIZE_X/Y=32, this creates 512x512 blocks (16x16 chunks)
constexpr int WORLD_MIN_CHUNK_X = -8;   // Minimum chunk X coordinate (inclusive)
constexpr int WORLD_MAX_CHUNK_X = 7;    // Maximum chunk X coordinate (inclusive)
constexpr int WORLD_MIN_CHUNK_Y = -8;   // Minimum chunk Y coordinate (inclusive)
constexpr int WORLD_MAX_CHUNK_Y = 7;    // Maximum chunk Y coordinate (inclusive)

// Calculated world dimensions
constexpr int WORLD_SIZE_CHUNKS_X = (WORLD_MAX_CHUNK_X - WORLD_MIN_CHUNK_X + 1); // 16 chunks wide
constexpr int WORLD_SIZE_CHUNKS_Y = (WORLD_MAX_CHUNK_Y - WORLD_MIN_CHUNK_Y + 1); // 16 chunks deep
constexpr int TOTAL_WORLD_CHUNKS  = WORLD_SIZE_CHUNKS_X * WORLD_SIZE_CHUNKS_Y;   // 256 chunks total

//----------------------------------------------------------------------------------------------------
// Job Queue Limiting - Prevent overwhelming the worker threads
//----------------------------------------------------------------------------------------------------
constexpr int MAX_PENDING_GENERATE_JOBS = 128;  // Maximum chunk generation jobs in flight (increased from 16)
constexpr int MAX_PENDING_LOAD_JOBS     = 16;   // Maximum chunk load jobs in flight (increased from 4)
constexpr int MAX_PENDING_MESH_JOBS     = 16;   // Maximum chunk mesh jobs in flight
constexpr int MAX_PENDING_SAVE_JOBS     = 4;    // Maximum chunk save jobs in flight

//----------------------------------------------------------------------------------------------------
// 4. World units: Each world unit is 1 meter.  Each block is 1.0 x 1.0 x 1.0 world units (meters) in size.
//    This assumption (that blocks are each 1m x 1m x 1m) is fundamental and should be hard-coded (for purposes of speed and numerical precision).
// 5. World positions: 3D, Vec3 (float x,y,z), free-floating positions in world space.
//    The world's bounds extend infinitely in +/- X (east/west) and +/- Y (north/south) directions, but are finite vertically (from Z=0.0 at world bottom to Z=128.0 at world top, if chunks are 128 blocks tall).

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 10: Fast Voxel Raycast Result
//----------------------------------------------------------------------------------------------------
struct RaycastResult
{
    bool     m_didImpact = false;       // True if ray hit a solid block
    IntVec3  m_impactBlockCoords;       // Block coordinates of hit (if m_didImpact)
    float    m_impactDistance = 0.0f;   // Distance along ray to impact point
    Vec3     m_impactPosition;          // World position of impact
    Vec3     m_impactNormal;            // Surface normal at impact (face direction)

    RaycastResult() = default;
    RaycastResult(bool didImpact, IntVec3 const& blockCoords = IntVec3::ZERO, float distance = 0.0f)
        : m_didImpact(didImpact), m_impactBlockCoords(blockCoords), m_impactDistance(distance) {}
};

//----------------------------------------------------------------------------------------------------
class World
{
public:
    World();
    ~World();

    void Update(float deltaSeconds);
    void Render() const;

    void    ActivateChunk(IntVec2 const& chunkCoords);
    void    DeactivateChunk(IntVec2 const& chunkCoords, bool forceSynchronousSave = false);
    void    DeactivateAllChunks(bool forceSynchronousSave = false); // For debug F8 and shutdown
    void    RegenerateAllChunks(); // For ImGui "Regenerate Chunks" - forces fresh terrain generation
    void    ToggleGlobalChunkDebugDraw(); // For debug F2 key

    // Debug Visualization (Phase 0, Task 0.4)
    void    SetDebugVisualizationMode(DebugVisualizationMode mode);
    DebugVisualizationMode GetDebugVisualizationMode() const { return m_debugVisualizationMode; }
    bool    SetBlockAtGlobalCoords(IntVec3 const& globalCoords, uint8_t blockTypeIndex); // Set block at world position
    uint8_t GetBlockTypeAtGlobalCoords(IntVec3 const& globalCoords) const; // Get block type at world position
    bool    IsBlockSolid(IntVec3 const& globalCoords) const; // Assignment 6: Check if block is solid (for collision)
    bool    IsEntityOnGround(Entity const* entity) const;    // Assignment 6: 4-corner raycast ground detection
    void    PushEntityOutOfBlocks(Entity* entity);           // Assignment 6: Iterative collision resolution
    Chunk*  GetChunk(IntVec2 const& chunkCoords) const;

    // Assignment 5 Stage 8: Get computed sky/fog color for rendering clear screen
    Rgba8   GetSkyColor() const { return m_skyColor; }

    // Debug information getters
    int GetActiveChunkCount() const;
    int GetTotalVertexCount() const;
    int GetTotalIndexCount() const;
    int GetPendingGenerateJobCount() const;
    int GetPendingLoadJobCount() const;
    int GetPendingSaveJobCount() const;

    // Digging and placing methods
    bool    DigBlockAtCameraPosition(Vec3 const& cameraPos); // LMB - dig highest non-air block at or below camera
    bool    PlaceBlockAtCameraPosition(Vec3 const& cameraPos, uint8_t blockType); // RMB - place block above highest non-air block
    IntVec3 FindHighestNonAirBlockAtOrBelow(Vec3 const& position) const; // Helper to find the highest solid block

    // Assignment 7: Entity management
    void SpawnItemEntity(Vec3 const& position, struct ItemStack const& itemStack); // Spawn dropped item in world
    std::vector<class ItemEntity*> GetNearbyItemEntities(Vec3 const& position, float radius) const; // Get ItemEntities within radius

    // Assignment 7-AI: Agent management
    uint64_t SpawnAgent(std::string const& name, Vec3 const& position); // Spawn AI agent at position, returns unique ID
    class Agent* FindAgentByID(uint64_t agentID); // Find agent by ID (for KADI tool handlers)
    void DespawnAgent(uint64_t agentID); // Remove agent from world and delete
    std::vector<class Agent*> GetAllAgents() const; // Get all active agents
    void UpdateAgents(float deltaSeconds); // Update all agents (called by World::Update)
    void RenderAgents() const; // Render all agents (called by World::Render)

    // Chunk management helper methods
    Vec3    GetCameraPosition() const;
    Vec3    GetPlayerVelocity() const;  // For directional preloading (Task 0.7)
    float   GetDistanceToChunkCenter(IntVec2 const& chunkCoords, Vec3 const& cameraPos) const;
    IntVec2 FindNearestMissingChunkInRange(Vec3 const& cameraPos) const;
    IntVec2 FindFarthestActiveChunkOutsideDeactivationRange(Vec3 const& cameraPos) const;
    Chunk*  FindNearestDirtyChunk(Vec3 const& cameraPos) const;

    // Asynchronous job processing
    void ProcessCompletedJobs();  // Consolidated processor for all job types
    void ProcessDirtyChunkMeshes();
    void SubmitChunkForGeneration(Chunk* chunk);
    void SubmitChunkForMeshGeneration(Chunk* chunk);
    void SubmitChunkForLoading(Chunk* chunk);
    void SubmitChunkForSaving(Chunk* chunk);

    // Assignment 5 Phase 4: Dirty light queue management
    void AddToDirtyLightQueue(BlockIterator const& blockIter);
    void ProcessDirtyLighting(float maxTimeSeconds);

    // Assignment 5 Phase 5: Light propagation algorithm
    void RecalculateBlockLighting(BlockIterator const& blockIter);

    // Assignment 5 Phase 10: Fast voxel raycast using Amanatides & Woo algorithm
    RaycastResult RaycastVoxel(Vec3 const& start, Vec3 const& direction, float maxDistance) const;

    // Assignment 5 Phase 10: Force mark chunk for mesh rebuild (for chunks with InitializeLighting)
    void MarkChunkForMeshRebuild(Chunk* chunk);

private:
    //----------------------------------------------------------------------------------------------------
    // Active Chunks - Fully loaded and renderable chunks (main thread only)
    //----------------------------------------------------------------------------------------------------
    /// @brief Active chunks stored in hash map for O(1) lookup
    std::unordered_map<IntVec2, Chunk*> m_activeChunks;
    mutable std::mutex m_activeChunksMutex;  // Protects m_activeChunks from concurrent access

    //----------------------------------------------------------------------------------------------------
    // Non-Active Chunks - Chunks being processed by worker threads (REQUIRED by assignment)
    //----------------------------------------------------------------------------------------------------
    // This separate container holds chunks that are:
    // - Being generated by generic worker threads (TERRAIN_GENERATING state)
    // - Being loaded by I/O worker thread (LOADING state)
    // - Being saved by I/O worker thread (SAVING state)
    //
    // Assignment Requirement: "World owns and maintains a separate std::set of chunks which have been
    // created and queued for generation or loading or saving but are not currently active."
    std::set<Chunk*> m_nonActiveChunks;
    mutable std::mutex m_nonActiveChunksMutex;  // Protects m_nonActiveChunks from concurrent access

    //----------------------------------------------------------------------------------------------------
    // Job Management - Track pending/executing jobs for asynchronous chunk operations
    //----------------------------------------------------------------------------------------------------
    std::vector<ChunkGenerateJob*> m_chunkGenerationJobs;
    std::vector<ChunkLoadJob*>     m_chunkLoadJobs;
    std::vector<ChunkMeshJob*>     m_chunkMeshJobs;
    std::vector<ChunkSaveJob*>     m_chunkSaveJobs;
    mutable std::mutex m_jobListsMutex;  // Protects all job vectors from concurrent access

    std::unordered_set<IntVec2> m_queuedGenerateChunks;  // Track which chunks are queued for generation
    mutable std::mutex m_queuedChunksMutex;  // Protects m_queuedGenerateChunks from concurrent access

    std::vector<Chunk*> m_dirtyChunks;  // Chunks needing mesh rebuild (accessed only on main thread)

    // Assignment 5 Phase 4: Dirty light queue for lighting propagation (8ms budget per frame)
    std::deque<BlockIterator> m_dirtyLightQueue;  // Blocks needing light recalculation (main thread only)

    // Assignment 5 Phase 7: O(1) duplicate detection for lighting queue (fixes infinite propagation loop)
    // Tracks which blocks are already in m_dirtyLightQueue to prevent duplicate additions
    // Without this, blocks re-add each other infinitely and lighting queue never empties
    std::unordered_set<BlockIterator> m_dirtyLightSet;  // Fast lookup for queued blocks (main thread only)

    // Assignment 5 Phase 10: Chunk mesh rebuild tracking (fixes inconsistent nighttime lighting)
    // When lighting changes, chunks are added to this set but NOT marked mesh-dirty immediately.
    // After dirty light queue empties (lighting stabilizes), all tracked chunks are marked mesh-dirty.
    // This prevents mesh rebuild starvation while preserving "wait for stable lighting" behavior.
    std::unordered_set<Chunk*> m_chunksNeedingMeshRebuild;  // Chunks with changed lighting (main thread only)
    mutable std::mutex m_meshRebuildSetMutex;  // Protects m_chunksNeedingMeshRebuild from concurrent access

    // Assignment 5: Track when initial world generation is complete
    bool m_initialWorldGenComplete = false;  // Set to true when all 256 chunks are activated

    //----------------------------------------------------------------------------------------------------
    // Game State
    //----------------------------------------------------------------------------------------------------
    // Global debug state for all chunks
    bool m_globalChunkDebugDraw = false;

    // Debug visualization mode (Phase 0, Task 0.4)
    DebugVisualizationMode m_debugVisualizationMode = DebugVisualizationMode::NORMAL_TERRAIN;

    //----------------------------------------------------------------------------------------------------
    // Assignment 5 Phase 8: World Shader and Lighting Constants
    //----------------------------------------------------------------------------------------------------
    Shader*         m_worldShader           = nullptr;  // World.hlsl shader for lighting
    ConstantBuffer* m_worldConstantBuffer   = nullptr;  // CBO for OutdoorBrightness (register b8)
    float           m_outdoorBrightness     = 1.0f;     // Day/night modulation (1.0=noon, 0.2=midnight)
    float           m_gameTime              = 0.0f;     // Game time in seconds for day/night cycle

    // Assignment 5 Stage 8: Computed lighting colors (persist from Update to Render)
    Rgba8           m_skyColor              = Rgba8::WHITE;              // Sky/fog clear color with day/night + lightning
    Rgba8           m_finalIndoorColor      = Rgba8(255, 230, 204, 255); // Indoor light with glowstone flicker
    Rgba8           m_finalOutdoorColor     = Rgba8::WHITE;              // Outdoor light with day/night + lightning

    // Assignment 7: Entity management
    std::vector<class ItemEntity*> m_itemEntities;  // Dropped items in the world

    // Assignment 7-AI: Agent management
    std::map<uint64_t, class Agent*> m_agents;  // agentID → Agent* (O(1) lookup for KADI tools)
    uint64_t m_nextAgentID = 1;  // Incrementing ID generator (starts at 1, 0 = invalid)

    // Chunk management helper methods
    bool ChunkExistsOnDisk(IntVec2 const& chunkCoords) const;
    bool LoadChunkFromDisk(Chunk* chunk) const;
    bool SaveChunkToDisk(Chunk* chunk) const;
    void UpdateNeighborPointers(IntVec2 const& chunkCoords);
    void ClearNeighborReferences(IntVec2 const& chunkCoords);
};
