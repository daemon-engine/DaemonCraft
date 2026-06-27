//----------------------------------------------------------------------------------------------------
// Block.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <cstdint>

#include "Engine/Core/ErrorWarningAssert.hpp"

// DebuggerPrintf is provided by ErrorWarningAssert.hpp for both Debug and Release

//----------------------------------------------------------------------------------------------------
//1. An ultra-flyweight voxel (volumetric element); one unit (1x1x1) of world-stuff.
//2. Each block knows its type, which is an index (stored as an unsigned char / uint8_t) into a global table of block definitions (see below).
//3. sizeof(Block) == 3 bytes (Assignment 5: Expanded for lighting system)
//4. In later assignments, each block will also store any additional information critical to its function on a per-block basis;
//   all other information (e.g., per-type) is stored on its block definition or elsewhere.
class Block
{
public:
    uint8_t m_typeIndex = 0;    // Block type index into BlockDefinition table - 1 byte
    uint8_t m_lightingData = 0; // High nibble: outdoor light (0-15), Low nibble: indoor light (0-15) - 1 byte
    uint8_t m_bitFlags = 0;     // Boolean flags (bit 0: isSkyVisible, bits 1-7: reserved) - 1 byte

    // Inline accessors for outdoor light (high nibble)
    inline uint8_t GetOutdoorLight() const { return (m_lightingData >> 4) & 0x0F; }
    inline void SetOutdoorLight(uint8_t value)
    {

        // BUG HUNT: Only log when outdoor light is CLEARED (non-zero -> 0)
        uint8_t oldValue = GetOutdoorLight();
        if (oldValue > 0 && value == 0)
        {
            DebuggerPrintf("[BUG HUNT] SetOutdoorLight: CLEARED outdoor light %d->0 at Block@0x%p\n", oldValue, this);
        }

        m_lightingData = (m_lightingData & 0x0F) | ((value & 0x0F) << 4);
    }

    // Inline accessors for indoor light (low nibble)
    inline uint8_t GetIndoorLight() const { return m_lightingData & 0x0F; }
    inline void SetIndoorLight(uint8_t value) { m_lightingData = (m_lightingData & 0xF0) | (value & 0x0F); }

    // Inline accessor for sky visibility flag (bit 0)
    inline bool IsSkyVisible() const { return (m_bitFlags & 0x01) != 0; }
    inline void SetIsSkyVisible(bool visible)
    {

        // BUG HUNT: Only log when isSkyVisible is CLEARED (true -> false)
        bool oldValue = IsSkyVisible();
        if (oldValue == true && visible == false)
        {
            DebuggerPrintf("[BUG HUNT] SetIsSkyVisible: CLEARED flag true->false at Block@0x%p m_bitFlags=0x%02X\n", this, m_bitFlags);
        }

        m_bitFlags = visible ? (m_bitFlags | 0x01) : (m_bitFlags & ~0x01);
    }

    // All other data lives in BlockDefinition
};
