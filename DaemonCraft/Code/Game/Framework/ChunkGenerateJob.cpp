//----------------------------------------------------------------------------------------------------
// ChunkGenerateJob.cpp - Implementation of asynchronous chunk terrain generation
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/ChunkGenerateJob.hpp"
#include "Game/Framework/Chunk.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

//----------------------------------------------------------------------------------------------------
ChunkGenerateJob::ChunkGenerateJob(Chunk* chunk)
    : m_chunk(chunk)
{
    GUARANTEE_OR_DIE(m_chunk != nullptr, "ChunkGenerateJob created with null chunk pointer");
    
    // Verify chunk is in the correct state for terrain generation
    ChunkState currentState = m_chunk->GetState();
    GUARANTEE_OR_DIE(currentState == ChunkState::TERRAIN_GENERATING, 
                     "ChunkGenerateJob created for chunk not in TERRAIN_GENERATING state");
}

//----------------------------------------------------------------------------------------------------
void ChunkGenerateJob::Execute()
{
    // Validate chunk state before starting work
    ValidateChunkState();
    
    try
    {
        // Perform the actual terrain generation
        GenerateTerrainData();
        
        // Mark as successful
        m_wasSuccessful = true;
        
        // Transition chunk to next state (lighting initialization)
        bool stateChanged = m_chunk->CompareAndSetState(ChunkState::TERRAIN_GENERATING, 
                                                       ChunkState::LIGHTING_INITIALIZING);
        
        if (!stateChanged)
        {
            // Something else changed the chunk state - this shouldn't happen
            ERROR_AND_DIE("ChunkGenerateJob: Failed to transition chunk state from TERRAIN_GENERATING to LIGHTING_INITIALIZING");
        }
    }
    catch (...)
    {
        // Mark as failed and leave chunk in TERRAIN_GENERATING state
        m_wasSuccessful = false;
        
        // In a production system, you might want to transition to an ERROR state
        // For now, we'll leave it in TERRAIN_GENERATING so it can be retried
    }
}

//----------------------------------------------------------------------------------------------------
IntVec2 ChunkGenerateJob::GetChunkCoords() const
{
    if (m_chunk)
    {
        return m_chunk->GetChunkCoords();
    }
    return IntVec2::ZERO;
}

//----------------------------------------------------------------------------------------------------
void ChunkGenerateJob::GenerateTerrainData()
{
    if (!m_chunk)
    {
        return;
    }
    
    // Call the existing terrain generation method
    // This method is thread-safe for chunks in TERRAIN_GENERATING state
    // because only worker threads access block data during this phase
    m_chunk->GenerateTerrain();
}

//----------------------------------------------------------------------------------------------------
void ChunkGenerateJob::ValidateChunkState()
{
    if (!m_chunk)
    {
        ERROR_AND_DIE("ChunkGenerateJob: Null chunk pointer during execution");
    }
    
    ChunkState currentState = m_chunk->GetState();
    if (currentState != ChunkState::TERRAIN_GENERATING)
    {
        ERROR_AND_DIE("ChunkGenerateJob: Chunk not in TERRAIN_GENERATING state during execution");
    }
}