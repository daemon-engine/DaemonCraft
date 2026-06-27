//----------------------------------------------------------------------------------------------------
// Block.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/Block.hpp"
#include <cstddef>  // For offsetof

//----------------------------------------------------------------------------------------------------
// Compile-time verification: Block structure size and layout
//----------------------------------------------------------------------------------------------------
static_assert(sizeof(Block) == 3, "Block must be exactly 3 bytes for Assignment 5 lighting system");
static_assert(offsetof(Block, m_typeIndex) == 0, "m_typeIndex must be at offset 0");
static_assert(offsetof(Block, m_lightingData) == 1, "m_lightingData must be at offset 1");
static_assert(offsetof(Block, m_bitFlags) == 2, "m_bitFlags must be at offset 2");
