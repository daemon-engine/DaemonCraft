//----------------------------------------------------------------------------------------------------
// ChunkMeshJob.cpp - Implementation of asynchronous chunk mesh generation
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/ChunkMeshJob.hpp"
#include "Game/Framework/Chunk.hpp"
#include "Game/Framework/BlockIterator.hpp"
#include "Game/Framework/GameCommon.hpp"
#include "Game/Definition/BlockDefinition.hpp"
#include "Game/Gameplay/World.hpp"  // Assignment 5 Phase 0: For cross-chunk neighbor access
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/VertexUtils.hpp"

//----------------------------------------------------------------------------------------------------
ChunkMeshJob::ChunkMeshJob(Chunk* chunk, World* world)
    : m_chunk(chunk), m_world(world)
{
    GUARANTEE_OR_DIE(m_chunk != nullptr, "ChunkMeshJob created with null chunk pointer");
    GUARANTEE_OR_DIE(m_world != nullptr, "ChunkMeshJob created with null world pointer");

    // Verify chunk is in the correct state for mesh generation
    ChunkState currentState = m_chunk->GetState();
    GUARANTEE_OR_DIE(currentState == ChunkState::COMPLETE,
                     "ChunkMeshJob created for chunk not in COMPLETE state");
}

//----------------------------------------------------------------------------------------------------
void ChunkMeshJob::Execute()
{
    // Validate chunk state before starting work
    ValidateChunkState();

    try
    {
        // Perform the actual CPU mesh building
        GenerateMeshData();

        // Mark as successful
        m_wasSuccessful = true;

        // Note: Chunk state remains COMPLETE
        // The mesh data will be applied to the chunk on the main thread
        // when ApplyMeshDataToChunk() is called
    }
    catch (...)
    {
        // Mark as failed
        m_wasSuccessful = false;

        // Clear any partial mesh data
        m_vertices.clear();
        m_indices.clear();
        m_debugVertices.clear();
        m_debugIndices.clear();
    }
}

//----------------------------------------------------------------------------------------------------
IntVec2 ChunkMeshJob::GetChunkCoords() const
{
    if (m_chunk)
    {
        return m_chunk->GetChunkCoords();
    }
    return IntVec2::ZERO;
}

//----------------------------------------------------------------------------------------------------
void ChunkMeshJob::GenerateMeshData()
{
    if (!m_chunk)
    {
        return;
    }

    // Clear any existing mesh data
    m_vertices.clear();
    m_indices.clear();
    m_debugVertices.clear();
    m_debugIndices.clear();

    // Get chunk coordinates for world position calculations
    IntVec2 chunkCoords = m_chunk->GetChunkCoords();
    Vec3 chunkWorldOffset((float)(chunkCoords.x * CHUNK_SIZE_X),
                         (float)(chunkCoords.y * CHUNK_SIZE_Y),
                         0.0f);

    // Cache-coherent iteration: iterate blocks in memory order for optimal cache performance
    // Memory layout is: index = x + (y << CHUNK_BITS_X) + (z << (CHUNK_BITS_X + CHUNK_BITS_Y))
    for (int blockIndex = 0; blockIndex < BLOCKS_PER_CHUNK; blockIndex++)
    {
        // Access block data via coordinate conversion (thread-safe since chunk is in COMPLETE state
        // and worker threads only read block data during mesh generation)
        IntVec3 localCoords = Chunk::IndexToLocalCoords(blockIndex);
        Block*  block       = m_chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
        if (!block) continue;

        sBlockDefinition* def = sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex);

        // Skip invisible blocks (air, transparent blocks)
        if (!def || !def->IsVisible()) continue;

        // Calculate block center in world coordinates
        Vec3 blockCenter = Vec3((float)localCoords.x + 0.5f,
                               (float)localCoords.y + 0.5f,
                               (float)localCoords.z + 0.5f) + chunkWorldOffset;

        // Assignment 5 Phase 0 FIX: Create block iterator with World pointer for cross-chunk navigation
        // This enables GetNeighbor() to access blocks in adjacent chunks for proper hidden surface removal
        BlockIterator blockIter(m_chunk, blockIndex, m_world);

        // Check each face direction and only add visible faces
        // Face directions as offset vectors
        IntVec3 faceDirections[6] = {
            IntVec3(0, 0, 1),   // Top face (+Z)
            IntVec3(0, 0, -1),  // Bottom face (-Z)
            IntVec3(1, 0, 0),   // East face (+X)
            IntVec3(-1, 0, 0),  // West face (-X)
            IntVec3(0, 1, 0),   // North face (+Y)
            IntVec3(0, -1, 0)   // South face (-Y)
        };

        Vec3 faceNormals[6] = {
            Vec3::Z_BASIS,      // Top
            -Vec3::Z_BASIS,     // Bottom
            Vec3::X_BASIS,      // East
            -Vec3::X_BASIS,     // West
            Vec3::Y_BASIS,      // North
            -Vec3::Y_BASIS      // South
        };

        // Assignment 5 Phase 7: Directional shading values for b channel
        // Top = 1.0 (255), Sides = 0.8 (204), Bottom = 0.6 (153)
        float directionalShading[6] = {
            1.0f,    // Top
            0.6f,    // Bottom
            0.8f,    // East
            0.8f,    // West
            0.8f,    // North
            0.8f     // South
        };

        // Check each of the 6 faces
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            // Check if face is visible (not hidden by adjacent opaque block)
            if (!IsFaceVisibleForJob(blockIter, faceDirections[faceIndex]))
            {
                continue; // Skip hidden face
            }

            // Get UV coordinates based on which face this is
            Vec2 uvs;
            if (faceIndex == 0)      uvs = def->GetTopUVs();    // Top
            else if (faceIndex == 1) uvs = def->GetBottomUVs(); // Bottom
            else                     uvs = def->GetSideUVs();    // Sides (East, West, North, South)

            // Assignment 5 Phase 7 FIX: Get lighting from NEIGHBOR block in face direction
            // This matches Chunk.cpp behavior - each face uses the light from the neighbor
            // For example, the top face of grass uses the light from the air block above
            BlockIterator neighborIter = blockIter.GetNeighbor(faceDirections[faceIndex]);
            Block* neighborBlock = neighborIter.IsValid() ? neighborIter.GetBlock() : nullptr;

            // Assignment 5: Read lighting from neighbor block
            // FIX: If neighbor is unavailable (unloaded chunk), use default skylight value
            // This prevents completely black faces on chunk edges where neighbors aren't loaded yet
            uint8_t outdoorLight = neighborBlock ? neighborBlock->GetOutdoorLight() : 15;  // Default to full skylight
            uint8_t indoorLight = neighborBlock ? neighborBlock->GetIndoorLight() : 0;

            // FIX: Apply minimum ambient light as INDOOR light (not outdoor) to prevent black faces
            // Indoor light is NOT modulated by day/night cycle, providing constant ambient illumination
            // This matches Minecraft's behavior where shadowed areas remain visible even at night
            constexpr uint8_t MIN_AMBIENT_LIGHT = 4;  // Minimum light level (4/15 = ~27% brightness)
            if (outdoorLight < MIN_AMBIENT_LIGHT && indoorLight == 0)
            {
                indoorLight = MIN_AMBIENT_LIGHT;  // Use indoor channel to avoid day/night modulation
            }

            // Normalize lighting to 0.0-1.0 range
            float outdoorNormalized = (float)outdoorLight / 15.0f;
            float indoorNormalized = (float)indoorLight / 15.0f;

            // Assignment 5 Phase 7: Encode lighting in vertex colors
            // r = outdoor light (0-255), g = indoor light (0-255), b = directional shading (0-255)
            uint8_t r = (uint8_t)(outdoorNormalized * 255.0f);
            uint8_t g = (uint8_t)(indoorNormalized * 255.0f);
            uint8_t b = (uint8_t)(directionalShading[faceIndex] * 255.0f);
            Rgba8 vertexColor = Rgba8(r, g, b, 255);

            // Add the visible face to our local vertex/index buffers with lighting
            AddBlockFaceToJob(blockCenter, faceNormals[faceIndex], uvs, vertexColor);
        }
    }

    // Add debug wireframe for chunk bounds
    AABB3 worldBounds = m_chunk->GetWorldBounds();
    AddVertsForWireframeAABB3D(m_debugVertices, worldBounds, 0.1f);
}

//----------------------------------------------------------------------------------------------------
void ChunkMeshJob::ValidateChunkState()
{
    if (!m_chunk)
    {
        ERROR_AND_DIE("ChunkMeshJob: Null chunk pointer during execution");
    }

    ChunkState currentState = m_chunk->GetState();
    if (currentState != ChunkState::COMPLETE)
    {
        ERROR_AND_DIE("ChunkMeshJob: Chunk not in COMPLETE state during execution");
    }
}

//----------------------------------------------------------------------------------------------------
void ChunkMeshJob::ApplyMeshDataToChunk()
{
    if (!m_wasSuccessful || !m_chunk)
    {
        return;
    }

    // Verify chunk is still in COMPLETE state
    ChunkState currentState = m_chunk->GetState();
    if (currentState != ChunkState::COMPLETE)
    {
        ERROR_AND_DIE("ChunkMeshJob: Chunk state changed before mesh data could be applied");
    }

    // Apply the generated mesh data to the chunk
    // This replaces the CPU-intensive part of RebuildMesh()
    m_chunk->SetMeshData(m_vertices, m_indices, m_debugVertices, m_debugIndices);

    // The chunk will handle DirectX buffer updates in its own UpdateVertexBuffer() method
    // which must be called on the main thread
}

//----------------------------------------------------------------------------------------------------
bool ChunkMeshJob::IsFaceVisibleForJob(BlockIterator const& blockIter, IntVec3 const& faceDirection) const
{
    // Assignment 5 Phase 0 FIX: Use GetNeighbor() instead of Move methods for cross-chunk support
    // MoveEast()/MoveWest()/etc. only work within the same chunk and return false at chunk boundaries
    // GetNeighbor() properly navigates across chunk boundaries via World pointer
    BlockIterator neighborIter = blockIter.GetNeighbor(faceDirection);

    // CRITICAL: If neighbor is invalid, assume it's OPAQUE (hidden face)
    // This prevents rendering chunk boundary faces when neighbor chunks aren't fully loaded
    // Only render faces at true world boundaries (z < 0 or z >= CHUNK_SIZE_Z)
    if (!neighborIter.IsValid())
    {
        // Check if we're at vertical world boundaries (top/bottom of world)
        IntVec3 currentCoords = blockIter.GetLocalCoords();
        IntVec3 neighborCoords = currentCoords + faceDirection;

        // Convert to world Z coordinate
        int worldZ = neighborCoords.z;

        // Only render face if at vertical world boundaries
        if (worldZ < 0 || worldZ >= CHUNK_SIZE_Z)
        {
            return true;  // At vertical world boundaries, render face
        }

        // Horizontal chunk boundary with unloaded neighbor - assume opaque (hide face)
        return false;
    }

    // Get the neighbor block
    Block* neighborBlock = neighborIter.GetBlock();
    if (!neighborBlock)
    {
        // No block data means assume opaque (safer for hidden surface removal)
        return false;
    }

    // Get neighbor block definition
    sBlockDefinition* neighborDef = sBlockDefinition::GetDefinitionByIndex(neighborBlock->m_typeIndex);
    if (!neighborDef)
    {
        // Invalid definition - assume opaque
        return false;
    }

    // Face is VISIBLE if neighbor is NOT opaque
    // Face is HIDDEN if neighbor is opaque
    return !neighborDef->IsOpaque();
}

//----------------------------------------------------------------------------------------------------
void ChunkMeshJob::AddBlockFaceToJob(Vec3 const& blockCenter, Vec3 const& faceNormal, Vec2 const& uvs, Rgba8 const& tint)
{
    // This is identical to Chunk::AddBlockFace but writes to the job's local vectors
    // instead of the chunk's member vectors

    // Use Engine's GetOrthonormalBasis for correct right/up vectors
    Vec3 right, up;
    faceNormal.GetOrthonormalBasis(faceNormal, &right, &up);

    // Calculate face center by moving from block center along normal
    Vec3 faceCenter = blockCenter + faceNormal * 0.5f;

    // Convert sprite coordinates to UV coordinates
    // Assuming 8x8 sprite atlas (64 total sprites in an 8x8 grid)
    constexpr float ATLAS_SIZE  = 8.f;  // 8x8 grid of sprites
    constexpr float SPRITE_SIZE = 1.f / ATLAS_SIZE;  // Each sprite is 1/8 of the texture
    Vec2 const uvMins = uvs;
    Vec2 const uvMaxs = uvs + Vec2::ONE;

    // Real UV coordinates (flipped Y for DirectX)
    Vec2 const realMins = Vec2(uvMins.x, uvMaxs.y) * SPRITE_SIZE;
    Vec2 const realMaxs = Vec2(uvMaxs.x, uvMins.y) * SPRITE_SIZE;

    // Additional Y-flip for sprite UVs (DirectX coordinate system)
    AABB2 const spriteUVs = AABB2(Vec2(realMins.x, 1.f - realMins.y),
                                  Vec2(realMaxs.x, 1.f - realMaxs.y));

    // Add vertices using the corrected UV coordinates
    // CRITICAL: Vertex order must be (BL, BR, TL, TR) for correct winding
    AddVertsForQuad3D(m_vertices, m_indices,
                     faceCenter - right * 0.5f - up * 0.5f,    // Bottom Left
                     faceCenter + right * 0.5f - up * 0.5f,    // Bottom Right
                     faceCenter - right * 0.5f + up * 0.5f,    // Top Left
                     faceCenter + right * 0.5f + up * 0.5f,    // Top Right
                     tint, spriteUVs);
}