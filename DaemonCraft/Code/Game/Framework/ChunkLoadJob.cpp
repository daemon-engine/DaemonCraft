//----------------------------------------------------------------------------------------------------
// ChunkLoadJob.cpp - Asynchronous chunk loading implementation
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/ChunkLoadJob.hpp"
#include "Game/Framework/Chunk.hpp"

//----------------------------------------------------------------------------------------------------
ChunkLoadJob::ChunkLoadJob(Chunk* chunk)
    : Job(JOB_TYPE_IO),  // Mark as I/O job - only I/O workers will claim this
      m_chunk(chunk)
{
    // Transition chunk to loading state
    // Note: Will need to add LOADING state to ChunkState enum
    // For now, this constructor just stores the chunk pointer
}

//----------------------------------------------------------------------------------------------------
void ChunkLoadJob::Execute()
{
    if (!m_chunk)
    {
        m_wasSuccessful = false;
        return;
    }

    try
    {
        // Load chunk data from disk (thread-safe, no GPU calls)
        m_wasSuccessful = m_chunk->LoadFromDisk();

        if (m_wasSuccessful)
        {
            // Assignment 5: Initialize lighting for loaded chunks
            // CRITICAL FIX: Chunks loaded from disk have old lighting data (all outdoor=0)
            // InitializeLighting() sets air blocks to outdoor=15 and opaque blocks to outdoor=0
            m_chunk->InitializeLighting();

            // Transition to load complete state using atomic operation
            m_chunk->SetState(ChunkState::LOAD_COMPLETE);
        }
        else
        {
            // Load failed - chunk will fall back to terrain generation
            // Main thread will check m_wasSuccessful and decide what to do
        }
    }
    catch (...)
    {
        // Catch any exceptions during file I/O
        m_wasSuccessful = false;
    }
}
