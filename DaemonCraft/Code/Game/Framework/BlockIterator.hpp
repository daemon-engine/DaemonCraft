//----------------------------------------------------------------------------------------------------
// BlockIterator.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once

#include "Engine/Math/IntVec3.hpp"
#include <functional>  // For std::hash specialization

//-Forward-Declaration--------------------------------------------------------------------------------
class Chunk;
class Block;
class World;

//----------------------------------------------------------------------------------------------------
// BlockIterator - Efficient block iteration with directional movement using bit operations
// Assignment 5 Phase 2: Added cross-chunk navigation support via World pointer
//----------------------------------------------------------------------------------------------------
class BlockIterator
{
public:
    // Constructor
    explicit BlockIterator(Chunk* chunk, int blockIndex = 0, World* world = nullptr);

    // Basic access
    Block*  GetBlock() const;
    Chunk*  GetChunk() const { return m_chunk; }
    int     GetBlockIndex() const { return m_blockIndex; }
    IntVec3 GetLocalCoords() const;
    bool    IsValid() const;

    // Directional movement (returns true if movement was successful)
    bool MoveNorth();   // +Y direction
    bool MoveSouth();   // -Y direction
    bool MoveEast();    // +X direction
    bool MoveWest();    // -X direction
    bool MoveUp();      // +Z direction
    bool MoveDown();    // -Z direction

    // Movement by offset
    bool MoveByOffset(IntVec3 const& offset);

    // Create iterator at neighboring position (returns invalid iterator if out of bounds)
    BlockIterator GetNeighbor(IntVec3 const& offset) const;
    BlockIterator GetNorthNeighbor() const;
    BlockIterator GetSouthNeighbor() const;
    BlockIterator GetEastNeighbor() const;
    BlockIterator GetWestNeighbor() const;
    BlockIterator GetUpNeighbor() const;
    BlockIterator GetDownNeighbor() const;

    // Equality comparison for std::unordered_set (Assignment 5 Phase 7)
    bool operator==(BlockIterator const& other) const
    {
        return m_chunk == other.m_chunk && m_blockIndex == other.m_blockIndex;
    }

private:
    Chunk* m_chunk      = nullptr;
    int    m_blockIndex = 0;
    World* m_world      = nullptr;  // Assignment 5 Phase 2: For cross-chunk navigation

    // Helper methods for bit operations
    bool IsIndexValid(int index) const;
    int  CalculateIndexFromOffset(IntVec3 const& offset) const;
};

//----------------------------------------------------------------------------------------------------
// std::hash specialization for BlockIterator (Assignment 5 Phase 7: O(1) duplicate detection)
//----------------------------------------------------------------------------------------------------
namespace std
{
    template <>
    struct hash<BlockIterator>
    {
        size_t operator()(BlockIterator const& iter) const noexcept
        {
            // Combine chunk pointer and block index into unique hash
            // Use XOR and bit shifting to mix the two values
            size_t h1 = std::hash<void*>{}(iter.GetChunk());
            size_t h2 = std::hash<int>{}(iter.GetBlockIndex());
            return h1 ^ (h2 << 1);  // Shift and XOR for good distribution
        }
    };
}
