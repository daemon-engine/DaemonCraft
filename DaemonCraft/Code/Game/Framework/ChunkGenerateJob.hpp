//----------------------------------------------------------------------------------------------------
// ChunkGenerateJob.hpp - Asynchronous chunk terrain generation job
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include "Engine/Core/Job.hpp"
#include "Engine/Math/IntVec2.hpp"

//----------------------------------------------------------------------------------------------------
class Chunk;

//----------------------------------------------------------------------------------------------------
// ChunkGenerateJob - Asynchronous terrain generation for chunk data
//
// This job moves expensive chunk terrain generation off the main thread to eliminate frame drops.
// The job operates on a chunk in TERRAIN_GENERATING state and transitions it to LIGHTING_INITIALIZING
// when complete.
//
// Thread Safety:
// - Execute() method runs on worker threads only
// - Only accesses chunk block data (worker threads have exclusive access during generation)
// - Uses atomic state transitions to coordinate with main thread
// - Does NOT access DirectX resources (main thread only)
//
// Performance:
// - Processes up to 32,768 blocks per chunk (16x16x128)
// - Uses noise generation and procedural algorithms
// - Execution time varies from 50ms to 500ms depending on complexity
//
// Lifecycle:
// 1. Main thread creates job with chunk pointer
// 2. JobSystem queues job for worker thread
// 3. Worker thread executes terrain generation
// 4. Job transitions chunk state to LIGHTING_INITIALIZING
// 5. Main thread retrieves completed job and handles lighting
//----------------------------------------------------------------------------------------------------
class ChunkGenerateJob : public Job
{
public:
    // Constructor: Takes chunk pointer that will be processed
    explicit ChunkGenerateJob(Chunk* chunk);

    // Destructor: Cleanup any resources
    ~ChunkGenerateJob() override = default;

    // Prevent copying and assignment
    ChunkGenerateJob(ChunkGenerateJob const&)            = delete;
    ChunkGenerateJob& operator=(ChunkGenerateJob const&) = delete;
    ChunkGenerateJob(ChunkGenerateJob&&)                 = delete;
    ChunkGenerateJob& operator=(ChunkGenerateJob&&)      = delete;

    // Job interface implementation - called by worker thread
    void Execute() override;

    // Accessors for monitoring and debugging
    Chunk*  GetChunk() const { return m_chunk; }
    IntVec2 GetChunkCoords() const;
    bool    WasSuccessful() const { return m_wasSuccessful; }

private:
    // Target chunk for terrain generation
    Chunk* m_chunk = nullptr;

    // Success flag for error handling
    bool m_wasSuccessful = false;

    // Internal terrain generation methods (called from Execute)
    void GenerateTerrainData();
    void ValidateChunkState();
};
