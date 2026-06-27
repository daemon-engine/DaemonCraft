[🏠 Root](../../CLAUDE.md) > **[📦 Run]**

**Navigation:** [Back to Root](../../CLAUDE.md) | [Game Module](../../Code/Game/CLAUDE.md) | [Framework](../../Code/Game/Framework/CLAUDE.md) | [Gameplay](../../Code/Game/Gameplay/CLAUDE.md)

---

# Run Module - Runtime Assets and Executables

## Quick Navigation
- **[Game Module](../../Code/Game/CLAUDE.md)** - Core game logic and entry points
- **[Framework Module](../../Code/Game/Framework/CLAUDE.md)** - Core systems (**Assignment 4: Primary implementation**)
- **[Definition Module](../../Code/Game/Definition/CLAUDE.md)** - BlockDefinition configuration
- **[Development Plan](../../.claude/plan/development.md)** - Assignment 4: World Generation
- **[Task Pointer](../../.claude/plan/task-pointer.md)** - Quick task reference

---

## Assignment 4: World Generation - Asset Requirements

**Status:** Phase 5A - Carvers (Ravines/Rivers) ✅ MOSTLY COMPLETED (Date Unknown)

**Asset Integration:** New sprite sheets and block definitions have been successfully integrated to support biome-specific blocks (trees, leaves, surface variants).

### Completed Asset Updates (Phase 1, Task 1.1)

**✅ COMPLETED:**
1. **BlockSpriteSheet_BlockDefinitions.xml**
   - Location: `Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml`
   - Status: **REPLACED** with new file from Canvas assignment zip
   - Contents: New block types for trees (wood, leaves), biome-specific surfaces
   - Block types added: Oak/Spruce/Jungle/Palm wood, various leaf types, grass variants, snow, ice

2. **New Sprite Sheets**
   - Location: `Data/Images/`
   - Status: **ADDED** new sprite sheet files from Canvas assignment zip
   - Contents: Biome-specific block textures (oak/pine/palm trees, varied leaves, etc.)
   - Resolution: Both 32px and 128px versions

**Verification Completed:**
- ✅ Downloaded assignment zip from Canvas
- ✅ Replaced `Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml`
- ✅ Added new sprite sheets to `Data/Images/`
- ✅ Launched game and verified assets load correctly
- ✅ Documented new block type indices in GameCommon.hpp

### New Block Types Expected

Based on Assignment 4 requirements, the new sprite sheet should include:
- **Wood Types**: Oak, Pine, Palm (for tree trunks)
- **Leaf Types**: Oak leaves, Pine needles, Palm fronds
- **Surface Variants**: Biome-specific grass, sand, snow textures
- **Underground**: Cave-specific blocks (stalactites/stalagmites if supported)

**Block Index Documentation:** After loading new definitions, update [Framework/GameCommon.hpp](../../Code/Game/Framework/CLAUDE.md) with new block type constants.

**Resources:**
- [Development Plan](../../.claude/plan/development.md) - Phase 1, Task 1.1: Asset Integration
- [Task Pointer](../../.claude/plan/task-pointer.md) - Quick reference for asset setup

---

## Module Responsibilities

The Run module contains all runtime assets, executables, and configuration files required to execute SimpleMiner. This directory serves as the working directory for the game application and contains data-driven configurations, 3D assets, shaders, audio files, and all necessary runtime libraries. The module follows a structured asset organization that supports the game's rendering pipeline and gameplay systems.

## Entry and Startup

### Executable Files
- **SimpleMiner_Debug_x64.exe**: Debug build with full debugging symbols and logging
- **SimpleMiner_Release_x64.exe**: Optimized release build for production
- **DaemonCraft_Debug_x64.exe**: Legacy executable name (being transitioned)
- **DaemonCraft_Release_x64.exe**: Legacy release executable

### Runtime Libraries
- **V8 JavaScript Engine**: v8.dll, v8_libbase.dll, v8_libplatform.dll
- **ICU Internationalization**: icuuc.dll, third_party_icu_icui18n.dll
- **FMOD Audio**: fmod.dll, fmod64.dll for professional audio support
- **Compression**: third_party_zlib.dll, third_party_abseil-cpp_absl.dll

## External Interfaces

### Configuration System
- **GameConfig.xml**: Primary game configuration
  - Window properties (size: 1600x800, center position)
  - Display settings and window behavior
  - Runtime toggles and system preferences

### Block Definition System
- **BlockSpriteSheet_BlockDefinitions.xml**: Data-driven block properties
  - 27 defined block types (Air, Grass, Stone, ores, wood types, etc.)
  - Visual properties: visibility, opacity, texture coordinates
  - Physical properties: solidity, collision detection
  - Lighting properties: emission values for light sources (Glowstone)

### Asset Loading Pipeline
- **Texture Atlas**: BlockSpriteSheet_128px.png and BlockSpriteSheet_32px.png
- **3D Models**: Cube variants, Tutorial_Box_Phong, Woman character model
- **Audio Assets**: TestSound.mp3 for audio system validation
- **Fonts**: DaemonFont.png, SquirrelFixedFont.png for UI text rendering

## Key Dependencies and Configuration

### Shader System (HLSL DirectX 11)
```hlsl
// Default.hlsl - Basic unlit rendering with Vertex_PCU format
// BlinnPhong.hlsl - Phong lighting model for realistic materials  
// Bloom.hlsl - Post-processing effects for enhanced visuals
```

### Data Directory Structure
```
Run/Data/
├── Audio/              # Sound effects and music
├── Definitions/        # XML configuration files
├── Fonts/             # Typography resources
├── Images/            # Textures and UI graphics
├── Models/            # 3D model assets (.obj, .FBX files)
├── Shaders/           # HLSL shader programs
└── GameConfig.xml     # Main configuration
```

### Asset Format Support
- **3D Models**: .obj, .FBX formats with material definitions
- **Textures**: .png, .tga formats with sprite sheet organization
- **Audio**: .mp3 format with FMOD processing
- **Configuration**: .xml for structured data definitions

## Data Models

### Block Definition Schema
```xml
<BlockDefinition name="Grass" 
                isVisible="true" 
                isSolid="true" 
                isOpaque="true" 
                topSpriteCoords="0, 0" 
                bottomSpriteCoords="2, 0" 
                sideSpriteCoords="1, 0"/>
```

### Texture Atlas System
- **Sprite Coordinates**: Grid-based UV mapping (x, y) coordinates
- **Multiple Resolutions**: 32px and 128px versions for different quality levels
- **Face-Specific Textures**: Top, bottom, and side faces can use different sprites

### Model Asset Organization
- **Cube Models**: Basic geometry variants (v, vi, vn, vni formats)
- **Character Models**: Woman model with diffuse and normal maps
- **Environment**: Tutorial_Box with complete material setup (diffuse, normal, specular)

## Testing and Quality

### Asset Validation
- **XML Schema**: Well-formed XML with consistent attribute naming
- **Texture Integrity**: Proper sprite sheet alignment and UV coordinates
- **Model Loading**: Vertex format compatibility with engine expectations
- **Audio Testing**: FMOD integration validation with test audio files

### Performance Considerations
- **Texture Resolution**: Multiple sizes for LOD and performance scaling
- **Model Complexity**: Optimized vertex counts for real-time rendering
- **Asset Streaming**: Efficient loading patterns for large world data
- **Memory Usage**: Conservative texture and model memory management

### Build Integration
- **Post-Build Events**: Automatic copying of executables from Temporary/ to Run/
- **DLL Deployment**: V8 runtime libraries copied based on build configuration
- **Asset Synchronization**: Game data remains in Run/ for consistent access

## FAQ

**Q: Why are there both SimpleMiner and DaemonCraft executables?**
A: This appears to be a naming transition. The project is moving from "DaemonCraft" to "SimpleMiner" branding, with both executables currently present during the transition.

**Q: How does the texture atlas system work?**
A: Block faces reference sprite coordinates (x, y) in the BlockSpriteSheet. Each block type can specify different textures for top, bottom, and side faces using these coordinates.

**Q: What is the purpose of multiple model variants (Cube_v, Cube_vi, etc.)?**
A: Different vertex formats: 'v' = vertices only, 'vi' = vertices+indices, 'vn' = vertices+normals, 'vni' = vertices+normals+indices for different rendering needs.

**Q: How are runtime libraries managed?**
A: V8 and other third-party DLLs are automatically copied by post-build events based on Debug/Release configuration. The game loads these dynamically at runtime.

**Q: Can game configuration be modified without rebuilding?**
A: Yes, GameConfig.xml and BlockSpriteSheet_BlockDefinitions.xml are loaded at runtime, allowing configuration changes without recompilation.

## Related File List

### Configuration Files
- `Data/GameConfig.xml` - Main game settings
- `Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml` - Block type definitions

### Rendering Assets
- `Data/Shaders/Default.hlsl` - Basic rendering shader
- `Data/Shaders/BlinnPhong.hlsl` - Lighting shader
- `Data/Shaders/Bloom.hlsl` - Post-processing shader
- `Data/Images/BlockSpriteSheet_*.png` - Texture atlas files

### 3D Assets
- `Data/Models/Cube/` - Basic geometry variants
- `Data/Models/TutorialBox_Phong/` - Complete material example
- `Data/Models/Woman/` - Character model with textures

### Runtime Files
- `SimpleMiner_*.exe` - Game executables
- `fmod*.dll` - Audio system libraries
- `v8*.dll` - JavaScript engine libraries
- `icu*.dll` - Internationalization libraries

## Changelog

- **2025-11-08**: Assignment 4 Phase 5A (Carvers) completed in Framework module
  - Assets continue to support all terrain generation features
  - BLOCK_WATER and BLOCK_SAND definitions used by river carver system
- **2025-11-03**: Completed Phase 2: 3D Density Terrain (Tasks 2.1-2.5) for Assignment 4: World Generation
  - Core chunk management system successfully integrated with 3D density terrain generation
  - Assets (sprite sheets and block definitions) verified and working with biome-specific surface blocks
  - Terrain now generates with proper biome-specific heights and surface materials
  - Surface blocks and tree features fully implemented with comprehensive biome coverage
  - Cross-chunk tree placement system ensures natural tree distribution across boundaries
- **2025-11-03**: Completed Phase 1: Foundation (Tasks 1.1-1.4) for Assignment 4: World Generation
  - ✅ Task 1.1: Asset integration completed (new sprite sheets and BlockDefinitions.xml)
  - ✅ Tasks 1.2-1.4: Biome system fully operational with 16 biome types
- **2025-11-01**: Completed Phase 0 prerequisites (Tasks 0.1-0.7) for Assignment 4: World Generation
  - Core chunk management system optimized and ready for biome integration
- **2025-10-26**: Updated documentation for Assignment 4: World Generation
  - Added Assignment 4 Asset Requirements section detailing critical file replacements
  - Added navigation breadcrumbs and quick navigation
  - Documented new sprite sheets and block definitions from Canvas assignment zip
  - Added verification steps for asset integration (Phase 1, Task 1.1)
  - Enhanced inline cross-references to Framework and Definition modules
  - Linked to development planning resources
- **2025-09-13**: Initial Run module documentation created
- **Recent**: Added comprehensive block definitions with 27+ block types
- **Recent**: Texture atlas system with multiple resolution support
- **Recent**: V8 JavaScript runtime integration with automatic DLL deployment