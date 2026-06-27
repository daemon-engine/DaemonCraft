//----------------------------------------------------------------------------------------------------
// ChunkSaveJob.cpp - Asynchronous chunk saving implementation
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/ChunkSaveJob.hpp"
#include "Game/Framework/Chunk.hpp"

//----------------------------------------------------------------------------------------------------
ChunkSaveJob::ChunkSaveJob(Chunk* chunk)
    : Job(JOB_TYPE_IO),  // Mark as I/O job - only I/O workers will claim this
      m_chunk(chunk)
{
    // Transition chunk to saving state
    // Note: Will need to add SAVING state to ChunkState enum
    // For now, this constructor just stores the chunk pointer
}

//----------------------------------------------------------------------------------------------------
void ChunkSaveJob::Execute()
{
    if (!m_chunk)
    {
        m_wasSuccessful = false;
        return;
    }

    try
    {
        // Save chunk data to disk (thread-safe, no GPU calls)
        // Chunk::SaveToDisk() will be implemented in Task 5.2
        m_wasSuccessful = m_chunk->SaveToDisk();

        // Note: Even if save fails (disk full, permission denied, etc.),
        // we mark the operation as complete to avoid blocking deactivation
        // The failure is logged but the chunk lifecycle continues
    }
    catch (...)
    {
        // Catch any exceptions during file I/O
        m_wasSuccessful = false;
        // In production, this should log the error for debugging
    }
}
