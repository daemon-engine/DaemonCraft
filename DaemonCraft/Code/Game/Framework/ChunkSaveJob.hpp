//----------------------------------------------------------------------------------------------------
// ChunkSaveJob.hpp - Asynchronous chunk saving to disk
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include "Engine/Core/Job.hpp"

//----------------------------------------------------------------------------------------------------
class Chunk;

//----------------------------------------------------------------------------------------------------
// ChunkSaveJob - Saves chunk block data to disk file
//
// Execution Flow:
// 1. Main thread creates job and submits to JobSystem with JOB_TYPE_IO
// 2. I/O worker thread claims job (only I/O workers can claim I/O jobs)
// 3. Worker compresses and writes chunk data to file
// 4. Worker transitions chunk state using atomic operations: SAVING -> SAVE_COMPLETE
// 5. Main thread retrieves completed job, deactivates/deletes chunk
//
// Thread Safety:
// - Chunk state is std::atomic for safe transitions
// - Only I/O thread accesses chunk blocks during SAVING state
// - Main thread must not modify chunk while SAVING
// - No DirectX/GPU calls in Execute() - only file I/O operations
//
// File Format:
// - Binary file: Saves/Chunk_(x,y).chunk
// - Raw block array (32,768 bytes for 16x16x128 chunks)
// - Future: Add compression and versioning
//
// Use Case:
// - Chunk deactivation: Save modified chunks before removing from active set
// - World shutdown: Batch save all modified chunks
// - Autosave: Periodic backup of player-modified chunks
//----------------------------------------------------------------------------------------------------
class ChunkSaveJob : public Job
{
public:
    // Constructor: Marks chunk as queued for saving, sets job type to I/O
    explicit ChunkSaveJob(Chunk* chunk);

    // Destructor
    virtual ~ChunkSaveJob() = default;

    // Execute: Save chunk to disk (runs on I/O worker thread)
    // This method is called by JobWorkerThread on I/O thread
    virtual void Execute() override;

    // Check if save was successful
    bool WasSuccessful() const { return m_wasSuccessful; }

    // Get the chunk being saved
    Chunk* GetChunk() const { return m_chunk; }

private:
    Chunk* m_chunk = nullptr;
    bool m_wasSuccessful = false;
};
