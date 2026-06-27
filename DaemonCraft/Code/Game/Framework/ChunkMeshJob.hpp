//----------------------------------------------------------------------------------------------------
// ChunkMeshJob.hpp - Asynchronous chunk mesh generation job
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include "Engine/Core/Job.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Renderer/VertexUtils.hpp"

//----------------------------------------------------------------------------------------------------
class Chunk;
class BlockIterator;
class World;

//----------------------------------------------------------------------------------------------------
// ChunkMeshJob - Asynchronous mesh generation for chunk rendering
//
// This job moves expensive CPU mesh building off the main thread to eliminate frame drops
// while keeping DirectX GPU operations on the main thread for thread safety.
//
// Thread Safety:
// - Execute() method runs on worker threads only
// - Only accesses chunk block data (worker threads have exclusive access during mesh building)
// - Does NOT access DirectX resources (must be done on main thread)
// - Uses atomic state transitions to coordinate with main thread
//
// Performance:
// - Processes up to 32,768 blocks per chunk (16x16x128)
// - Performs hidden surface removal for optimal triangle count
// - Execution time varies from 20ms to 200ms depending on terrain complexity
//
// Lifecycle:
// 1. Main thread creates job with chunk pointer
// 2. JobSystem queues job for worker thread
// 3. Worker thread executes CPU mesh building (vertex/index generation)
// 4. Job stores generated mesh data in member variables
// 5. Main thread retrieves completed job and calls UpdateVertexBuffer()
//----------------------------------------------------------------------------------------------------
class ChunkMeshJob : public Job
{
public:
    // Constructor: Takes chunk and world pointers for cross-chunk neighbor access
    explicit ChunkMeshJob(Chunk* chunk, World* world);

    // Destructor: Cleanup any resources
    ~ChunkMeshJob() override = default;

    // Prevent copying and assignment
    ChunkMeshJob(ChunkMeshJob const&)            = delete;
    ChunkMeshJob& operator=(ChunkMeshJob const&) = delete;
    ChunkMeshJob(ChunkMeshJob&&)                 = delete;
    ChunkMeshJob& operator=(ChunkMeshJob&&)      = delete;

    // Job interface implementation - called by worker thread
    void Execute() override;

    // Accessors for monitoring and debugging
    Chunk*         GetChunk() const { return m_chunk; }
    IntVec2        GetChunkCoords() const;
    bool           WasSuccessful() const { return m_wasSuccessful; }
    VertexList_PCU GetVertices() const { return m_vertices; }
    IndexList      GetIndices() const { return m_indices; }
    VertexList_PCU GetDebugVertices() const { return m_debugVertices; }
    IndexList      GetDebugIndices() const { return m_debugIndices; }

    // Apply mesh data to chunk - called by main thread only
    void ApplyMeshDataToChunk();

private:
    // Target chunk for mesh generation
    Chunk* m_chunk = nullptr;

    // World pointer for cross-chunk neighbor access (Assignment 5 Phase 0: Hidden Surface Removal)
    World* m_world = nullptr;

    // Success flag for error handling
    bool m_wasSuccessful = false;

    // Generated mesh data (filled by worker thread, consumed by main thread)
    VertexList_PCU m_vertices;
    IndexList      m_indices;
    VertexList_PCU m_debugVertices;
    IndexList      m_debugIndices;

    // Internal mesh generation methods (called from Execute)
    void GenerateMeshData();
    void ValidateChunkState();

    // Thread-safe mesh building helpers (write to job's local vectors)
    bool IsFaceVisibleForJob(BlockIterator const& blockIter, IntVec3 const& faceDirection) const;
    void AddBlockFaceToJob(Vec3 const& blockCenter, Vec3 const& faceNormal, Vec2 const& uvs, Rgba8 const& tint);
};