[🏠 Root](../../../CLAUDE.md) > [📂 Code](../../) > [📂 Game](../) > **[📋 Definition]**

**Navigation:** [Back to Game](../CLAUDE.md) | [Framework](../Framework/CLAUDE.md) | [Gameplay](../Gameplay/CLAUDE.md) | [Root](../../../CLAUDE.md)

---

# Definition Module - Data-Driven Block Configuration

## Quick Navigation
- **[Game Module](../CLAUDE.md)** - Core game logic and entry points
- **[Framework Module](../Framework/CLAUDE.md)** - Block and Chunk systems (**Assignment 4: Primary implementation**)
- **[Gameplay Module](../Gameplay/CLAUDE.md)** - World and Entity systems
- **[Runtime Assets](../../../Run/CLAUDE.md)** - XML definitions and sprite sheets
- **[Development Plan](../../../.claude/plan/development.md)** - Assignment 4: World Generation
- **[Task Pointer](../../../.claude/plan/task-pointer.md)** - Quick task reference

---

## Assignment 4: World Generation - Block Definition Requirements

**Status:** Phase 0 - Prerequisites ✅ COMPLETED (2025-11-01)

**CRITICAL:** Assignment 4 requires new block definitions for biome-specific blocks (trees, leaves, surface variants). The XML configuration file must be replaced before implementation begins.

### Required Updates

**BlockDefinition XML (Phase 1, Task 1.1):**
- **File**: [Run/Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml](../../../Run/CLAUDE.md)
- **Action**: REPLACE with new file from Canvas assignment zip
- **New Block Types**:
  - Wood types: Oak, Pine, Palm (tree trunks)
  - Leaf types: Oak leaves, Pine needles, Palm fronds
  - Surface variants: Biome-specific grass, sand, snow
  - Underground: Cave-specific blocks (if supported)

**Code Updates Required:**
- Document new block type indices after loading new XML
- Add block type constants to [Framework/GameCommon.hpp](../Framework/CLAUDE.md)
- Update [Framework/Chunk.cpp](../Framework/CLAUDE.md) to use new block types in terrain generation

**Resources:**
- [Development Plan](../../../.claude/plan/development.md) - Phase 1: Asset Integration
- [Runtime Assets](../../../Run/CLAUDE.md) - Asset file locations

---

## Module Responsibilities

The Definition module provides the data-driven configuration system for block types in SimpleMiner. It loads XML definitions and creates runtime [BlockDefinition](CLAUDE.md) objects that define visual, physical, and lighting properties for each block type. This system enables designers to modify block behavior without code changes.

See [Framework module](../Framework/CLAUDE.md) for how [Block](../Framework/CLAUDE.md) instances reference these definitions.

---

## Entry and Startup

### BlockDefinition Loading
- **File**: `BlockDefinition.cpp`
- **Method**: `BlockDefinition::LoadDefinitions()`
- **Source**: XML file at [Run/Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml](../../../Run/CLAUDE.md)
- **Timing**: Called during [App startup](../Framework/CLAUDE.md)
- **Storage**: Global static vector of BlockDefinition objects

### Definition Lifecycle
1. **Startup**: Parse XML, create BlockDefinition objects
2. **Runtime**: [Block instances](../Framework/CLAUDE.md) reference definitions by index
3. **Shutdown**: Clean up definition objects

---

## External Interfaces

### XML Configuration Format

```xml
<BlockDefinition name="Grass"
                isVisible="true"
                isSolid="true"
                isOpaque="true"
                topSpriteCoords="0, 0"
                bottomSpriteCoords="2, 0"
                sideSpriteCoords="1, 0"/>
```

### BlockDefinition Class

```cpp
class BlockDefinition {
public:
    std::string m_name;              // Block type name
    bool m_isVisible;                // Render this block?
    bool m_isSolid;                  // Collision enabled?
    bool m_isOpaque;                 // Blocks light?
    IntVec2 m_topSpriteCoords;       // UV coords for top face
    IntVec2 m_bottomSpriteCoords;    // UV coords for bottom face
    IntVec2 m_sideSpriteCoords;      // UV coords for side faces

    // Lighting (if implemented)
    Rgba8 m_emissionColor;           // Light emission (e.g., Glowstone)

    // Static methods
    static void LoadDefinitions(std::string const& path);
    static BlockDefinition const* GetDefinition(uint8_t typeIndex);
    static uint8_t GetTypeIndexByName(std::string const& name);
};
```

---

## Key Dependencies and Configuration

### XML Schema
- **Root Element**: `<BlockDefinitions>`
- **Child Elements**: `<BlockDefinition>` (one per block type)
- **Required Attributes**: name, isVisible, isSolid, isOpaque
- **Optional Attributes**: topSpriteCoords, bottomSpriteCoords, sideSpriteCoords, emissionColor

### Sprite Sheet Integration
- **Texture Atlas**: [Run/Data/Images/BlockSpriteSheet_*.png](../../../Run/CLAUDE.md)
- **Coordinate System**: Grid-based (x, y) indices into sprite sheet
- **Face-Specific Textures**: Different textures for top, bottom, and sides
- **Resolution Support**: Multiple sprite sheet resolutions (32px, 128px)

### Block Type Indexing
- **Index 0**: Always AIR (invisible, non-solid)
- **Indices 1-N**: Defined in XML order
- **Flyweight Pattern**: [Block](../Framework/CLAUDE.md) stores only `uint8_t m_typeIndex`
- **Lookup**: `BlockDefinition::GetDefinition(typeIndex)` returns shared definition

---

## Data Models

### Block Property Categories

**Visual Properties:**
- `m_isVisible` - Whether block renders
- Sprite coordinates for each face
- Texture mapping via sprite sheet

**Physical Properties:**
- `m_isSolid` - Collision detection enabled
- Determines walkability and block placement

**Lighting Properties:**
- `m_isOpaque` - Blocks light propagation
- `m_emissionColor` - Light emission (e.g., Glowstone, Lava)

**Metadata:**
- `m_name` - Human-readable name
- Type index (implicit, based on load order)

---

## Testing and Quality

### XML Validation
- **Well-formed XML**: Proper element nesting and closure
- **Required Attributes**: All mandatory attributes present
- **Type Safety**: Boolean values ("true"/"false"), integer coordinates
- **Sprite Bounds**: Coordinates within sprite sheet dimensions

### Definition Loading
- **Error Handling**: Graceful failure on XML parse errors
- **Missing Files**: Report if XML not found
- **Duplicate Names**: Warn if block names conflict
- **Index Consistency**: Maintain same order across sessions

### Performance Characteristics
- **Load Time**: One-time cost at startup (~ms for 30-50 blocks)
- **Runtime Lookup**: O(1) array access by type index
- **Memory Usage**: Minimal, shared definitions (~1KB total for all blocks)

---

## FAQ

**Q: How are new block types added?**
A: Add `<BlockDefinition>` entries to XML file. Definitions load in order, creating sequential type indices.

**Q: Can block properties change at runtime?**
A: No, definitions are immutable after loading. Changing behavior requires XML edit and restart.

**Q: How does the Block flyweight pattern work?**
A: [Block instances](../Framework/CLAUDE.md) store only a `uint8_t` type index. All properties retrieved from shared BlockDefinition via `GetDefinition(typeIndex)`.

**Q: What happens if sprite coordinates are invalid?**
A: UV mapping may render incorrect textures. Validation during load recommended but not currently enforced.

**Q: How are biome-specific blocks handled?**
A: BlockDefinition defines properties, [Chunk terrain generation](../Framework/CLAUDE.md) decides which blocks to place based on biome logic.

---

## Related File List

### Core Definition Files
- `BlockDefinition.hpp` - BlockDefinition class declaration
- `BlockDefinition.cpp` - XML loading and definition management (**Assignment 4: May need new block constants**)

### Configuration Files
- [Run/Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml](../../../Run/CLAUDE.md) - Block properties (**Assignment 4: REPLACE with new file**)
- [Run/Data/Images/BlockSpriteSheet_*.png](../../../Run/CLAUDE.md) - Sprite sheet textures (**Assignment 4: ADD new files**)

### Dependencies
- [Framework/Block.hpp/cpp](../Framework/CLAUDE.md) - Uses definitions via type index
- [Framework/Chunk.cpp](../Framework/CLAUDE.md) - Places blocks during terrain generation (**Assignment 4: PRIMARY FILE**)
- [Framework/GameCommon.hpp](../Framework/CLAUDE.md) - Block type constants (**Assignment 4: ADD new constants**)

### Related Modules
- **[Game Module](../CLAUDE.md)** - Entry points and build configuration
- **[Framework Module](../Framework/CLAUDE.md)** - Block and Chunk systems
- **[Gameplay Module](../Gameplay/CLAUDE.md)** - World management
- **[Runtime Assets](../../../Run/CLAUDE.md)** - Asset files (**Assignment 4: Critical updates required**)
- **[Development Plan](../../../.claude/plan/development.md)** - Assignment 4 implementation guide

---

## Changelog

- **2025-11-08**: Assignment 4 Phase 5A (Carvers) completed in Framework module
  - Block definitions continue to support all terrain generation phases
  - BLOCK_WATER and BLOCK_SAND used by river carver for water channels and riverbeds
- **2025-11-01**: Completed Phase 0 prerequisites (Tasks 0.1-0.7) for Assignment 4: World Generation
  - Core systems optimized and ready for new block definitions
  - Phase 1, Task 1.1 completed: XML replacement with biome-specific block types
- **2025-10-26**: Initial Definition module documentation created
  - Documented BlockDefinition system and XML configuration
  - Added Assignment 4 context for new block types and asset requirements
  - Added navigation breadcrumbs and quick navigation
  - Documented XML schema, sprite sheet integration, and block property categories
  - Enhanced inline cross-references to Framework, Gameplay, and Run modules
  - Linked to development planning resources
  - Noted requirement to replace XML and add new sprite sheets (Phase 1, Task 1.1)
