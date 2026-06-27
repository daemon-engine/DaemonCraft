//----------------------------------------------------------------------------------------------------
// ChunkLoadJob.hpp - Asynchronous chunk loading from disk
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include "Engine/Core/Job.hpp"

//----------------------------------------------------------------------------------------------------
class Chunk;

//----------------------------------------------------------------------------------------------------
// ChunkLoadJob - Loads chunk block data from disk file
//
// Execution Flow:
// 1. Main thread creates job and submits to JobSystem with JOB_TYPE_IO
// 2. I/O worker thread claims job (only I/O workers can claim I/O jobs)
// 3. Worker reads chunk file, decompresses data, populates chunk blocks
// 4. Worker transitions chunk state using atomic operations: LOADING -> LOAD_COMPLETE
// 5. Main thread retrieves completed job, activates chunk
//
// Thread Safety:
// - Chunk state is std::atomic for safe transitions
// - Only I/O thread accesses chunk blocks during LOADING state
// - Main thread waits for LOAD_COMPLETE before accessing blocks
// - No DirectX/GPU calls in Execute() - only file I/O operations
//
// File Format:
// - Binary file: Saves/Chunk_(x,y).chunk
// - Raw block array (32,768 bytes for 16x16x128 chunks)
// - Future: Add compression and versioning
//----------------------------------------------------------------------------------------------------
class ChunkLoadJob : public Job
{
public:
    // Constructor: Marks chunk as queued for loading, sets job type to I/O
    explicit ChunkLoadJob(Chunk* chunk);

    // Destructor
    virtual ~ChunkLoadJob() = default;

    // Execute: Load chunk from disk (runs on I/O worker thread)
    // This method is called by JobWorkerThread on I/O thread
    virtual void Execute() override;

    // Check if load was successful
    bool WasSuccessful() const { return m_wasSuccessful; }

    // Get the chunk being loaded
    Chunk* GetChunk() const { return m_chunk; }

private:
    Chunk* m_chunk = nullptr;
    bool m_wasSuccessful = false;
};
