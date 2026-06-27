//----------------------------------------------------------------------------------------------------
// BlockIterator.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/BlockIterator.hpp"
#include "Game/Framework/Block.hpp"
#include "Game/Framework/Chunk.hpp"
#include "Game/Gameplay/World.hpp"  // Assignment 5 Phase 2: For cross-chunk navigation

//----------------------------------------------------------------------------------------------------
BlockIterator::BlockIterator(Chunk*    chunk,
                             int const blockIndex,
                             World*    world)
    : m_chunk(chunk),
      m_blockIndex(blockIndex),
      m_world(world)
{
}

//----------------------------------------------------------------------------------------------------
Block* BlockIterator::GetBlock() const
{
    if (!IsValid()) return nullptr;

    IntVec3 const localCoords = GetLocalCoords();

    return m_chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
}

//----------------------------------------------------------------------------------------------------
IntVec3 BlockIterator::GetLocalCoords() const
{
    return Chunk::IndexToLocalCoords(m_blockIndex);
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::IsValid() const
{
    return m_chunk != nullptr && IsIndexValid(m_blockIndex);
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveNorth()
{
    return MoveByOffset(IntVec3(0, 1, 0));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveSouth()
{
    return MoveByOffset(IntVec3(0, -1, 0));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveEast()
{
    return MoveByOffset(IntVec3(1, 0, 0));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveWest()
{
    return MoveByOffset(IntVec3(-1, 0, 0));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveUp()
{
    return MoveByOffset(IntVec3(0, 0, 1));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveDown()
{
    return MoveByOffset(IntVec3(0, 0, -1));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveByOffset(IntVec3 const& offset)
{
    if (!IsValid()) return false;

    int const newIndex = CalculateIndexFromOffset(offset);

    if (IsIndexValid(newIndex))
    {
        m_blockIndex = newIndex;
        return true;
    }

    return false;
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 2: Enhanced GetNeighbor() with cross-chunk navigation support
//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetNeighbor(IntVec3 const& offset) const
{
    if (!IsValid()) return BlockIterator(nullptr, -1, m_world);

    IntVec3 const currentCoords = GetLocalCoords();
    IntVec3 const newCoords     = currentCoords + offset;

    // Check if neighbor is within same chunk
    if (newCoords.x >= 0 && newCoords.x < CHUNK_SIZE_X &&
        newCoords.y >= 0 && newCoords.y < CHUNK_SIZE_Y &&
        newCoords.z >= 0 && newCoords.z < CHUNK_SIZE_Z)
    {
        int const newIndex = Chunk::LocalCoordsToIndex(newCoords);
        return BlockIterator(m_chunk, newIndex, m_world);
    }

    // Neighbor is in adjacent chunk - need cross-chunk navigation
    if (!m_world) return BlockIterator(nullptr, -1, m_world);  // No world reference available

    // Calculate which neighboring chunk we need
    IntVec2 const currentChunkCoords = m_chunk->GetChunkCoords();
    IntVec2       neighborChunkOffset(0, 0);

    // Determine chunk offset based on which boundary we crossed
    if (newCoords.x < 0)                  neighborChunkOffset.x = -1;  // Crossed west boundary
    if (newCoords.x >= CHUNK_SIZE_X)      neighborChunkOffset.x = 1;   // Crossed east boundary
    if (newCoords.y < 0)                  neighborChunkOffset.y = -1;  // Crossed south boundary
    if (newCoords.y >= CHUNK_SIZE_Y)      neighborChunkOffset.y = 1;   // Crossed north boundary

    IntVec2 const neighborChunkCoords = currentChunkCoords + neighborChunkOffset;
    Chunk*  const neighborChunk       = m_world->GetChunk(neighborChunkCoords);

    if (!neighborChunk) return BlockIterator(nullptr, -1, m_world);  // Neighbor chunk not loaded

    // Calculate local coordinates within neighbor chunk (wrap around using modulo)
    IntVec3 neighborLocalCoords;
    neighborLocalCoords.x = (newCoords.x + CHUNK_SIZE_X) % CHUNK_SIZE_X;  // Wrap negative/overflow X
    neighborLocalCoords.y = (newCoords.y + CHUNK_SIZE_Y) % CHUNK_SIZE_Y;  // Wrap negative/overflow Y
    neighborLocalCoords.z = newCoords.z;                                   // Z never crosses chunk boundary

    // Vertical out of bounds check (no neighbor chunk in Z direction)
    if (neighborLocalCoords.z < 0 || neighborLocalCoords.z >= CHUNK_SIZE_Z)
    {
        return BlockIterator(nullptr, -1, m_world);
    }

    int const neighborIndex = Chunk::LocalCoordsToIndex(neighborLocalCoords);
    return BlockIterator(neighborChunk, neighborIndex, m_world);
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetNorthNeighbor() const
{
    return GetNeighbor(IntVec3(0, 1, 0));
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetSouthNeighbor() const
{
    return GetNeighbor(IntVec3(0, -1, 0));
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetEastNeighbor() const
{
    return GetNeighbor(IntVec3(1, 0, 0));
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetWestNeighbor() const
{
    return GetNeighbor(IntVec3(-1, 0, 0));
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetUpNeighbor() const
{
    return GetNeighbor(IntVec3(0, 0, 1));
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetDownNeighbor() const
{
    return GetNeighbor(IntVec3(0, 0, -1));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::IsIndexValid(int const index) const
{
    return index >= 0 && index < BLOCKS_PER_CHUNK;
}

//----------------------------------------------------------------------------------------------------
int BlockIterator::CalculateIndexFromOffset(IntVec3 const& offset) const
{
    IntVec3 const currentCoords = GetLocalCoords();
    IntVec3 const newCoords     = currentCoords + offset;

    // Check bounds using bit operations for efficiency
    if (newCoords.x < 0 || newCoords.x >= CHUNK_SIZE_X ||
        newCoords.y < 0 || newCoords.y >= CHUNK_SIZE_Y ||
        newCoords.z < 0 || newCoords.z >= CHUNK_SIZE_Z)
    {
        return -1;  // Out of bounds
    }

    // Use bit operations for fast index calculation
    return Chunk::LocalCoordsToIndex(newCoords);
}
