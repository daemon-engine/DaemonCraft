# Fix Lighting Propagation for Correct Initial Chunk Lighting

## Objective

Ensure all generated chunks have correct initial lighting values that respond to the day/night cycle from the moment they become visible.

## Problem Statement

Currently, chunks may appear with incorrect lighting because:
1. `InitializeLighting()` sets sky-visible blocks to outdoor=15
2. `OnActivate()` queues surface blocks to dirty light queue
3. Mesh building can occur BEFORE lighting propagation completes
4. Result: Mesh has stale outdoor=0 values instead of propagated outdoor=15

## Root Cause Investigation Areas

### Area 1: Timing Between InitializeLighting and Mesh Building
- InitializeLighting() is called in ChunkGenerateJob (worker thread)
- Mesh is rebuilt when chunk transitions to COMPLETE state
- Dirty light queue is processed in World::Update() on main thread
- **Gap**: Mesh may build before dirty queue is fully processed

### Area 2: OnActivate() Propagation Scope
- Currently only queues surface blocks (air directly above terrain)
- But InitializeLighting() sets ALL air blocks above surface to outdoor=15
- The propagation needs to spread from surface down into terrain
- **Question**: Is the propagation reaching all blocks that need it?

### Area 3: Mesh Rebuild Triggering
- Chunks are marked dirty when blocks change
- But after InitializeLighting(), the lighting data IS correct
- The mesh just needs to be rebuilt AFTER lighting propagates

## Implementation Options

### Option A: Force Complete Lighting Propagation Before Mesh Build

**Approach**: Process all dirty blocks for a chunk before building its mesh

**Implementation**:
1. In `World::ActivateChunk()` or when processing COMPLETE chunks
2. Before calling `RebuildMesh()`, fully process dirty lighting for that chunk
3. This ensures mesh has correct lighting data

**Pros**:
- Guarantees correct initial lighting
- Simple conceptually

**Cons**:
- May cause frame hitches if many dirty blocks
- Needs careful implementation to only process relevant blocks

### Option B: Defer Mesh Building Until Lighting Stable

**Approach**: Add a new chunk state for "lighting stabilizing"

**Implementation**:
1. After COMPLETE state, add LIGHTING_STABILIZING state
2. Don't build mesh until chunk has no dirty blocks for N frames
3. Then build mesh and transition to final state

**Pros**:
- Clean separation of concerns
- Natural integration with state machine

**Cons**:
- Adds complexity to state machine
- Chunks may be invisible longer

### Option C: Immediate Sync Propagation in InitializeLighting

**Approach**: Propagate lighting synchronously during initialization

**Implementation**:
1. In InitializeLighting(), after setting sky blocks to outdoor=15
2. Immediately propagate downward to adjacent opaque blocks
3. Don't use dirty queue, do direct propagation

**Pros**:
- Lighting is complete when generation finishes
- No timing issues with mesh building

**Cons**:
- Runs on worker thread (thread safety concerns)
- May slow down chunk generation

### Option D: Mark Mesh Dirty After Lighting Propagation (RECOMMENDED)

**Approach**: Ensure mesh rebuilds AFTER lighting propagates, not before

**Implementation**:
1. Track when chunks have pending dirty blocks
2. After ProcessDirtyLighting() processes blocks from a chunk
3. Mark that chunk's mesh as dirty again
4. This triggers a second mesh rebuild with correct lighting

**Pros**:
- Minimal code changes
- Uses existing systems
- Doesn't block chunk activation

**Cons**:
- Chunks may briefly show incorrect lighting
- Extra mesh rebuilds

## Recommended Approach: Option D with Enhancement

### Phase 1: Verify Current Lighting Flow (Investigation)

**Task 1.1**: Add logging to trace lighting propagation timing
- Log when InitializeLighting() completes
- Log when ProcessDirtyLighting() processes blocks
- Log when RebuildMesh() occurs
- Verify the order of operations

**Task 1.2**: Identify the specific timing gap
- Is mesh building happening before dirty queue processing?
- Is OnActivate() queuing the right blocks?
- Is propagation reaching all necessary blocks?

### Phase 2: Fix the Timing Issue

**Task 2.1**: Ensure mesh rebuilds after lighting propagates
- In `ProcessDirtyLighting()`, when a block's lighting changes
- Mark that block's chunk as needing mesh rebuild
- This ensures any lighting changes trigger mesh update

**Task 2.2**: Verify cross-chunk propagation
- When light propagates across chunk boundaries
- Both chunks need mesh rebuilds
- Check that neighbor chunks are properly marked dirty

### Phase 3: Optimize Initial Lighting

**Task 3.1**: Consider sync propagation for new chunks
- For newly generated chunks only
- Process immediate neighbors synchronously
- Reduces "flicker" when chunks appear

**Task 3.2**: Test and validate
- Generate new world
- Verify all chunks have correct initial lighting
- Verify day/night cycle affects all chunks
- Check for any remaining dark/bright anomalies

## Acceptance Criteria

1. ✅ All newly generated chunks appear with correct lighting matching current time of day
2. ✅ All chunks respond to day/night cycle changes in real-time
3. ✅ No "frozen" chunks that don't update with lighting changes
4. ✅ No significant performance regression (frame rate stays stable)
5. ✅ Cross-chunk lighting propagation works correctly at boundaries

## Files to Modify

- `Code/Game/Gameplay/World.cpp` - ProcessDirtyLighting(), chunk mesh dirty tracking
- `Code/Game/Framework/Chunk.cpp` - InitializeLighting(), OnActivate()
- `Code/Game/Framework/Chunk.hpp` - Possibly add lighting state tracking

## Risk Assessment

- **Low Risk**: Logging and investigation
- **Medium Risk**: Mesh dirty tracking changes
- **High Risk**: Sync propagation changes (thread safety)

## Timeline Estimate

- Phase 1 (Investigation): 30 minutes
- Phase 2 (Fix): 1-2 hours
- Phase 3 (Optimization): 30 minutes - 1 hour

Total: 2-4 hours depending on complexity found during investigation
