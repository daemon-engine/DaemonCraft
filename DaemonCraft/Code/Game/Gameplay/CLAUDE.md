[🏠 Root](../../../CLAUDE.md) > [📂 Code](../../) > [📂 Game](../) > **[🌍 Gameplay]**

**Navigation:** [Back to Game](../CLAUDE.md) | [Framework](../Framework/CLAUDE.md) | [Definition](../Definition/CLAUDE.md) | [Root](../../../CLAUDE.md)

---

# Gameplay Module - Game Logic and World Systems

## Quick Navigation
- **[Game Module](../CLAUDE.md)** - Core game logic and entry points
- **[Framework Module](../Framework/CLAUDE.md)** - App, Block, Chunk systems (**Assignment 4: Primary implementation**)
- **[Definition Module](../Definition/CLAUDE.md)** - BlockDefinition configuration
- **[Runtime Assets](../../../Run/CLAUDE.md)** - Shaders, models, configurations
- **[Development Plan](../../../.claude/plan/development.md)** - Assignment 4: World Generation
- **[Task Pointer](../../../.claude/plan/task-pointer.md)** - Quick task reference

---

## Assignment 4: World Generation - Chunk Management Context

**Status:** Phase 0 - Prerequisites ✅ COMPLETED (2025-11-01)

This module manages the [World class](CLAUDE.md) which coordinates chunk activation and manages the multi-threaded chunk generation system. While the primary terrain generation occurs in [Chunk.cpp](../Framework/CLAUDE.md), this module handles:

**World Class Responsibilities:**
- Async chunk activation/deactivation based on player position
- Job queue management (ChunkGenerateJob, ChunkLoadJob, ChunkSaveJob)
- Thread-safe chunk state transitions with mutexes
- Processing completed generation jobs
- Mesh rebuilding coordination

**Changes for Assignment 4:**
- Chunk activation may be enhanced with biome-based loading strategies
- Job processing will handle new biome data and enhanced terrain complexity
- Mesh rebuilding may need performance optimization for complex 3D terrain

**Key Files:**
- `World.hpp/cpp` - Chunk coordination and job management
- `Game.hpp/cpp` - Game state and player management

**Resources:**
- [Development Plan](../../../.claude/plan/development.md) - Complete implementation phases
- [Framework Module](../Framework/CLAUDE.md) - Primary terrain generation implementation

---

## Module Responsibilities

The Gameplay module implements the core game mechanics, world management, and player systems for SimpleMiner. It coordinates the high-level game state, manages the voxel world through chunk systems, handles player input and movement, and provides the foundation for entity-based gameplay. This module contains the game-specific logic that sits above the Framework layer.

## Entry and Startup

### Game State Machine
- **Game Class**: Primary game controller with state management
- **States**: `eGameState::ATTRACT` (menu/title) and `eGameState::GAME` (active play)
- **Initialization**: Creates world, spawns player, initializes cameras and props
- **Update Pipeline**: Input handling → Entity updates → World updates

### World Initialization
- **World Class**: Container for all active chunks and spatial management
- **Multi-threaded Architecture**:
  - 4 mutexes protect shared data: `m_activeChunksMutex`, `m_nonActiveChunksMutex`, `m_jobListsMutex`, `m_queuedChunksMutex`
  - `m_nonActiveChunks` std::set tracks chunks being processed by worker threads
  - Atomic chunk state transitions coordinate between main and worker threads
- **Chunk Activation**: Dynamic async loading/generation based on player position
  - Burst mode: 5 chunks/frame when count < 20 (startup optimization)
  - Steady state: 1 chunk/frame when count >= 20
- **Job Queue Limiting**: MAX_PENDING_GENERATE_JOBS=16, MAX_PENDING_LOAD_JOBS=4, MAX_PENDING_SAVE_JOBS=4
- **Camera Management**: World camera separate from UI/debug cameras

## External Interfaces

### Player Input System
- **Keyboard Input**: `UpdateFromKeyBoard()` - Movement, actions, debug toggles
- **Controller Support**: `UpdateFromController()` - Gamepad input handling
- **Input Coordination**: `UpdateFromInput()` - Combines all input sources

### Entity Management
- **Base Entity Class**: Foundation for all dynamic game objects
- **Player Entity**: Specialized entity with camera, input, and movement
- **Prop System**: Static and dynamic world objects, environmental elements
- **Update Coordination**: `UpdateEntities()` manages entity lifecycle and interactions

### World-Chunk Interface
- **Chunk Activation**: `World::ActivateChunk()` submits async jobs
  - Checks if chunk file exists on disk
  - Submits `ChunkLoadJob` (I/O worker) or `ChunkGenerateJob` (generic worker)
  - Never blocks main thread
- **Chunk Deactivation**: `World::DeactivateChunk()` submits async save
  - Submits `ChunkSaveJob` if chunk modified (I/O worker)
  - Cleans up neighbor pointers without blocking
- **Spatial Queries**: `World::GetChunk()` retrieves chunks by coordinates (mutex-protected)
- **Job Processing**: `ProcessCompletedJobs()` handles all completed work
  - Single `RetrieveAllCompletedJobs()` call prevents job loss
  - Processes generation, load, and save jobs in one pass
  - Transitions chunks from worker states to main thread states
- **Mesh Rebuilding**: `ProcessDirtyChunkMeshes()` rebuilds 2 nearest dirty chunks per frame (main thread only)

## Key Dependencies and Configuration

### World Management System
```cpp
class World {
    // Active chunks (ready for rendering, main thread only)
    std::unordered_map<IntVec2, Chunk*> m_activeChunks;
    mutable std::mutex m_activeChunksMutex;

    // Non-active chunks (being processed by workers)
    std::set<Chunk*> m_nonActiveChunks;
    mutable std::mutex m_nonActiveChunksMutex;

    // Job tracking
    std::vector<ChunkGenerateJob*> m_chunkGenerationJobs;
    std::vector<ChunkLoadJob*>     m_chunkLoadJobs;
    std::vector<ChunkSaveJob*>     m_chunkSaveJobs;
    mutable std::mutex m_jobListsMutex;

    // Queued chunk tracking
    std::unordered_set<IntVec2> m_queuedGenerateChunks;
    mutable std::mutex m_queuedChunksMutex;

    Camera* m_worldCamera;  // Primary gameplay camera
};
```

**Thread Safety Rules:**
- Main thread: Adds/removes chunks from lists, handles D3D11 operations
- Worker threads: Access chunk blocks only during LOADING/TERRAIN_GENERATING/SAVING states
- All shared data access protected by mutexes with minimal lock duration
- No heavy work performed while holding mutexes

### Player System Architecture
- **Movement**: 3D character controller with collision detection
- **Camera Control**: First-person perspective with mouse/keyboard input
- **World Interaction**: Block placement/removal, item interaction
- **State Management**: Health, inventory, position tracking

### Entity-Component Architecture
- **Entity Base**: Position, rotation, scale, update/render virtuals
- **Prop Specialization**: Static world objects, decorative elements
- **Extensibility**: Foundation for future entity types (NPCs, animals, items)

## Data Models

### Spatial Coordinate Systems
```cpp
// World coordinates: Float-based 3D positions (Vec3)
// Chunk coordinates: Integer 2D grid positions (IntVec2)
// Local coordinates: Block positions within chunk (IntVec3)
```

### Game State Data
- **Player State**: Position, camera orientation, movement velocity
- **World State**: Active chunk list, loaded chunk boundaries
- **Game Mode**: Current state (attract/game), timing, progression

### Chunk Management
- **Dynamic Activation**: Chunks loaded based on player proximity
- **Memory Management**: Inactive chunks unloaded to conserve memory  
- **Seamless World**: Adjacent chunks align perfectly for continuous world

## Testing and Quality

### Performance Optimization
- **Chunk LOD**: Distance-based level of detail for chunk updates
- **Selective Updates**: Only update active chunks within player range
- **Entity Culling**: Entities outside view frustum skip expensive operations
- **Frame Rate**: Consistent 60fps target with deltaTime-based updates

### Game Loop Architecture
```cpp
void Game::Update() {
    UpdateFromInput();           // Process player input
    UpdateEntities(gameDelta, systemDelta);  // Update all entities
    UpdateWorld(gameDelta);      // Update world systems
}
```

### Debug and Development Tools
- **Attract Mode**: Debug/development state separate from gameplay
- **Player Basis Rendering**: Debug visualization of player coordinate system
- **Entity Debug Info**: Position, velocity, state visualization
- **World Debug**: Chunk boundaries, loading zones, performance metrics

## FAQ

**Q: How does the world coordinate system work?**
A: World uses floating-point Vec3 coordinates where each unit equals 1 meter. Players move continuously in this space while chunks provide discrete 16x16x128 block regions.

**Q: When are chunks activated/deactivated?**
A: Chunks are dynamically loaded based on player position. The exact distance algorithm needs investigation, but typically uses a radius around the player.

**Q: How does player movement relate to chunk boundaries?**
A: Player movement is continuous across chunk boundaries. The World class handles seamless transitions and ensures appropriate chunks are loaded.

**Q: What is the difference between Entity and Prop?**
A: Entity is the base class for all dynamic objects. Prop is a specialized Entity for static/decorative world objects. Player is another specialized Entity.

**Q: How does the camera system work?**
A: Multiple cameras exist: m_screenCamera for UI, m_worldCamera for 3D gameplay. The player controls the world camera through input.

## Related File List

### Core Gameplay Files
- `Game.hpp/cpp` - Main game controller and state machine
- `World.hpp/cpp` - World management and chunk coordination (**Assignment 4: Enhanced job processing**)
- `Player.hpp/cpp` - Player character controller and input
- `Entity.hpp/cpp` - Base class for dynamic game objects
- `Prop.hpp/cpp` - Static and dynamic world objects

### Framework Dependencies
- [Framework/Chunk.hpp](../Framework/CLAUDE.md) - Individual chunk management (**Assignment 4: PRIMARY FILE**)
- [Framework/Block.hpp](../Framework/CLAUDE.md) - Block data structure
- [Framework/App.hpp](../Framework/CLAUDE.md) - Application lifecycle coordination

### Definition Dependencies
- [Definition/BlockDefinition.hpp/cpp](../Definition/CLAUDE.md) - Block type definitions

### Engine Dependencies
- Engine math libraries for Vec3, camera, transformations
- Engine input system for keyboard/controller handling
- Engine rendering for entity and world visualization

### Related Modules
- **[Game Module](../CLAUDE.md)** - Entry points and build configuration
- **[Framework Module](../Framework/CLAUDE.md)** - Core systems (**Assignment 4: Primary implementation**)
- **[Definition Module](../Definition/CLAUDE.md)** - Block configuration
- **[Runtime Assets](../../../Run/CLAUDE.md)** - Asset files
- **[Development Plan](../../../.claude/plan/development.md)** - Assignment 4 guide

## Changelog

- **2025-11-09**: Critical bug fixes in World class (Phase 5B progress)
  - ✅ **World.cpp - Fixed RegenerateAllChunks crashes** (lines 398-472)
    - Fixed dangling reference bug in `DeactivateChunk()` (line 299)
      - Problem: `chunkCoords` reference to map key invalidated after `map.erase()`
      - Solution: Copy `chunkCoords` to local variable `localChunkCoords` before erasing
    - Fixed double-delete bug with completed jobs (lines 405-472)
      - Problem: Completed jobs deleted twice (from `completedJobs` AND tracking lists)
      - Solution: Retrieve jobs from JobSystem, remove from tracking lists WITHOUT delete, then delete separately
    - Fixed memory leak with 3 DirectX 11 buffers after RegenerateAllChunks
      - Problem: Completed jobs still in JobSystem queue when chunks deleted
      - Solution: Call `g_jobSystem->RetrieveAllCompletedJobs()` before chunk deletion
  - ✅ **World.cpp - Enhanced job management** (lines 403-472)
    - Properly retrieve all completed jobs from JobSystem before cleanup
    - Remove completed jobs from tracking lists to prevent double-delete
    - Delete remaining tracked jobs (not yet completed) separately
    - Finally delete completed jobs after all processing
- **2025-11-08**: Assignment 4 Phase 5A (Carvers) completed in Framework module
  - World class continues to manage chunk activation and job processing for all world generation phases
  - Ravine and river carvers now part of terrain generation pipeline
- **2025-11-01**: Completed Phase 0 prerequisites (Tasks 0.1-0.7) for Assignment 4: World Generation
  - ✅ World class chunk management optimizations complete
  - ✅ Thread-safe job processing and chunk state transitions
  - ✅ Smart directional preloading and mesh rebuilding optimization
  - Ready to begin Phase 1: Foundation (Biome System Implementation)
- **2025-10-26**: Updated documentation for Assignment 4: World Generation
  - Added Assignment 4 context section explaining World class role in chunk management
  - Added navigation breadcrumbs and quick navigation
  - Enhanced inline cross-references to Framework and Definition modules
  - Documented World class responsibilities for job management and chunk activation
  - Linked to development planning resources
- **2025-09-13**: Initial Gameplay module documentation created
- **Recent**: World class now properly manages active chunks for dynamic loading
- **Recent**: Player system integration with input handling and camera control
- **Recent**: Entity system foundation established for future gameplay expansion