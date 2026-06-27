//----------------------------------------------------------------------------------------------------
// World.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/World.hpp"

#include <algorithm>
#include <chrono>      // For std::chrono::milliseconds
#include <cmath>       // For cosf, fmod in day/night cycle (Assignment 5 Phase 9)
#include <filesystem>
#include <thread>      // For std::this_thread::sleep_for

#include "Engine/Core/Clock.hpp"  // Assignment 5 Phase 4: For GetTotalSeconds()
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/Renderer.hpp"  // Assignment 5 Phase 8: For shader and constant buffer
#include "Engine/Renderer/Shader.hpp"    // Assignment 5 Phase 8: For Shader class
#include "Engine/Renderer/ConstantBuffer.hpp"  // Assignment 5 Phase 8: For ConstantBuffer class
#include "Engine/Renderer/VertexBuffer.hpp"  // For leak tracking
#include "Engine/Renderer/IndexBuffer.hpp"   // For leak tracking
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Game/Framework/App.hpp"
#include "Game/Framework/Block.hpp"  // Assignment 5 Phase 4: For BlockIterator
#include "Game/Framework/BlockIterator.hpp"  // Assignment 5 Phase 4: For dirty light queue
#include "Game/Framework/Chunk.hpp"
#include "Game/Definition/BlockDefinition.hpp"  // Assignment 5 Phase 5: For IsOpaque(), IsEmissive()
#include "Game/Framework/ChunkGenerateJob.hpp"
#include "Game/Framework/ChunkLoadJob.hpp"
#include "Game/Framework/ChunkMeshJob.hpp"
#include "Game/Framework/ChunkSaveJob.hpp"
#include "Game/Framework/GameCommon.hpp"
#include "Game/Gameplay/Entity.hpp"  // Assignment 6: For GetWorldAABB() in IsEntityOnGround()
#include "Game/Gameplay/Game.hpp"
#include "Game/Gameplay/ItemEntity.hpp"  // Assignment 7: For SpawnItemEntity()
#include "Game/Gameplay/ItemStack.hpp"   // Assignment 7: For ItemStack in SpawnItemEntity()
#include "Game/Gameplay/Agent.hpp"       // Assignment 7-AI: For Agent management
#include "ThirdParty/Noise/SmoothNoise.hpp"

//----------------------------------------------------------------------------------------------------
// Hash function implementation for IntVec2
//----------------------------------------------------------------------------------------------------
size_t std::hash<IntVec2>::operator()(IntVec2 const& vec) const noexcept
{
    return std::hash<int>()(vec.x) ^ (std::hash<int>()(vec.y) << 1);
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 8: World shader constant buffer structure
// Must match layout in World.hlsl cbuffer WorldConstants : register(b8)
// 16-byte aligned structure
// Assignment 5: Updated to match A5 specification requirements
//----------------------------------------------------------------------------------------------------
struct WorldConstants
{
    float cameraPosition[4];    // Camera world position (for fog distance calculation)
    float indoorLightColor[4];  // Indoor light color (default: 255, 230, 204)
    float outdoorLightColor[4]; // Outdoor light color (varies with day/night)
    float skyColor[4];          // Sky/fog color (for distance fog)
    float fogNearDistance;      // Fog near distance
    float fogFarDistance;       // Fog far distance
    float gameTime;             // Current game time in seconds
    float padding;              // 16-byte alignment
};

//----------------------------------------------------------------------------------------------------
World::World()
{
    // Assignment 5 Phase 8: Load World shader and create constant buffer
    m_worldShader = g_resourceSubsystem->CreateOrGetShaderFromFile("Data/Shaders/World");
    m_worldConstantBuffer = g_renderer->CreateConstantBuffer(sizeof(WorldConstants));
}

//----------------------------------------------------------------------------------------------------
World::~World()
{
    // DEBUG: Count active chunks before cleanup
    int activeChunkCount = 0;
    int nonActiveChunkCount = 0;
    int meshRebuildCount = 0;

    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        activeChunkCount = (int)m_activeChunks.size();
    }

    {
        std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
        nonActiveChunkCount = (int)m_nonActiveChunks.size();
    }

    {
        std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
        meshRebuildCount = (int)m_chunksNeedingMeshRebuild.size();
    }

    // DebuggerPrintf("[WORLD DESTRUCTOR] Starting cleanup:\n");
    // DebuggerPrintf("[WORLD DESTRUCTOR]   Active chunks: %d\n", activeChunkCount);
    // DebuggerPrintf("[WORLD DESTRUCTOR]   Non-active chunks: %d\n", nonActiveChunkCount);
    // DebuggerPrintf("[WORLD DESTRUCTOR]   Chunks needing mesh rebuild: %d\n", meshRebuildCount);

    // CRITICAL FIX: Wait for all executing jobs to finish BEFORE retrieving completed jobs
    // This prevents DirectX buffer leaks from ChunkMeshJobs that created buffers but didn't
    // finish applying them to chunks before shutdown.
    //
    // Problem: Previously, we retrieved completed jobs immediately (line 104), which returned 0 jobs
    // because the 16 pending ChunkMeshJobs were still executing on worker threads.
    // When chunks were deleted, their buffers (created by incomplete jobs) leaked.
    //
    // Solution: Wait for worker threads to finish executing ALL jobs, then retrieve completed jobs.
    // This ensures ApplyMeshDataToChunk() and UpdateVertexBuffer() are called for all mesh jobs.
    int executingJobCount = g_jobSystem->GetExecutingJobCount();
    if (executingJobCount > 0)
    {
        // DebuggerPrintf("[WORLD DESTRUCTOR] Waiting for %d executing jobs to complete...\n", executingJobCount);
        
        int waitIterations = 0;
        while (g_jobSystem->GetExecutingJobCount() > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            waitIterations++;
            
            // Safety timeout: prevent infinite loop if jobs are stuck
            if (waitIterations > 500) // 5 seconds timeout (500 * 10ms)
            {
                // DebuggerPrintf("[WORLD DESTRUCTOR] WARNING: Timeout waiting for jobs (still %d executing)\n",
                //               g_jobSystem->GetExecutingJobCount());
                break;
            }
        }
        
        executingJobCount = g_jobSystem->GetExecutingJobCount();
        if (executingJobCount == 0)
        {
            // DebuggerPrintf("[WORLD DESTRUCTOR] All jobs completed successfully (waited %dms)\n", waitIterations * 10);
        }
    }

    // NOW retrieve all completed jobs from JobSystem BEFORE deleting chunks
    // This should now include all the ChunkMeshJobs that just finished executing
    std::vector<Job*> completedJobs = g_jobSystem->RetrieveAllCompletedJobs();
    // DebuggerPrintf("[WORLD DESTRUCTOR] Retrieved %d completed jobs from JobSystem\n", (int)completedJobs.size());

    // CRITICAL: Process completed ChunkMeshJobs
    // If we don't call UpdateVertexBuffer() for completed mesh jobs, the DirectX buffers leak!
    //
    // NOTE: After the wait loop above, all jobs SHOULD be completed and in the completedJobs list.
    // The "late retrieval" at line ~235 is now only a safety fallback for timeout edge cases.
    int meshJobsProcessed = 0;
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        // DebuggerPrintf("[WORLD DESTRUCTOR] Processing %d ChunkMeshJobs (%d completed)...\n",
        //               (int)m_chunkMeshJobs.size(), (int)completedJobs.size());

        for (ChunkMeshJob* meshJob : m_chunkMeshJobs)
        {
            if (meshJob != nullptr)
            {
                // Check if this job is in the completed list
                bool isCompleted = std::find(completedJobs.begin(), completedJobs.end(), meshJob) != completedJobs.end();

                if (isCompleted)
                {
                    Chunk* chunk = meshJob->GetChunk();
                    if (chunk && meshJob->WasSuccessful())
                    {
                        // DebuggerPrintf("[WORLD DESTRUCTOR]   Processing completed ChunkMeshJob for Chunk(%d,%d)\n",
                        //               chunk->GetChunkCoords().x, chunk->GetChunkCoords().y);
                        meshJob->ApplyMeshDataToChunk();
                        chunk->UpdateVertexBuffer();
                        chunk->SetMeshClean();
                        meshJobsProcessed++;
                    }
                    // NOTE: Don't delete here - will be deleted with completedJobs list below
                }
                // If job is NOT completed, it's still pending or running
                // We cannot safely delete it here - leave it for late retrieval
            }
        }

        // DebuggerPrintf("[WORLD DESTRUCTOR] Processed %d ChunkMeshJobs successfully\n", meshJobsProcessed);

        // Now clear job tracking lists (but don't delete the jobs yet)
        m_chunkMeshJobs.clear();
        m_chunkGenerationJobs.clear();
        m_chunkLoadJobs.clear();
        m_chunkSaveJobs.clear();
    }

    // Delete all completed jobs
    for (Job* job : completedJobs)
    {
        delete job;
    }
    // DebuggerPrintf("[WORLD DESTRUCTOR] Deleted %d completed jobs\n", (int)completedJobs.size());

    // Clean up chunks in m_nonActiveChunks (being processed by workers)
    {
        std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
        // DebuggerPrintf("[WORLD DESTRUCTOR] Deleting %d non-active chunks...\n", (int)m_nonActiveChunks.size());
        for (Chunk* chunk : m_nonActiveChunks)
        {
            if (chunk != nullptr)
            {
                if (chunk->GetNeedsSaving())
                {
                    chunk->SaveToDisk();
                }
                delete chunk;
            }
        }
        m_nonActiveChunks.clear();
    }

    // Clear mesh rebuild tracking set
    {
        std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
        // DebuggerPrintf("[WORLD DESTRUCTOR] Clearing %d chunks from mesh rebuild set\n", (int)m_chunksNeedingMeshRebuild.size());
        m_chunksNeedingMeshRebuild.clear();
    }

    // CRITICAL: Retrieve any jobs that completed AFTER the first retrieval
    // This catches pending jobs that finished while we were processing the first batch
    // MUST retrieve jobs BEFORE deleting active chunks (jobs hold chunk pointers!)
    std::vector<Job*> lateCompletedJobs = g_jobSystem->RetrieveAllCompletedJobs();
    // DebuggerPrintf("[WORLD DESTRUCTOR] Retrieved %d late completed jobs\n", (int)lateCompletedJobs.size());

    // Process late ChunkMeshJobs to prevent DirectX buffer leaks
    int lateProcessed = 0;
    for (Job* job : lateCompletedJobs)
    {
        ChunkMeshJob* meshJob = dynamic_cast<ChunkMeshJob*>(job);
        if (meshJob != nullptr)
        {
            Chunk* chunk = meshJob->GetChunk();
            if (chunk && meshJob->WasSuccessful())
            {
                // DebuggerPrintf("[WORLD DESTRUCTOR]   Processing LATE ChunkMeshJob for Chunk(%d,%d)\n",
                //               chunk->GetChunkCoords().x, chunk->GetChunkCoords().y);
                meshJob->ApplyMeshDataToChunk();
                chunk->UpdateVertexBuffer();
                chunk->SetMeshClean();
                lateProcessed++;
            }
        }
        delete job;
    }

    if (lateProcessed > 0)
    {
        // DebuggerPrintf("[WORLD DESTRUCTOR] Processed %d late ChunkMeshJobs to prevent DirectX leaks\n", lateProcessed);
    }

    // NOW safe to delete active chunks (all jobs that reference them have been processed)
    // DebuggerPrintf("[WORLD DESTRUCTOR] Deactivating %d active chunks...\n", activeChunkCount);
    DeactivateAllChunks(true);
    // DebuggerPrintf("[WORLD DESTRUCTOR] Deactivation complete\n");

    // Print buffer leak reports
    DebuggerPrintf("\n");
    VertexBuffer::PrintLeakReport();
    IndexBuffer::PrintLeakReport();
    DebuggerPrintf("\n");

    // Assignment 7: Clean up all ItemEntities
    for (ItemEntity* itemEntity : m_itemEntities)
    {
        if (itemEntity != nullptr)
        {
            delete itemEntity;
        }
    }
    m_itemEntities.clear();

    // Assignment 7-AI: Clean up all agents
    for (auto const& pair : m_agents)
    {
        if (pair.second != nullptr)
        {
            delete pair.second;
        }
    }
    m_agents.clear();

    // Assignment 5 Phase 8: Clean up shader resources
    // Note: Shader is owned by Renderer (CreateOrGetShaderFromFile uses a cache), don't delete it
    // Only delete the constant buffer we own
    if (m_worldConstantBuffer != nullptr)
    {
        delete m_worldConstantBuffer;
        m_worldConstantBuffer = nullptr;
    }
}

//----------------------------------------------------------------------------------------------------
void World::Update(float const deltaSeconds)
{
    if (g_game == nullptr) return;

    // Assignment 5 Phase 9: Update day/night cycle
    // KEYCODE_Y accelerates time by 60x for testing day/night transitions
    float timeMultiplier = 1.0f;
    if (g_input && g_input->IsKeyDown(KEYCODE_Y))
    {
        timeMultiplier = 60.0f;  // Hold Y to fast-forward time (60x speed = 4 seconds per full cycle)
    }
    m_gameTime += deltaSeconds * timeMultiplier;

    constexpr float DAY_NIGHT_CYCLE_DURATION = 240.0f;  // 4 minutes per full day/night cycle
    constexpr float LOCAL_PI = 3.14159265359f;
    float cyclePosition = fmod(m_gameTime, DAY_NIGHT_CYCLE_DURATION) / DAY_NIGHT_CYCLE_DURATION;

    // Outdoor brightness: 1.0 at noon (cyclePosition=0.5), 0.2 at midnight (cyclePosition=0.0 or 1.0)
    // Use cosine wave: cos(0) = 1 (midnight), cos(PI) = -1 (noon)
    float cosValue = cosf(cyclePosition * 2.0f * LOCAL_PI);  // Range [-1, 1], peaks at midnight
    m_outdoorBrightness = RangeMap(cosValue, 1.0f, -1.0f, 0.2f, 1.0f);  // Map to [0.2, 1.0]

    // Assignment 5 Stage 8: Day/night sky and outdoor light color modulation
    // m_outdoorBrightness already computed above (lines 174): ranges from 0.2 (midnight) to 1.0 (noon)

    // Sky/fog color: dark blue at night, light blue at day
    Rgba8 nightSkyColor(20, 20, 40, 255);      // Dark blue - midnight sky
    Rgba8 daySkyColor(200, 230, 255, 255);     // Light blue - noon sky
    Rgba8 baseSkyColor = Interpolate(nightSkyColor, daySkyColor, m_outdoorBrightness);

    // Outdoor light color: dark blue-grey at night, white at day
    Rgba8 nightOutdoorColor(50, 50, 80, 255);  // Dark blue-grey - dim moonlight
    Rgba8 dayOutdoorColor(255, 255, 255, 255); // Pure white - bright sunlight
    Rgba8 baseOutdoorColor = Interpolate(nightOutdoorColor, dayOutdoorColor, m_outdoorBrightness);

    // Assignment 5 Stage 8: Lightning strikes (1D Perlin, 9 octaves, scale 50 - FASTER)
    // Spec: Use 1D Perlin noise based on world time, range-map [0.6, 0.9] to [0, 1]
    // Effect: Lerp sky and outdoor colors toward white for dramatic flashes
    // Modified: Reduced scale from 200→50 for 4× faster lightning frequency (~5-15s intervals)
    unsigned int lightningSeed = GAME_SEED + 1000;  // Unique seed for lightning (avoid collision with terrain)
    float lightningPerlin = Compute1dPerlinNoise(m_gameTime, 5.0f, 9, 0.5f, 2.0f, true, lightningSeed);

    // Range-map [0.6, 0.9] to lightningStrength [0, 1]
    // Values below 0.6 clamp to 0 (no lightning), above 0.9 clamp to 1 (full white)
    float lightningStrength = RangeMapClamped(lightningPerlin, 0.6f, 0.9f, 0.0f, 1.0f);

    // Lerp sky and outdoor colors toward pure white based on lightning strength
    Rgba8 lightningWhite(255, 255, 255, 255);
    m_skyColor = Interpolate(baseSkyColor, lightningWhite, lightningStrength);
    m_finalOutdoorColor = Interpolate(baseOutdoorColor, lightningWhite, lightningStrength);

    // Assignment 5 Stage 8: Glowstone flicker (1D Perlin, 9 octaves, scale 100 - FASTER)
    // Spec: Use 1D Perlin noise based on world time, range-map [-1, 1] to [0.8, 1.0]
    // Effect: Multiply base indoor light color by glow strength for subtle pulsing
    // Modified: Reduced scale from 500→100 for 5× faster pulse frequency (~2-3min cycles)
    unsigned int glowstoneSeed = GAME_SEED + 2000;  // Unique seed for glowstone (different from lightning)
    float glowPerlin = Compute1dPerlinNoise(m_gameTime, 1.0f, 9, 0.5f, 2.0f, true, glowstoneSeed);

    // Range-map [-1, 1] to glowStrength [0.8, 1.0]
    // Full Perlin range used: -1 maps to 0.8 (80% brightness), +1 maps to 1.0 (100% brightness)
    float glowStrength = RangeMapClamped(glowPerlin, -1.0f, 1.0f, 0.8f, 1.0f);

    // Base indoor light color from spec (warm orange-ish light)
    Rgba8 baseIndoorColor(255, 230, 204, 255);

    // Multiply color by glow strength (component-wise scalar multiplication)
    m_finalIndoorColor = Rgba8(
        (uint8_t)(baseIndoorColor.r * glowStrength),
        (uint8_t)(baseIndoorColor.g * glowStrength),
        (uint8_t)(baseIndoorColor.b * glowStrength),
        255  // Alpha always 255 (fully opaque)
    );

    // Update all active chunks first
    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        for (auto const& chunkPair : m_activeChunks)
        {
            if (chunkPair.second != nullptr)
            {
                chunkPair.second->Update(deltaSeconds);
            }
        }
    }

    // Assignment 7: Update all ItemEntities
    for (size_t i = 0; i < m_itemEntities.size(); )
    {
        ItemEntity* itemEntity = m_itemEntities[i];
        if (itemEntity != nullptr)
        {
            itemEntity->Update(deltaSeconds);

            // Remove despawned entities (after 5 minutes)
            if (itemEntity->IsDespawned())
            {
                delete itemEntity;
                m_itemEntities[i] = m_itemEntities.back();
                m_itemEntities.pop_back();
                // Don't increment i - check the swapped entity
            }
            else
            {
                ++i;
            }
        }
        else
        {
            // Remove null entities
            m_itemEntities[i] = m_itemEntities.back();
            m_itemEntities.pop_back();
        }
    }

    // Process asynchronous job operations
    ProcessCompletedJobs();  // Process all completed jobs (generation, load, save)

    // Assignment 5 Phase 4: Process dirty light queue with 16ms budget
    // CRITICAL: Process lighting BEFORE mesh rebuilds to prevent progressive brightening
    // Meshes read lighting values, so lighting must propagate first
    ProcessDirtyLighting(0.016f);

    // Process dirty meshes AFTER lighting propagation
    ProcessDirtyChunkMeshes();

    // Get camera position for chunk management decisions
    Vec3 const cameraPos = GetCameraPosition();

    // At startup or when very few chunks exist, activate multiple chunks per frame
    // to quickly populate the world around the player
    size_t activeChunkCount;
    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        activeChunkCount = m_activeChunks.size();
    }

    // DEBUG MODE: Fixed world generation (Task 5B.4)
    // Generate exactly 256 chunks (16×16 grid) centered at origin, then stop
    if (DEBUG_FIXED_WORLD_GEN)
    {
        // Generate all 256 chunks at startup (aggressive burst)
        int chunksToActivateThisFrame = (activeChunkCount < 256) ? 50 : 0;

        // 1. Check if initial world generation is complete (all 256 chunks activated)
        // CRITICAL FIX: Defer ALL mesh rebuilding until world gen is complete
        // This prevents progressive brightening from cross-chunk lighting propagation
        if (!m_initialWorldGenComplete && activeChunkCount >= 256)
        {
            m_initialWorldGenComplete = true;
        }

        // 2. Only rebuild meshes after initial world gen is complete AND light queue is empty
        if (m_initialWorldGenComplete && m_dirtyLightQueue.empty())
        {
            // Assignment 5 Phase 10 FIX: Mark chunks with changed lighting as mesh-dirty FIRST
            {
                std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
                if (!m_chunksNeedingMeshRebuild.empty())
                {
                    for (Chunk* chunk : m_chunksNeedingMeshRebuild)
                    {
                        if (chunk)
                        {
                            chunk->SetIsMeshDirty(true);
                        }
                    }
                    m_chunksNeedingMeshRebuild.clear();
                }
            }

            Chunk* nearestDirtyChunk = FindNearestDirtyChunk(cameraPos);
            if (nearestDirtyChunk != nullptr)
            {
                nearestDirtyChunk->RebuildMesh();
                nearestDirtyChunk->SetIsMeshDirty(false);
            }
        }

        // 3. Generate fixed 16×16 chunk grid only (never generate beyond this)
        for (int i = 0; i < chunksToActivateThisFrame; i++)
        {
            // Find next chunk in fixed grid that hasn't been generated yet
            bool foundChunk = false;
            for (int chunkX = -DEBUG_FIXED_WORLD_HALF_SIZE; chunkX < DEBUG_FIXED_WORLD_HALF_SIZE && !foundChunk; chunkX++)
            {
                for (int chunkY = -DEBUG_FIXED_WORLD_HALF_SIZE; chunkY < DEBUG_FIXED_WORLD_HALF_SIZE && !foundChunk; chunkY++)
                {
                    IntVec2 chunkCoords(chunkX, chunkY);

                    // Check if this chunk already exists
                    bool exists = false;
                    {
                        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
                        exists = (m_activeChunks.find(chunkCoords) != m_activeChunks.end());
                    }
                    if (!exists)
                    {
                        std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
                        for (Chunk* chunk : m_nonActiveChunks)
                        {
                            if (chunk->GetChunkCoords() == chunkCoords)
                            {
                                exists = true;
                                break;
                            }
                        }
                    }

                    if (!exists)
                    {
                        ActivateChunk(chunkCoords);
                        foundChunk = true;
                    }
                }
            }
            if (!foundChunk) break; // All 256 chunks generated
        }

        // 3. NEVER deactivate chunks in debug mode (keep all 256 active)
        return; // Skip normal chunk management
    }

    // NORMAL MODE: Dynamic world generation
    // Tiered burst mode activation thresholds for responsive world generation
    // Adjusted for CHUNK_ACTIVATION_RANGE = 480
    int chunksToActivateThisFrame = 1; // Default: steady state (minimal activation)

    if (activeChunkCount < 400)
    {
        chunksToActivateThisFrame = 50; // AGGRESSIVE burst: fill initial activation range quickly
    }
    else if (activeChunkCount < 1200)
    {
        chunksToActivateThisFrame = 30; // HIGH burst: continue populating surrounding areas
    }
    else if (activeChunkCount < 2500)
    {
        chunksToActivateThisFrame = 15; // MEDIUM burst: approaching full coverage
    }
    else if (activeChunkCount < 5000)
    {
        chunksToActivateThisFrame = 5; // LOW burst: taper off to steady state
    }
    // else: steady state (1 per frame) - world is well-populated

    // Execute chunk management actions
    // Priority: 1) Regenerate dirty chunk, 2) Activate missing chunks, 3) Deactivate distant chunk

    // 1. Check for dirty chunks and regenerate the single nearest dirty chunk
    // CRITICAL FIX: Only rebuild if light queue is empty to prevent progressive brightening
    if (m_dirtyLightQueue.empty())
    {
        // Assignment 5 Phase 10 FIX: Mark chunks with changed lighting as mesh-dirty FIRST
        {
            std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
            if (!m_chunksNeedingMeshRebuild.empty())
            {
                for (Chunk* chunk : m_chunksNeedingMeshRebuild)
                {
                    if (chunk)
                    {
                        chunk->SetIsMeshDirty(true);
                    }
                }
                m_chunksNeedingMeshRebuild.clear();
            }
        }

        Chunk* nearestDirtyChunk = FindNearestDirtyChunk(cameraPos);

        if (nearestDirtyChunk != nullptr)
        {
            nearestDirtyChunk->RebuildMesh();
            nearestDirtyChunk->SetIsMeshDirty(false);
        }
    }

    // 2. Activate missing chunks within activation range
    for (int i = 0; i < chunksToActivateThisFrame; i++)
    {
        IntVec2 const nearestMissingChunk = FindNearestMissingChunkInRange(cameraPos);

        if (nearestMissingChunk != IntVec2(INT_MAX, INT_MAX)) // Valid chunk found
        {
            ActivateChunk(nearestMissingChunk);
        }
        else
        {
            break; // No more chunks to activate in range
        }
    }

    // 3. Deactivate the farthest active chunk if outside deactivation range
    IntVec2 const farthestChunk = FindFarthestActiveChunkOutsideDeactivationRange(cameraPos);

    if (farthestChunk != IntVec2(INT_MAX, INT_MAX)) // Valid chunk found
    {
        DeactivateChunk(farthestChunk);
    }

    // Assignment 7-AI: Update all agents
    UpdateAgents(deltaSeconds);
}

//----------------------------------------------------------------------------------------------------
void World::Render() const
{
    // Assignment 5 Phase 8: Bind World shader and update lighting constants
    if (m_worldShader != nullptr && m_worldConstantBuffer != nullptr)
    {
        g_renderer->BindShader(m_worldShader);
    }

    // Continue with constant buffer update ONLY if shader was bound successfully
    if (m_worldShader != nullptr && m_worldConstantBuffer != nullptr)
    {

        // Update world constants for day/night cycle (A5 specification compliant)
        WorldConstants worldConstants;

        // Camera position (for fog distance calculation)
        Vec3 cameraPos3 = GetCameraPosition();
        worldConstants.cameraPosition[0] = cameraPos3.x;
        worldConstants.cameraPosition[1] = cameraPos3.y;
        worldConstants.cameraPosition[2] = cameraPos3.z;
        worldConstants.cameraPosition[3] = 1.0f;

        // Assignment 5 Stage 8: Use computed indoor light color with glowstone flicker
        worldConstants.indoorLightColor[0] = m_finalIndoorColor.r / 255.0f;
        worldConstants.indoorLightColor[1] = m_finalIndoorColor.g / 255.0f;
        worldConstants.indoorLightColor[2] = m_finalIndoorColor.b / 255.0f;
        worldConstants.indoorLightColor[3] = 1.0f;

        // Assignment 5 Stage 8: Use computed outdoor light color with day/night + lightning
        worldConstants.outdoorLightColor[0] = m_finalOutdoorColor.r / 255.0f;
        worldConstants.outdoorLightColor[1] = m_finalOutdoorColor.g / 255.0f;
        worldConstants.outdoorLightColor[2] = m_finalOutdoorColor.b / 255.0f;
        worldConstants.outdoorLightColor[3] = 1.0f;

        // Assignment 5 Stage 8: Use computed sky color with day/night + lightning
        // Store fog max alpha in skyColor.a (0.8 = 80% fog blending at max distance)
        worldConstants.skyColor[0] = m_skyColor.r / 255.0f;
        worldConstants.skyColor[1] = m_skyColor.g / 255.0f;
        worldConstants.skyColor[2] = m_skyColor.b / 255.0f;
        worldConstants.skyColor[3] = 0.8f;

        // Fog distances based on chunk activation range
        // Near distance: start fog at 60% of activation range (gives clear vision before fading)
        // Far distance: full fog at activation range boundary
        float activationRange = static_cast<float>(CHUNK_ACTIVATION_RANGE);
        worldConstants.fogNearDistance = activationRange * 0.6f;  // 480 * 0.6 = 288 blocks
        worldConstants.fogFarDistance = activationRange;          // 480 blocks

        worldConstants.gameTime = m_gameTime;
        worldConstants.padding = 0.0f;

        g_renderer->CopyCPUToGPU(&worldConstants, sizeof(WorldConstants), m_worldConstantBuffer);
        g_renderer->BindConstantBuffer(8, m_worldConstantBuffer);  // Register b8 for WorldConstants
    }

    std::lock_guard<std::mutex> lock(m_activeChunksMutex);
    for (std::pair<IntVec2 const, Chunk*> const& chunkPair : m_activeChunks)
    {
        if (chunkPair.second != nullptr)
        {
            chunkPair.second->Render();
        }
    }

    // Assignment 7: Render all ItemEntities
    for (ItemEntity* itemEntity : m_itemEntities)
    {
        if (itemEntity != nullptr)
        {
            itemEntity->Render();
        }
    }

    // Assignment 7-AI: Render all agents
    RenderAgents();
}

//----------------------------------------------------------------------------------------------------
void World::ActivateChunk(IntVec2 const& chunkCoords)
{
    // Check if chunk is already active (with mutex protection)
    {
        std::lock_guard lock(m_activeChunksMutex);
        if (m_activeChunks.contains(chunkCoords))
        {
            return; // Already active
        }
    }

    // Check if chunk is already queued for generation (with mutex protection)
    {
        std::lock_guard lock(m_queuedChunksMutex);
        if (m_queuedGenerateChunks.find(chunkCoords) != m_queuedGenerateChunks.end())
        {
            return; // Already queued for generation
        }
    }

    // Create new chunk
    Chunk* newChunk = new Chunk(chunkCoords);

    // Set initial state
    newChunk->SetState(ChunkState::ACTIVATING);

    // Try to load from disk first using asynchronous I/O job
    if (ChunkExistsOnDisk(chunkCoords))
    {
        // Submit LoadChunkJob to I/O worker thread
        SubmitChunkForLoading(newChunk);
    }
    else
    {
        // No save file exists, submit for asynchronous generation
        SubmitChunkForGeneration(newChunk);
    }
}

//----------------------------------------------------------------------------------------------------
void World::DeactivateChunk(IntVec2 const& chunkCoords, bool forceSynchronousSave)
{
    // CRITICAL FIX: Copy chunkCoords to local variable BEFORE erasing from map
    // Problem: chunkCoords is a reference to map key (it->first)
    // When we erase(it), the reference becomes invalid → crash when used later
    IntVec2 localChunkCoords = chunkCoords;

    Chunk* chunk = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        auto const                  it = m_activeChunks.find(localChunkCoords);

        if (it == m_activeChunks.end())
        {
            // DebuggerPrintf("[DEACTIVATE] Chunk(%d,%d) not in active chunks map\n",
            //               localChunkCoords.x, localChunkCoords.y);
            return; // Chunk not active
        }

        chunk = it->second;
        if (chunk == nullptr)
        {
            // DebuggerPrintf("[DEACTIVATE] Chunk(%d,%d) pointer is NULL, removing from map\n",
            //               localChunkCoords.x, localChunkCoords.y);
            m_activeChunks.erase(it);
            return;
        }

        // Remove from active chunks map
        m_activeChunks.erase(it);
    }

    // DebuggerPrintf("[DEACTIVATE] Chunk(%d,%d) found, processing deactivation...\n",
    //               localChunkCoords.x, localChunkCoords.y);

    // Clear neighbor pointers (outside mutex - only affects this chunk's members)
    chunk->ClearNeighborPointers();

    // Assignment 5 Phase 10: Remove chunk from mesh rebuild tracking set
    {
        std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
        m_chunksNeedingMeshRebuild.erase(chunk);
    }

    // Update neighbors to remove references to this chunk
    ClearNeighborReferences(localChunkCoords);

    // Save to disk if needed
    if (chunk->GetNeedsSaving())
    {
        // DebuggerPrintf("[DEACTIVATE] Chunk(%d,%d) needs saving\n",
        //               localChunkCoords.x, localChunkCoords.y);
        if (forceSynchronousSave)
        {
            chunk->SaveToDisk();
            // DebuggerPrintf("[DEACTIVATE] Chunk(%d,%d) saved, deleting...\n",
            //               localChunkCoords.x, localChunkCoords.y);
            delete chunk;
        }
        else
        {
            SubmitChunkForSaving(chunk);
        }
    }
    else
    {
        // DebuggerPrintf("[DEACTIVATE] Chunk(%d,%d) doesn't need saving, deleting...\n",
        //               localChunkCoords.x, localChunkCoords.y);
        delete chunk;
    }
}

//----------------------------------------------------------------------------------------------------
void World::DeactivateAllChunks(bool forceSynchronousSave)
{
    while (!m_activeChunks.empty())
    {
        auto const it = m_activeChunks.begin();
        DeactivateChunk(it->first, forceSynchronousSave);
    }
}

//----------------------------------------------------------------------------------------------------
void World::RegenerateAllChunks()
{
    // Purpose: Force all active chunks to regenerate with fresh procedural terrain
    // This is critical for rapid iteration when tuning world generation parameters via ImGui
    //
    // Strategy:
    // 1. Cancel all pending mesh jobs (prevents dangling pointers when chunks are deleted)
    // 2. Mark all active chunks as NOT needing save (prevents old terrain from being written to disk)
    // 3. Deactivate all chunks (which will skip saving since needsSaving=false)
    // 4. Natural chunk activation system will regenerate chunks with current generation parameters

    // Clear the queued chunks tracking set FIRST to stop new jobs from being submitted
    {
        std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
        m_queuedGenerateChunks.clear();
    }

    // CRITICAL FIX: Wait for ALL currently executing jobs to complete BEFORE deleting any jobs
    // Problem: Deleting jobs/chunks while worker threads are still executing causes crashes
    // - Worker threads access freed chunk memory → 0x01010101 crash pattern
    // - SetBlock() writes to freed memory → corrupted chunks saved to disk
    //
    // Solution: Poll until all workers finish their current Execute() calls
    // This ensures no worker thread is accessing chunk memory before we delete it
    //
    // Why this works:
    // 1. We cleared m_queuedGenerateChunks - no new jobs will be submitted
    // 2. Workers will finish their current job and find no more queued jobs
    // 3. GetExecutingJobCount() will drop to 0 when all jobs complete
    // 4. Only THEN is it safe to delete job objects and chunks
    //
    // IMPORTANT: Must wait BEFORE deleting any jobs (including mesh jobs)
    // Deleting a job while it's executing causes orphaned threads and infinite hang
    while (g_jobSystem->GetExecutingJobCount() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // Give worker threads time to finish GenerateTerrain(), LoadFromDisk(), RebuildMesh(), etc.
        // This prevents accessing freed memory (chunk coordinates, block array)
    }

    // CRITICAL FIX: Retrieve ALL completed jobs from JobSystem BEFORE deleting chunks
    // Problem: Completed jobs are in JobSystem's completedJobs queue, NOT in our tracking lists
    // If we delete chunks first, ProcessCompletedJobs() will try to access deleted chunks
    //
    // Solution: Retrieve completed jobs, remove from tracking lists, then delete ALL jobs together
    std::vector<Job*> completedJobs = g_jobSystem->RetrieveAllCompletedJobs();

    // Remove completed jobs from tracking lists to prevent double-delete
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);

        // Remove completed mesh jobs from tracking list
        for (Job* completedJob : completedJobs)
        {
            // Try to find and remove from mesh jobs list
            auto meshIt = std::find(m_chunkMeshJobs.begin(), m_chunkMeshJobs.end(),
                                   static_cast<ChunkMeshJob*>(completedJob));
            if (meshIt != m_chunkMeshJobs.end())
            {
                m_chunkMeshJobs.erase(meshIt);
                continue;  // Found in mesh jobs, skip other lists
            }

            // Try to find and remove from generation jobs list
            auto genIt = std::find(m_chunkGenerationJobs.begin(), m_chunkGenerationJobs.end(),
                                  static_cast<ChunkGenerateJob*>(completedJob));
            if (genIt != m_chunkGenerationJobs.end())
            {
                m_chunkGenerationJobs.erase(genIt);
                continue;  // Found in generation jobs, skip other lists
            }

            // Try to find and remove from load jobs list
            auto loadIt = std::find(m_chunkLoadJobs.begin(), m_chunkLoadJobs.end(),
                                   static_cast<ChunkLoadJob*>(completedJob));
            if (loadIt != m_chunkLoadJobs.end())
            {
                m_chunkLoadJobs.erase(loadIt);
            }
        }

        // NOW delete all remaining tracked jobs (not yet completed)
        for (ChunkMeshJob* meshJob : m_chunkMeshJobs)
        {
            if (meshJob != nullptr)
            {
                delete meshJob;
            }
        }
        m_chunkMeshJobs.clear();

        for (ChunkGenerateJob* genJob : m_chunkGenerationJobs)
        {
            if (genJob != nullptr)
            {
                delete genJob;
            }
        }
        m_chunkGenerationJobs.clear();

        for (ChunkLoadJob* loadJob : m_chunkLoadJobs)
        {
            if (loadJob != nullptr)
            {
                delete loadJob;
            }
        }
        m_chunkLoadJobs.clear();
    }

    // Finally, delete all completed jobs (removed from tracking lists above)
    for (Job* job : completedJobs)
    {
        delete job;
    }

    // NOW it's safe to delete orphaned chunks in m_nonActiveChunks
    // No worker thread is accessing this memory anymore
    {
        std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
        for (Chunk* chunk : m_nonActiveChunks)
        {
            if (chunk != nullptr)
            {
                delete chunk;  // Safe now - releases DirectX resources in ~Chunk() destructor
            }
        }
        m_nonActiveChunks.clear();
    }

    // Mark all chunks as not needing save
    // CRITICAL: This prevents RegenerateAllChunks() from writing old terrain to disk
    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        for (auto& chunkPair : m_activeChunks)
        {
            if (chunkPair.second != nullptr)
            {
                chunkPair.second->SetNeedsSaving(false);
            }
        }
    }

    // CRITICAL FIX: Delete ALL saved chunk files to force fresh terrain generation
    // This ensures chunks regenerate with NEW terrain instead of loading old saves
    // Bug: Without this, ActivateChunk() finds saved files and loads old terrain!
    try
    {
        std::string saveDir = "Saves/";
        if (std::filesystem::exists(saveDir))
        {
            // Iterate through all .chunk files in Saves/ directory
            for (auto const& entry : std::filesystem::directory_iterator(saveDir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".chunk")
                {
                    try
                    {
                        std::filesystem::remove(entry.path());
                        DebuggerPrintf("Deleted chunk file: %s\n", entry.path().string().c_str());
                    }
                    catch (std::filesystem::filesystem_error const& e)
                    {
                        DebuggerPrintf("Failed to delete chunk file %s: %s\n",
                                       entry.path().string().c_str(), e.what());
                    }
                }
            }
        }
    }
    catch (std::filesystem::filesystem_error const& e)
    {
        DebuggerPrintf("Failed to iterate Saves/ directory: %s\n", e.what());
    }

    // Deactivate all chunks (they won't be saved due to SetNeedsSaving(false))
    DeactivateAllChunks();

    // Note: Chunks will automatically reactivate and regenerate during the next Update()
    // when the activation system detects missing chunks around the player
}

//----------------------------------------------------------------------------------------------------
void World::ToggleGlobalChunkDebugDraw()
{
    std::lock_guard<std::mutex> lock(m_activeChunksMutex);
    m_globalChunkDebugDraw = !m_globalChunkDebugDraw;
    for (auto& chunkPair : m_activeChunks)
    {
        if (chunkPair.second != nullptr)
        {
            chunkPair.second->SetDebugDraw(m_globalChunkDebugDraw);
        }
    }
}

//----------------------------------------------------------------------------------------------------
void World::SetDebugVisualizationMode(DebugVisualizationMode mode)
{
    // Only regenerate if mode actually changed
    if (m_debugVisualizationMode == mode)
    {
        return;
    }

    m_debugVisualizationMode = mode;

    // Regenerate all chunks to apply new visualization
    // This ensures chunks display the selected noise layer
    RegenerateAllChunks();
}

//----------------------------------------------------------------------------------------------------
bool World::SetBlockAtGlobalCoords(IntVec3 const& globalCoords,
                                   uint8_t const  blockTypeIndex)
{
    // Get the chunk that contains this global coordinate
    IntVec2 chunkCoords = Chunk::GetChunkCoords(globalCoords);
    Chunk*  chunk       = GetChunk(chunkCoords);

    if (chunk == nullptr)
    {
        return false; // Chunk not active, cannot modify
    }

    // Convert global coordinates to local coordinates within the chunk
    IntVec3 localCoords = Chunk::GlobalCoordsToLocalCoords(globalCoords);

    // Validate Z coordinate is within chunk bounds
    if (localCoords.z < 0 || localCoords.z > CHUNK_MAX_Z)
    {
        return false; // Z coordinate out of bounds
    }

    // Set the block using the chunk's SetBlock method (which handles save/mesh dirty flags and lighting)
    chunk->SetBlock(localCoords.x, localCoords.y, localCoords.z, blockTypeIndex, this);

    // Mark neighboring chunks as dirty if the modified block is on a chunk boundary
    // This ensures proper face culling updates across chunk edges
    if (localCoords.x == 0) // West boundary
    {
        IntVec2 westChunkCoords = IntVec2(chunkCoords.x - 1, chunkCoords.y);
        Chunk*  westChunk       = GetChunk(westChunkCoords);
        if (westChunk != nullptr)
        {
            westChunk->SetIsMeshDirty(true);
        }
    }
    else if (localCoords.x == CHUNK_MAX_X) // East boundary
    {
        IntVec2 eastChunkCoords = IntVec2(chunkCoords.x + 1, chunkCoords.y);
        Chunk*  eastChunk       = GetChunk(eastChunkCoords);
        if (eastChunk != nullptr)
        {
            eastChunk->SetIsMeshDirty(true);
        }
    }

    if (localCoords.y == 0) // South boundary
    {
        IntVec2 southChunkCoords = IntVec2(chunkCoords.x, chunkCoords.y - 1);
        Chunk*  southChunk       = GetChunk(southChunkCoords);
        if (southChunk != nullptr)
        {
            southChunk->SetIsMeshDirty(true);
        }
    }
    else if (localCoords.y == CHUNK_MAX_Y) // North boundary
    {
        IntVec2 northChunkCoords = IntVec2(chunkCoords.x, chunkCoords.y + 1);
        Chunk*  northChunk       = GetChunk(northChunkCoords);
        if (northChunk != nullptr)
        {
            northChunk->SetIsMeshDirty(true);
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------
uint8_t World::GetBlockTypeAtGlobalCoords(IntVec3 const& globalCoords) const
{
    // Get the chunk that contains this global coordinate
    IntVec2 chunkCoords = Chunk::GetChunkCoords(globalCoords);
    Chunk*  chunk       = GetChunk(chunkCoords);

    if (chunk == nullptr)
    {
        return 0; // Return air block if chunk not active
    }

    // Convert global coordinates to local coordinates within the chunk
    IntVec3 localCoords = Chunk::GlobalCoordsToLocalCoords(globalCoords);

    // Validate Z coordinate is within chunk bounds
    if (localCoords.z < 0 || localCoords.z > CHUNK_MAX_Z)
    {
        return 0; // Return air block if out of bounds
    }

    // Get the block using the chunk's GetBlock method
    Block* block = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);

    if (block == nullptr)
    {
        return 0; // Return air block if invalid
    }

    return block->m_typeIndex;
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Check if block at global coordinates is solid (opaque) for collision detection
// Returns false for air (type 0) and transparent blocks like glass/water
// Returns true for opaque blocks that should block player movement
//----------------------------------------------------------------------------------------------------
bool World::IsBlockSolid(IntVec3 const& globalCoords) const
{
    // Get block type at this position
    uint8_t blockType = GetBlockTypeAtGlobalCoords(globalCoords);

    // Air (type 0) is never solid
    if (blockType == 0)
    {
        return false;
    }

    // Get the block definition to check opacity
    sBlockDefinition* blockDef = sBlockDefinition::GetDefinitionByIndex(blockType);

    // If no definition, treat as non-solid (safe default)
    if (blockDef == nullptr)
    {
        return false;
    }

    // Opaque blocks are solid for collision purposes
    return blockDef->IsOpaque();
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Check if entity is standing on solid ground using 4-corner raycast
// Casts 4 short downward rays from AABB bottom corners to detect ground contact
// Returns true if ANY corner ray hits a solid block (handles edges and slopes)
//----------------------------------------------------------------------------------------------------
bool World::IsEntityOnGround(Entity const* entity) const
{
    // Get entity AABB in world space
    AABB3 worldAABB = entity->GetWorldAABB();

    // Get 4 bottom corners of the AABB
    Vec3 corners[4];
    corners[0] = Vec3(worldAABB.m_mins.x, worldAABB.m_mins.y, worldAABB.m_mins.z); // Bottom-left-front
    corners[1] = Vec3(worldAABB.m_maxs.x, worldAABB.m_mins.y, worldAABB.m_mins.z); // Bottom-right-front
    corners[2] = Vec3(worldAABB.m_mins.x, worldAABB.m_maxs.y, worldAABB.m_mins.z); // Bottom-left-back
    corners[3] = Vec3(worldAABB.m_maxs.x, worldAABB.m_maxs.y, worldAABB.m_mins.z); // Bottom-right-back

    // Check each corner with a short downward raycast
    for (int i = 0; i < 4; ++i)
    {
        // Start ray slightly above corner to avoid self-collision
        Vec3 rayStart = corners[i] + Vec3(0.0f, 0.0f, RAYCAST_OFFSET);

        // Raycast straight down
        Vec3 rayDirection = Vec3(0.0f, 0.0f, -1.0f);

        // Ray length: twice the offset to check just below the AABB
        // GROUND_CHECK_DISTANCE + RAYCAST_OFFSET = total downward distance
        float rayLength = GROUND_CHECK_DISTANCE + RAYCAST_OFFSET;

        // Perform raycast
        RaycastResult result = RaycastVoxel(rayStart, rayDirection, rayLength);

        // If ray hit something and it's a solid block, entity is grounded
        if (result.m_didImpact && IsBlockSolid(result.m_impactBlockCoords))
        {
            return true;
        }
    }

    // No corner detected solid ground
    return false;
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Iteratively resolve entity-block overlaps by pushing along minimum penetration axis
// Uses up to MAX_PUSH_ITERATIONS to resolve complex multi-block overlaps
// Zeros velocity on pushed axes to prevent entity from continuing into blocks
//----------------------------------------------------------------------------------------------------
void World::PushEntityOutOfBlocks(Entity* entity)
{
    // Iteratively resolve overlaps (pushing on one axis may create overlap on another)
    for (int iteration = 0; iteration < MAX_PUSH_ITERATIONS; ++iteration)
    {
        // Get entity's current world-space AABB
        AABB3 worldAABB = entity->GetWorldAABB();

        // Calculate block coordinate bounds that entity AABB spans
        IntVec3 minBlockCoords(
            static_cast<int>(floorf(worldAABB.m_mins.x)),
            static_cast<int>(floorf(worldAABB.m_mins.y)),
            static_cast<int>(floorf(worldAABB.m_mins.z))
        );
        IntVec3 maxBlockCoords(
            static_cast<int>(floorf(worldAABB.m_maxs.x)),
            static_cast<int>(floorf(worldAABB.m_maxs.y)),
            static_cast<int>(floorf(worldAABB.m_maxs.z))
        );

        // Track if we found any overlaps this iteration
        bool foundOverlap = false;

        // Check all blocks that entity AABB spans
        for (int z = minBlockCoords.z; z <= maxBlockCoords.z; ++z)
        {
            for (int y = minBlockCoords.y; y <= maxBlockCoords.y; ++y)
            {
                for (int x = minBlockCoords.x; x <= maxBlockCoords.x; ++x)
                {
                    IntVec3 blockCoords(x, y, z);

                    // Skip non-solid blocks
                    if (!IsBlockSolid(blockCoords))
                    {
                        continue;
                    }

                    // Create AABB for this solid block (1x1x1 cube at block position)
                    AABB3 blockAABB;
                    blockAABB.m_mins = Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                    blockAABB.m_maxs = blockAABB.m_mins + Vec3(1.0f, 1.0f, 1.0f);

                    // Check if entity AABB overlaps this block AABB using separating axis test
                    // Boxes overlap if they overlap on ALL three axes
                    bool overlapsX = !(worldAABB.m_maxs.x <= blockAABB.m_mins.x || worldAABB.m_mins.x >= blockAABB.m_maxs.x);
                    bool overlapsY = !(worldAABB.m_maxs.y <= blockAABB.m_mins.y || worldAABB.m_mins.y >= blockAABB.m_maxs.y);
                    bool overlapsZ = !(worldAABB.m_maxs.z <= blockAABB.m_mins.z || worldAABB.m_mins.z >= blockAABB.m_maxs.z);

                    if (!overlapsX || !overlapsY || !overlapsZ)
                    {
                        continue; // No overlap
                    }

                    // Calculate penetration depth on each axis
                    float penetrationX1 = blockAABB.m_maxs.x - worldAABB.m_mins.x; // Push entity +X
                    float penetrationX2 = worldAABB.m_maxs.x - blockAABB.m_mins.x; // Push entity -X
                    float penetrationY1 = blockAABB.m_maxs.y - worldAABB.m_mins.y; // Push entity +Y
                    float penetrationY2 = worldAABB.m_maxs.y - blockAABB.m_mins.y; // Push entity -Y
                    float penetrationZ1 = blockAABB.m_maxs.z - worldAABB.m_mins.z; // Push entity +Z
                    float penetrationZ2 = worldAABB.m_maxs.z - blockAABB.m_mins.z; // Push entity -Z

                    // Find minimum penetration (smallest push distance)
                    float minPenetration = penetrationX1;
                    int   pushAxis       = 0; // 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z

                    if (penetrationX2 < minPenetration) { minPenetration = penetrationX2; pushAxis = 1; }
                    if (penetrationY1 < minPenetration) { minPenetration = penetrationY1; pushAxis = 2; }
                    if (penetrationY2 < minPenetration) { minPenetration = penetrationY2; pushAxis = 3; }
                    if (penetrationZ1 < minPenetration) { minPenetration = penetrationZ1; pushAxis = 4; }
                    if (penetrationZ2 < minPenetration) { minPenetration = penetrationZ2; pushAxis = 5; }

                    // Push entity position along minimum penetration axis
                    switch (pushAxis)
                    {
                        case 0: entity->m_position.x += penetrationX1; entity->m_velocity.x = 0.0f; break; // Push +X
                        case 1: entity->m_position.x -= penetrationX2; entity->m_velocity.x = 0.0f; break; // Push -X
                        case 2: entity->m_position.y += penetrationY1; entity->m_velocity.y = 0.0f; break; // Push +Y
                        case 3: entity->m_position.y -= penetrationY2; entity->m_velocity.y = 0.0f; break; // Push -Y
                        case 4: entity->m_position.z += penetrationZ1; entity->m_velocity.z = 0.0f; break; // Push +Z
                        case 5: entity->m_position.z -= penetrationZ2; entity->m_velocity.z = 0.0f; break; // Push -Z
                    }

                    // Mark that we found and resolved an overlap
                    foundOverlap = true;

                    // Break to outer loop to recalculate AABB and check for new overlaps
                    goto next_iteration;
                }
            }
        }

    next_iteration:
        // If no overlaps found, entity is fully resolved
        if (!foundOverlap)
        {
            return;
        }
    }

    // If we reach here, max iterations exceeded (shouldn't happen often)
    // This can occur with very complex overlaps or entity spawning deep inside geometry
    DebuggerPrintf("WARNING: PushEntityOutOfBlocks() exceeded MAX_PUSH_ITERATIONS=%d for entity at (%.2f, %.2f, %.2f)\n",
                   MAX_PUSH_ITERATIONS, entity->m_position.x, entity->m_position.y, entity->m_position.z);
}

//----------------------------------------------------------------------------------------------------
// Assignment 7: Spawn dropped item entity in world
// Creates ItemEntity at specified position with given ItemStack and adds to entity list
//----------------------------------------------------------------------------------------------------
void World::SpawnItemEntity(Vec3 const& position, ItemStack const& itemStack)
{
    // Don't spawn empty items
    if (itemStack.IsEmpty())
    {
        return;
    }

    // Create new ItemEntity at spawn position
    ItemEntity* itemEntity = new ItemEntity(g_game, position, itemStack);

    // Add to world's item entity list
    m_itemEntities.push_back(itemEntity);

    // Debug: Log item spawn
    DebuggerPrintf("Spawned ItemEntity: itemID=%u, quantity=%u at (%.1f, %.1f, %.1f)\n",
                   itemStack.itemID, itemStack.quantity, position.x, position.y, position.z);
}

//----------------------------------------------------------------------------------------------------
// Assignment 7: Get ItemEntities within radius of position
// Returns vector of ItemEntity pointers within specified distance (for magnetic pickup)
//----------------------------------------------------------------------------------------------------
std::vector<ItemEntity*> World::GetNearbyItemEntities(Vec3 const& position, float radius) const
{
    std::vector<ItemEntity*> nearbyItems;

    // Calculate radius squared to avoid sqrt in distance checks
    float radiusSquared = radius * radius;

    // Check all item entities in the world
    for (ItemEntity* itemEntity : m_itemEntities)
    {
        if (itemEntity == nullptr || itemEntity->IsDespawned())
        {
            continue; // Skip null or despawned items
        }

        // Calculate distance squared from position to item
        Vec3 toItem = itemEntity->m_position - position;
        float distanceSquared = toItem.GetLengthSquared();

        // Add to results if within radius
        if (distanceSquared <= radiusSquared)
        {
            nearbyItems.push_back(itemEntity);
        }
    }

    return nearbyItems;
}

//----------------------------------------------------------------------------------------------------
// Assignment 7-AI: Spawn AI agent at position, returns unique ID
// Task a4cea8fc-2ca6-47f5-8ddb-6c07cc3278cc
//----------------------------------------------------------------------------------------------------
uint64_t World::SpawnAgent(std::string const& name, Vec3 const& position)
{
    // Generate unique ID
    uint64_t agentID = m_nextAgentID++;

    // Create Agent entity
    Agent* agent = new Agent(g_game, name, agentID, position);

    // Add to agent map for O(1) lookup by ID
    m_agents[agentID] = agent;

    DebuggerPrintf("World: Spawned Agent '%s' with ID %llu at (%.1f, %.1f, %.1f)\n",
                   name.c_str(), agentID, position.x, position.y, position.z);

    return agentID;
}

//----------------------------------------------------------------------------------------------------
// Assignment 7-AI: Find agent by ID (for KADI tool handlers)
// Returns nullptr if agent not found
//----------------------------------------------------------------------------------------------------
Agent* World::FindAgentByID(uint64_t agentID)
{
    auto it = m_agents.find(agentID);
    if (it != m_agents.end())
    {
        return it->second;
    }
    return nullptr;
}

//----------------------------------------------------------------------------------------------------
// Assignment 7-AI: Remove agent from world and delete
//----------------------------------------------------------------------------------------------------
void World::DespawnAgent(uint64_t agentID)
{
    auto it = m_agents.find(agentID);
    if (it != m_agents.end())
    {
        Agent* agent = it->second;
        std::string agentName = agent->GetName();

        // Delete agent entity
        delete agent;

        // Remove from map
        m_agents.erase(it);

        DebuggerPrintf("World: Despawned Agent '%s' (ID: %llu)\n", agentName.c_str(), agentID);
    }
    else
    {
        ERROR_RECOVERABLE(Stringf("World::DespawnAgent: Agent ID %llu not found", agentID));
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 7-AI: Get all active agents
//----------------------------------------------------------------------------------------------------
std::vector<Agent*> World::GetAllAgents() const
{
    std::vector<Agent*> agents;
    agents.reserve(m_agents.size());

    for (auto const& pair : m_agents)
    {
        agents.push_back(pair.second);
    }

    return agents;
}

//----------------------------------------------------------------------------------------------------
// Assignment 7-AI: Update all agents (called by World::Update)
//----------------------------------------------------------------------------------------------------
void World::UpdateAgents(float deltaSeconds)
{
    // Update all agents
    for (auto const& pair : m_agents)
    {
        Agent* agent = pair.second;
        if (agent != nullptr)
        {
            agent->Update(deltaSeconds);
        }
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 7-AI: Render all agents (called by World::Render)
//----------------------------------------------------------------------------------------------------
void World::RenderAgents() const
{
    // Render all agents
    for (auto const& pair : m_agents)
    {
        Agent* agent = pair.second;
        if (agent != nullptr)
        {
            agent->Render();
        }
    }
}

//----------------------------------------------------------------------------------------------------
Chunk* World::GetChunk(IntVec2 const& chunkCoords) const
{
    std::lock_guard lock(m_activeChunksMutex);
    auto const      it = m_activeChunks.find(chunkCoords);

    if (it != m_activeChunks.end())
    {
        return it->second;
    }
    return nullptr;
}

//----------------------------------------------------------------------------------------------------
int World::GetActiveChunkCount() const
{
    std::lock_guard lock(m_activeChunksMutex);
    return (int)m_activeChunks.size();
}

//----------------------------------------------------------------------------------------------------
int World::GetTotalVertexCount() const
{
    std::lock_guard lock(m_activeChunksMutex);
    int             totalVertices = 0;
    for (const auto& pair : m_activeChunks)
    {
        if (pair.second)
        {
            totalVertices += pair.second->GetVertexCount();
        }
    }
    return totalVertices;
}

//----------------------------------------------------------------------------------------------------
int World::GetTotalIndexCount() const
{
    std::lock_guard lock(m_activeChunksMutex);
    int             totalIndices = 0;
    for (const auto& pair : m_activeChunks)
    {
        if (pair.second)
        {
            totalIndices += pair.second->GetIndexCount();
        }
    }
    return totalIndices;
}

//----------------------------------------------------------------------------------------------------
int World::GetPendingGenerateJobCount() const
{
    std::lock_guard lock(m_jobListsMutex);
    return (int)m_chunkGenerationJobs.size();
}

//----------------------------------------------------------------------------------------------------
int World::GetPendingLoadJobCount() const
{
    std::lock_guard lock(m_jobListsMutex);
    return (int)m_chunkLoadJobs.size();
}

//----------------------------------------------------------------------------------------------------
int World::GetPendingSaveJobCount() const
{
    std::lock_guard lock(m_jobListsMutex);
    return (int)m_chunkSaveJobs.size();
}

//----------------------------------------------------------------------------------------------------
Vec3 World::GetCameraPosition() const
{
    if (g_game != nullptr)
    {
        return g_game->GetPlayerCameraPosition();
    }
    return Vec3::ZERO; // Fallback if no game instance
}

//----------------------------------------------------------------------------------------------------
Vec3 World::GetPlayerVelocity() const
{
    if (g_game != nullptr)
    {
        return g_game->GetPlayerVelocity();
    }
    return Vec3::ZERO; // Fallback if no game instance
}

//----------------------------------------------------------------------------------------------------
float World::GetDistanceToChunkCenter(IntVec2 const& chunkCoords, Vec3 const& cameraPos) const
{
    IntVec2 chunkCenter   = Chunk::GetChunkCenter(chunkCoords);
    Vec3    chunkCenter3D = Vec3(static_cast<float>(chunkCenter.x), static_cast<float>(chunkCenter.y), cameraPos.z);

    // Only consider XY distance as specified
    float deltaX = chunkCenter3D.x - cameraPos.x;
    float deltaY = chunkCenter3D.y - cameraPos.y;
    return sqrtf(deltaX * deltaX + deltaY * deltaY);
}

//----------------------------------------------------------------------------------------------------
IntVec2 World::FindNearestMissingChunkInRange(Vec3 const& cameraPos) const
{
    IntVec2 cameraChunkCoords = Chunk::GetChunkCoords(IntVec3(static_cast<int>(cameraPos.x), static_cast<int>(cameraPos.y), static_cast<int>(cameraPos.z)));

    int maxRadius = (CHUNK_ACTIVATION_RANGE / 16) + 2; // CHUNK_SIZE_X is 16

    // Phase 0, Task 0.7: Get player velocity for directional preloading
    Vec3  playerVelocity        = GetPlayerVelocity();
    float velocityMagnitude     = playerVelocity.GetLength();
    bool  useDirectionalPreload = velocityMagnitude > PRELOAD_VELOCITY_THRESHOLD;

    // Calculate movement direction (normalized)
    Vec3 movementDirection = Vec3::ZERO;
    if (useDirectionalPreload)
    {
        movementDirection = playerVelocity.GetNormalized();
    }

    // OPTIMIZATION: Build lookup sets ONCE with single mutex lock per set
    // This reduces mutex locks from ~1,936 per call to just 2 total
    std::unordered_set<IntVec2> activeSet;
    std::unordered_set<IntVec2> queuedSet;

    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        for (auto const& pair : m_activeChunks)
        {
            activeSet.insert(pair.first);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
        queuedSet = m_queuedGenerateChunks; // Copy the set
    }

    // Phase 0, Task 0.7: Smart directional preloading
    // If player is moving, prioritize chunks ahead of movement direction
    if (useDirectionalPreload)
    {
        // Calculate lookahead position (3 chunks ahead in movement direction)
        float   lookaheadDistance = PRELOAD_LOOKAHEAD_CHUNKS * CHUNK_SIZE_X;
        Vec3    lookaheadPos      = cameraPos + (movementDirection * lookaheadDistance);
        IntVec2 lookaheadChunk    = Chunk::GetChunkCoords(IntVec3(static_cast<int>(lookaheadPos.x), static_cast<int>(lookaheadPos.y), 0));

        // Search in expanding rings around lookahead position
        for (int radius = 0; radius <= 2; radius++) // Only check nearby chunks around lookahead
        {
            for (int dx = -radius; dx <= radius; dx++)
            {
                for (int dy = -radius; dy <= radius; dy++)
                {
                    // Only check perimeter of current ring
                    if (abs(dx) != radius && abs(dy) != radius)
                    {
                        continue;
                    }

                    IntVec2 testChunk(lookaheadChunk.x + dx, lookaheadChunk.y + dy);

                    // Phase 0, Task 0.6: Fixed world bounds DISABLED for infinite world generation
                    // Previously limited to 16x16 chunks (256 total) for testing
                    // Now supports infinite terrain generation
                    /*
                    if (testChunk.x < WORLD_MIN_CHUNK_X || testChunk.x > WORLD_MAX_CHUNK_X ||
                        testChunk.y < WORLD_MIN_CHUNK_Y || testChunk.y > WORLD_MAX_CHUNK_Y)
                    {
                        continue;
                    }
                    */

                    // Fast O(1) lookups without mutex locks
                    if (activeSet.find(testChunk) != activeSet.end())
                    {
                        continue;
                    }
                    if (queuedSet.find(testChunk) != queuedSet.end())
                    {
                        continue;
                    }

                    // Check if within activation range
                    float distance = GetDistanceToChunkCenter(testChunk, cameraPos);
                    if (distance <= CHUNK_ACTIVATION_RANGE)
                    {
                        return testChunk; // Found chunk in movement direction! Prioritize this
                    }
                }
            }
        }
    }

    // Standard spiral search from camera position (fallback or when not moving)
    // OPTIMIZATION: Spiral search outward from camera (nearest-first)
    // Returns immediately when first valid chunk found, avoiding distance sorting
    for (int radius = 0; radius <= maxRadius; radius++)
    {
        // Check ring at 'radius' distance from camera
        for (int dx = -radius; dx <= radius; dx++)
        {
            for (int dy = -radius; dy <= radius; dy++)
            {
                // Only check perimeter of current ring (not interior)
                if (abs(dx) != radius && abs(dy) != radius)
                {
                    continue;
                }

                IntVec2 testChunk(cameraChunkCoords.x + dx, cameraChunkCoords.y + dy);

                // Phase 0, Task 0.6: Fixed world bounds DISABLED for infinite world generation
                // Previously limited to 16x16 chunks (256 total) for testing
                // Now supports infinite terrain generation
                /*
                if (testChunk.x < WORLD_MIN_CHUNK_X || testChunk.x > WORLD_MAX_CHUNK_X ||
                    testChunk.y < WORLD_MIN_CHUNK_Y || testChunk.y > WORLD_MAX_CHUNK_Y)
                {
                    continue;
                }
                */

                // Fast O(1) lookups without mutex locks
                if (activeSet.find(testChunk) != activeSet.end())
                {
                    continue;
                }
                if (queuedSet.find(testChunk) != queuedSet.end())
                {
                    continue;
                }

                // Check if within activation range
                float distance = GetDistanceToChunkCenter(testChunk, cameraPos);
                if (distance <= CHUNK_ACTIVATION_RANGE)
                {
                    return testChunk; // Found nearest! Return immediately
                }
            }
        }
    }

    return IntVec2(INT_MAX, INT_MAX); // No valid chunk found in range
}

//----------------------------------------------------------------------------------------------------
IntVec2 World::FindFarthestActiveChunkOutsideDeactivationRange(Vec3 const& cameraPos) const
{
    std::lock_guard<std::mutex> lock(m_activeChunksMutex);
    float                       farthestDistance = 0.0f;
    IntVec2                     farthestChunk    = IntVec2(INT_MAX, INT_MAX);

    for (auto const& chunkPair : m_activeChunks)
    {
        float distance = GetDistanceToChunkCenter(chunkPair.first, cameraPos);

        // Only consider chunks outside deactivation range
        if (distance > CHUNK_DEACTIVATION_RANGE && distance > farthestDistance)
        {
            farthestDistance = distance;
            farthestChunk    = chunkPair.first;
        }
    }

    return farthestChunk;
}

//----------------------------------------------------------------------------------------------------
Chunk* World::FindNearestDirtyChunk(Vec3 const& cameraPos) const
{
    std::lock_guard<std::mutex> lock(m_activeChunksMutex);
    float                       nearestDistance   = FLT_MAX;
    Chunk*                      nearestDirtyChunk = nullptr;

    for (const auto& chunkPair : m_activeChunks)
    {
        Chunk* chunk = chunkPair.second;
        if (chunk != nullptr && chunk->GetIsMeshDirty())
        {
            float distance = GetDistanceToChunkCenter(chunkPair.first, cameraPos);
            if (distance < nearestDistance)
            {
                nearestDistance   = distance;
                nearestDirtyChunk = chunk;
            }
        }
    }

    return nearestDirtyChunk;
}

//----------------------------------------------------------------------------------------------------
bool World::ChunkExistsOnDisk(IntVec2 const& chunkCoords) const
{
    std::string filename = StringFormat("Saves/Chunk({0},{1}).chunk", chunkCoords.x, chunkCoords.y);
    return std::filesystem::exists(filename);
}

//----------------------------------------------------------------------------------------------------
bool World::LoadChunkFromDisk(Chunk* chunk) const
{
    if (chunk == nullptr) return false;

    IntVec2     chunkCoords = chunk->GetChunkCoords();
    std::string filename    = StringFormat("Saves/Chunk({0},{1}).chunk", chunkCoords.x, chunkCoords.y);

    std::vector<uint8_t> buffer;
    if (!FileReadToBuffer(buffer, filename))
    {
        return false;
    }

    // Verify minimum file size (header + at least one RLE entry)
    if (buffer.size() < sizeof(ChunkFileHeader) + sizeof(ChunkRLEEntry))
    {
        return false; // File too small
    }

    // Read and validate header
    ChunkFileHeader header;
    memcpy(&header, buffer.data(), sizeof(ChunkFileHeader));

    // Validate header
    if (header.fourCC[0] != 'G' || header.fourCC[1] != 'C' ||
        header.fourCC[2] != 'H' || header.fourCC[3] != 'K')
    {
        return false; // Invalid 4CC
    }

    if (header.version != 1 || header.chunkBitsX != CHUNK_BITS_X ||
        header.chunkBitsY != CHUNK_BITS_Y || header.chunkBitsZ != CHUNK_BITS_Z)
    {
        return false; // Incompatible format
    }

    // Decompress RLE data
    size_t dataOffset = sizeof(ChunkFileHeader);
    int    blockIndex = 0;

    while (dataOffset < buffer.size() && blockIndex < BLOCKS_PER_CHUNK)
    {
        // Read RLE entry
        if (dataOffset + sizeof(ChunkRLEEntry) > buffer.size())
        {
            return false; // Incomplete RLE entry
        }

        ChunkRLEEntry entry;
        memcpy(&entry, buffer.data() + dataOffset, sizeof(ChunkRLEEntry));
        dataOffset += sizeof(ChunkRLEEntry);

        // Apply run to blocks
        for (int i = 0; i < entry.count && blockIndex < BLOCKS_PER_CHUNK; i++)
        {
            IntVec3 localCoords = Chunk::IndexToLocalCoords(blockIndex);
            Block*  block       = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
            if (block != nullptr)
            {
                block->m_typeIndex = entry.value;
            }
            blockIndex++;
        }
    }

    // Verify we loaded exactly the right number of blocks
    if (blockIndex != BLOCKS_PER_CHUNK)
    {
        return false; // RLE data doesn't match expected block count
    }

    return true;
}

//----------------------------------------------------------------------------------------------------
bool World::SaveChunkToDisk(Chunk* chunk) const
{
    if (chunk == nullptr) return false;

    IntVec2 chunkCoords = chunk->GetChunkCoords();

    // Debug output to help diagnose save issues
    DebuggerPrintf("Saving chunk (%d,%d) to disk...\n", chunkCoords.x, chunkCoords.y);

    // Ensure save directory exists (relative to executable in Run/ directory)
    std::string saveDir = "Saves/";
    std::filesystem::create_directories(saveDir);

    std::string filename = StringFormat("{0}Chunk({1},{2}).chunk", saveDir, chunkCoords.x, chunkCoords.y);

    // Collect block data in order for RLE compression
    std::vector<uint8_t> blockData(BLOCKS_PER_CHUNK);
    for (int i = 0; i < BLOCKS_PER_CHUNK; i++)
    {
        IntVec3 localCoords = Chunk::IndexToLocalCoords(i);
        Block*  block       = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
        if (block != nullptr)
        {
            blockData[i] = block->m_typeIndex;
        }
        else
        {
            blockData[i] = 0; // Air block if invalid
        }
    }

    // Compress using RLE
    std::vector<ChunkRLEEntry> rleEntries;
    uint8_t                    currentType = blockData[0];
    uint8_t                    runLength   = 1;

    for (int i = 1; i < BLOCKS_PER_CHUNK; i++)
    {
        if (blockData[i] == currentType && runLength < 255)
        {
            runLength++;
        }
        else
        {
            // End current run
            rleEntries.push_back({currentType, runLength});
            currentType = blockData[i];
            runLength   = 1;
        }
    }
    // Don't forget the last run
    rleEntries.push_back({currentType, runLength});

    // Create file header
    ChunkFileHeader header;
    header.fourCC[0]  = 'G';
    header.fourCC[1]  = 'C';
    header.fourCC[2]  = 'H';
    header.fourCC[3]  = 'K';
    header.version    = 1;
    header.chunkBitsX = CHUNK_BITS_X;
    header.chunkBitsY = CHUNK_BITS_Y;
    header.chunkBitsZ = CHUNK_BITS_Z;

    // Calculate total file size
    size_t               fileSize = sizeof(ChunkFileHeader) + rleEntries.size() * sizeof(ChunkRLEEntry);
    std::vector<uint8_t> fileBuffer(fileSize);

    // Write header
    memcpy(fileBuffer.data(), &header, sizeof(ChunkFileHeader));

    // Write RLE entries
    size_t offset = sizeof(ChunkFileHeader);
    for (const ChunkRLEEntry& entry : rleEntries)
    {
        memcpy(fileBuffer.data() + offset, &entry, sizeof(ChunkRLEEntry));
        offset += sizeof(ChunkRLEEntry);
    }

    // Write to file using safe fopen_s
    FILE*   file = nullptr;
    errno_t err  = fopen_s(&file, filename.c_str(), "wb");
    if (err != 0 || file == nullptr) return false;

    size_t written = fwrite(fileBuffer.data(), 1, fileBuffer.size(), file);
    fclose(file);

    return written == fileBuffer.size();
}

//----------------------------------------------------------------------------------------------------
void World::UpdateNeighborPointers(IntVec2 const& chunkCoords)
{
    Chunk* centerChunk = GetChunk(chunkCoords);
    if (centerChunk == nullptr) return;

    // Get neighbor coordinates
    IntVec2 northCoords = chunkCoords + IntVec2(0, 1);
    IntVec2 southCoords = chunkCoords + IntVec2(0, -1);
    IntVec2 eastCoords  = chunkCoords + IntVec2(1, 0);
    IntVec2 westCoords  = chunkCoords + IntVec2(-1, 0);

    // Get neighbor chunks (if they exist)
    Chunk* northChunk = GetChunk(northCoords);
    Chunk* southChunk = GetChunk(southCoords);
    Chunk* eastChunk  = GetChunk(eastCoords);
    Chunk* westChunk  = GetChunk(westCoords);

    // Set neighbor pointers on center chunk
    centerChunk->SetNeighborChunks(northChunk, southChunk, eastChunk, westChunk);

    // Update neighbor chunks to point back to center chunk
    if (northChunk != nullptr) northChunk->SetNeighborChunks(northChunk->GetNorthNeighbor(), centerChunk, northChunk->GetEastNeighbor(), northChunk->GetWestNeighbor());
    if (southChunk != nullptr) southChunk->SetNeighborChunks(centerChunk, southChunk->GetSouthNeighbor(), southChunk->GetEastNeighbor(), southChunk->GetWestNeighbor());
    if (eastChunk != nullptr) eastChunk->SetNeighborChunks(eastChunk->GetNorthNeighbor(), eastChunk->GetSouthNeighbor(), eastChunk->GetEastNeighbor(), centerChunk);
    if (westChunk != nullptr) westChunk->SetNeighborChunks(westChunk->GetNorthNeighbor(), westChunk->GetSouthNeighbor(), centerChunk, westChunk->GetWestNeighbor());
}

//----------------------------------------------------------------------------------------------------
void World::ClearNeighborReferences(IntVec2 const& chunkCoords)
{
    // Clear references to this chunk from its neighbors
    IntVec2 northCoords = chunkCoords + IntVec2(0, 1);
    IntVec2 southCoords = chunkCoords + IntVec2(0, -1);
    IntVec2 eastCoords  = chunkCoords + IntVec2(1, 0);
    IntVec2 westCoords  = chunkCoords + IntVec2(-1, 0);

    Chunk* northChunk = GetChunk(northCoords);
    Chunk* southChunk = GetChunk(southCoords);
    Chunk* eastChunk  = GetChunk(eastCoords);
    Chunk* westChunk  = GetChunk(westCoords);

    if (northChunk != nullptr) northChunk->SetNeighborChunks(northChunk->GetNorthNeighbor(), nullptr, northChunk->GetEastNeighbor(), northChunk->GetWestNeighbor());
    if (southChunk != nullptr) southChunk->SetNeighborChunks(nullptr, southChunk->GetSouthNeighbor(), southChunk->GetEastNeighbor(), southChunk->GetWestNeighbor());
    if (eastChunk != nullptr) eastChunk->SetNeighborChunks(eastChunk->GetNorthNeighbor(), eastChunk->GetSouthNeighbor(), eastChunk->GetEastNeighbor(), nullptr);
    if (westChunk != nullptr) westChunk->SetNeighborChunks(westChunk->GetNorthNeighbor(), westChunk->GetSouthNeighbor(), nullptr, westChunk->GetWestNeighbor());
}

//----------------------------------------------------------------------------------------------------
// Digging and Placing System
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
IntVec3 World::FindHighestNonAirBlockAtOrBelow(Vec3 const& position) const
{
    // Start from camera position and search downward
    IntVec3 searchPos = IntVec3(static_cast<int>(floorf(position.x)),
                                static_cast<int>(floorf(position.y)),
                                static_cast<int>(floorf(position.z)));

    // Search downward from camera Z position to find highest non-air block
    for (int z = searchPos.z; z >= 0; z--)
    {
        IntVec3 testPos(searchPos.x, searchPos.y, z);
        uint8_t blockType = GetBlockTypeAtGlobalCoords(testPos);

        // If we found a non-air block, this is our target
        if (blockType != 0) // 0 = BLOCK_AIR
        {
            return testPos;
        }
    }

    // No non-air block found (all air down to bedrock)
    return IntVec3(INT_MAX, INT_MAX, INT_MAX); // Invalid position
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 10: Fast Voxel Raycast using Amanatides & Woo algorithm
//----------------------------------------------------------------------------------------------------
RaycastResult World::RaycastVoxel(Vec3 const& start, Vec3 const& direction, float maxDistance) const
{
    // Validate input: direction must be normalized
    Vec3 normalizedDir = direction.GetNormalized();

    // Convert world position to global block coordinates
    IntVec3 currentBlock = IntVec3(static_cast<int>(floorf(start.x)),
                                   static_cast<int>(floorf(start.y)),
                                   static_cast<int>(floorf(start.z)));

    // Get chunk and check if starting position is valid
    IntVec2 chunkCoords = Chunk::GetChunkCoords(IntVec3(currentBlock.x, currentBlock.y, currentBlock.z));
    Chunk*  chunk       = GetChunk(chunkCoords);

    // If starting position is outside loaded chunks, return no hit
    if (chunk == nullptr)
    {
        return RaycastResult(false);
    }

    // Check if we're starting inside a solid block (shouldn't happen in gameplay, but handle it)
    IntVec3 localCoords = Chunk::GlobalCoordsToLocalCoords(currentBlock);
    Block*  block       = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
    sBlockDefinition* def = (block != nullptr) ? sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex) : nullptr;
    if (def != nullptr && def->IsOpaque())
    {
        // Starting inside solid block - return immediate hit
        RaycastResult result(true, currentBlock, 0.0f);
        result.m_impactPosition = start;
        result.m_impactNormal   = -normalizedDir;
        return result;
    }

    // ================================================================================
    // Amanatides & Woo Algorithm - Fast Voxel Traversal
    // ================================================================================

    // Calculate step direction for each axis (-1, 0, or +1)
    int stepX = (normalizedDir.x < 0.0f) ? -1 : (normalizedDir.x > 0.0f) ? 1 : 0;
    int stepY = (normalizedDir.y < 0.0f) ? -1 : (normalizedDir.y > 0.0f) ? 1 : 0;
    int stepZ = (normalizedDir.z < 0.0f) ? -1 : (normalizedDir.z > 0.0f) ? 1 : 0;

    // Calculate tDelta: distance along ray to cross 1 voxel in each axis
    // Avoid division by zero for rays parallel to axis
    float tDeltaX = (stepX != 0) ? (1.0f / fabsf(normalizedDir.x)) : FLT_MAX;
    float tDeltaY = (stepY != 0) ? (1.0f / fabsf(normalizedDir.y)) : FLT_MAX;
    float tDeltaZ = (stepZ != 0) ? (1.0f / fabsf(normalizedDir.z)) : FLT_MAX;

    // Calculate tMax: distance along ray to next voxel boundary in each axis
    // This is the distance from start to the first grid boundary crossed
    float tMaxX, tMaxY, tMaxZ;

    // X axis
    if (stepX > 0)
        tMaxX = ((currentBlock.x + 1.0f) - start.x) / normalizedDir.x;
    else if (stepX < 0)
        tMaxX = (currentBlock.x - start.x) / normalizedDir.x;
    else
        tMaxX = FLT_MAX;

    // Y axis
    if (stepY > 0)
        tMaxY = ((currentBlock.y + 1.0f) - start.y) / normalizedDir.y;
    else if (stepY < 0)
        tMaxY = (currentBlock.y - start.y) / normalizedDir.y;
    else
        tMaxY = FLT_MAX;

    // Z axis
    if (stepZ > 0)
        tMaxZ = ((currentBlock.z + 1.0f) - start.z) / normalizedDir.z;
    else if (stepZ < 0)
        tMaxZ = (currentBlock.z - start.z) / normalizedDir.z;
    else
        tMaxZ = FLT_MAX;

    // ================================================================================
    // Main Traversal Loop
    // ================================================================================

    float distanceTraveled = 0.0f;
    Vec3  impactNormal     = Vec3(0.0f, 0.0f, 0.0f);

    while (distanceTraveled < maxDistance)
    {
        // Determine which axis boundary we cross first
        if (tMaxX < tMaxY && tMaxX < tMaxZ)
        {
            // Cross X boundary first
            if (tMaxX > maxDistance)
                break; // Exceeded max distance

            // Step to next voxel in X direction
            currentBlock.x += stepX;
            distanceTraveled = tMaxX;
            tMaxX += tDeltaX;
            impactNormal = Vec3(static_cast<float>(-stepX), 0.0f, 0.0f);
        }
        else if (tMaxY < tMaxZ)
        {
            // Cross Y boundary first
            if (tMaxY > maxDistance)
                break; // Exceeded max distance

            // Step to next voxel in Y direction
            currentBlock.y += stepY;
            distanceTraveled = tMaxY;
            tMaxY += tDeltaY;
            impactNormal = Vec3(0.0f, static_cast<float>(-stepY), 0.0f);
        }
        else
        {
            // Cross Z boundary first
            if (tMaxZ > maxDistance)
                break; // Exceeded max distance

            // Step to next voxel in Z direction
            currentBlock.z += stepZ;
            distanceTraveled = tMaxZ;
            tMaxZ += tDeltaZ;
            impactNormal = Vec3(0.0f, 0.0f, static_cast<float>(-stepZ));
        }

        // Check if we stepped outside world bounds
        if (currentBlock.z < 0 || currentBlock.z >= CHUNK_SIZE_Z)
        {
            return RaycastResult(false); // Hit world boundary
        }

        // Get the chunk containing the new block
        chunkCoords = Chunk::GetChunkCoords(IntVec3(currentBlock.x, currentBlock.y, currentBlock.z));
        chunk       = GetChunk(chunkCoords);

        // If chunk not loaded, treat as no hit
        if (chunk == nullptr)
        {
            return RaycastResult(false);
        }

        // Check if current block is solid
        localCoords = Chunk::GlobalCoordsToLocalCoords(currentBlock);
        block       = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
        def         = (block != nullptr) ? sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex) : nullptr;

        if (def != nullptr && def->IsOpaque())
        {
            // Hit a solid block!
            RaycastResult result(true, currentBlock, distanceTraveled);
            result.m_impactPosition = start + (normalizedDir * distanceTraveled);
            result.m_impactNormal   = impactNormal;
            return result;
        }
    }

    // No hit within max distance
    return RaycastResult(false);
}

//----------------------------------------------------------------------------------------------------
bool World::DigBlockAtCameraPosition(Vec3 const& cameraPos)
{
    // Find highest non-air block at or below camera position
    IntVec3 targetBlock = FindHighestNonAirBlockAtOrBelow(cameraPos);

    // Check if we found a valid block to dig
    if (targetBlock.x == INT_MAX || targetBlock.y == INT_MAX || targetBlock.z == INT_MAX)
    {
        return false; // No block to dig
    }

    // Convert the block to air (dig it)
    bool success = SetBlockAtGlobalCoords(targetBlock, 0); // 0 = BLOCK_AIR

    if (success)
    {
        DebuggerPrintf("Dug block at (%d,%d,%d)\n", targetBlock.x, targetBlock.y, targetBlock.z);
    }

    return success;
}

//----------------------------------------------------------------------------------------------------
bool World::PlaceBlockAtCameraPosition(Vec3 const&   cameraPos,
                                       uint8_t const blockType)
{
    // Find highest non-air block at or below camera position
    IntVec3 highestBlock = FindHighestNonAirBlockAtOrBelow(cameraPos);

    // Check if we found a valid foundation block
    if (highestBlock.x == INT_MAX || highestBlock.y == INT_MAX || highestBlock.z == INT_MAX)
    {
        return false; // No foundation block found
    }

    // Place block directly above the highest non-air block
    IntVec3 placePos = IntVec3(highestBlock.x, highestBlock.y, highestBlock.z + 1);

    // Check if the target position is valid (within world bounds)
    if (placePos.z >= CHUNK_SIZE_Z)
    {
        return false; // Would place block above world height limit
    }

    // Check if target position is already occupied by a non-air block
    uint8_t existingBlock = GetBlockTypeAtGlobalCoords(placePos);
    if (existingBlock != 0) // 0 = BLOCK_AIR
    {
        return false; // Position already occupied
    }

    // Place the new block
    bool success = SetBlockAtGlobalCoords(placePos, blockType);

    if (success)
    {
        DebuggerPrintf("Placed block type %d at (%d,%d,%d)\n", blockType, placePos.x, placePos.y, placePos.z);
    }

    return success;
}

//----------------------------------------------------------------------------------------------------
// Asynchronous job processing methods
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
void World::ProcessCompletedJobs()
{
    if (g_jobSystem == nullptr) return;

    // Retrieve all completed jobs from JobSystem ONCE
    std::vector<Job*> completedJobs = g_jobSystem->RetrieveAllCompletedJobs();

    // Process each completed job
    for (Job* completedJob : completedJobs)
    {
        ChunkGenerateJob* job      = nullptr;
        int               jobIndex = -1;

        // Find the corresponding ChunkGenerateJob in our tracking list
        {
            std::lock_guard<std::mutex> lock(m_jobListsMutex);
            for (int i = 0; i < (int)m_chunkGenerationJobs.size(); ++i)
            {
                if (m_chunkGenerationJobs[i] == completedJob)
                {
                    job      = m_chunkGenerationJobs[i];
                    jobIndex = i;
                    break;
                }
            }
        }

        // If not a generation job, check if it's a load job
        if (job == nullptr)
        {
            // BUGFIX: Store chunk pointer before releasing mutex to prevent deadlock
            // when calling SubmitChunkForGeneration() which also needs m_jobListsMutex
            Chunk*        chunkToGenerate = nullptr;
            ChunkLoadJob* loadJobToDelete = nullptr;

            {
                std::lock_guard<std::mutex> lock(m_jobListsMutex);
                for (int i = 0; i < (int)m_chunkLoadJobs.size(); ++i)
                {
                    if (m_chunkLoadJobs[i] == completedJob)
                    {
                        ChunkLoadJob* loadJob = m_chunkLoadJobs[i];
                        Chunk*        chunk   = loadJob->GetChunk();

                        if (chunk)
                        {
                            IntVec2 chunkCoords = chunk->GetChunkCoords();

                            if (loadJob->WasSuccessful() && chunk->GetState() == ChunkState::LOAD_COMPLETE)
                            {
                                chunk->SetState(ChunkState::COMPLETE);
                                chunk->SetDebugDraw(m_globalChunkDebugDraw); // Inherit global debug state

                                {
                                    std::lock_guard<std::mutex> nonActiveLock(m_nonActiveChunksMutex);
                                    m_nonActiveChunks.erase(chunk);
                                }

                                {
                                    std::lock_guard<std::mutex> activeLock(m_activeChunksMutex);
                                    m_activeChunks[chunkCoords] = chunk;
                                }

                                UpdateNeighborPointers(chunkCoords);

                                // Assignment 5 Phase 6: Trigger cross-chunk lighting propagation
                                chunk->OnActivate(this);  // CRITICAL: Re-enabled for loaded chunks

                                // NOTE: Do NOT call SetIsMeshDirty(true) here!
                                // The deferred mesh rebuild system will mark this chunk dirty
                                // AFTER lighting stabilizes via ProcessDirtyChunkMeshes()
                            }
                            else
                            {
                                {
                                    std::lock_guard<std::mutex> nonActiveLock(m_nonActiveChunksMutex);
                                    m_nonActiveChunks.erase(chunk);
                                }

                                chunk->SetState(ChunkState::ACTIVATING);
                                // Store chunk for generation AFTER releasing mutex
                                chunkToGenerate = chunk;
                            }
                        }

                        m_chunkLoadJobs.erase(m_chunkLoadJobs.begin() + i);
                        loadJobToDelete = loadJob;
                        job = reinterpret_cast<ChunkGenerateJob*>(1); // Mark as processed
                        break;
                    }
                }
            } // Release m_jobListsMutex here

            // BUGFIX: Call SubmitChunkForGeneration AFTER releasing mutex
            // to prevent deadlock (SubmitChunkForGeneration also locks m_jobListsMutex)
            if (chunkToGenerate != nullptr)
            {
                SubmitChunkForGeneration(chunkToGenerate);
            }

            // Clean up after mutex is released
            if (loadJobToDelete != nullptr)
            {
                delete loadJobToDelete;
            }
        }

        // If not a load job either, check if it's a mesh job
        if (job == nullptr)
        {
            std::lock_guard<std::mutex> lock(m_jobListsMutex);
            for (int i = 0; i < (int)m_chunkMeshJobs.size(); ++i)
            {
                if (m_chunkMeshJobs[i] == completedJob)
                {
                    ChunkMeshJob* meshJob = m_chunkMeshJobs[i];
                    Chunk*        chunk   = meshJob->GetChunk();

                    if (meshJob->WasSuccessful())
                    {
                        // Apply mesh data on main thread (CPU data only)
                        meshJob->ApplyMeshDataToChunk();

                        // Now perform DirectX operations on main thread
                        chunk->UpdateVertexBuffer();
                        chunk->SetMeshClean();
                    }

                    m_chunkMeshJobs.erase(m_chunkMeshJobs.begin() + i);
                    delete meshJob;
                    job = reinterpret_cast<ChunkGenerateJob*>(1); // Mark as processed
                    break;
                }
            }
        }

        // If not a mesh job either, check if it's a save job
        if (job == nullptr)
        {
            std::lock_guard<std::mutex> lock(m_jobListsMutex);
            for (int i = 0; i < (int)m_chunkSaveJobs.size(); ++i)
            {
                if (m_chunkSaveJobs[i] == completedJob)
                {
                    ChunkSaveJob* saveJob = m_chunkSaveJobs[i];
                    Chunk*        chunk   = saveJob->GetChunk();

                    if (chunk)
                    {
                        {
                            std::lock_guard<std::mutex> nonActiveLock(m_nonActiveChunksMutex);
                            m_nonActiveChunks.erase(chunk);
                        }

                        delete chunk;
                    }

                    m_chunkSaveJobs.erase(m_chunkSaveJobs.begin() + i);
                    delete saveJob;
                    job = reinterpret_cast<ChunkGenerateJob*>(1); // Mark as processed
                    break;
                }
            }
        }

        if (job == nullptr)
        {
            continue; // Not any of our job types
        }

        // If job is a real ChunkGenerateJob pointer (not marker), process it
        if (job != reinterpret_cast<ChunkGenerateJob*>(1))
        {
            Chunk* chunk = job->GetChunk();
            if (!chunk)
            {
                // Clean up invalid job
                {
                    std::lock_guard<std::mutex> lock(m_jobListsMutex);
                    m_chunkGenerationJobs.erase(m_chunkGenerationJobs.begin() + jobIndex);
                }
                delete job;
                continue;
            }

            IntVec2 chunkCoords = chunk->GetChunkCoords();

            // Verify the chunk is in the expected state
            if (chunk->GetState() == ChunkState::LIGHTING_INITIALIZING)
            {
                // Job completed successfully - handle lighting initialization on main thread
                // For now, just transition to COMPLETE state
                chunk->SetState(ChunkState::COMPLETE);
                chunk->SetDebugDraw(m_globalChunkDebugDraw); // Inherit global debug state

                // Remove from non-active chunks, add to active chunks
                {
                    std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
                    m_nonActiveChunks.erase(chunk);
                }

                {
                    std::lock_guard<std::mutex> lock(m_activeChunksMutex);
                    if (m_activeChunks.find(chunkCoords) == m_activeChunks.end())
                    {
                        m_activeChunks[chunkCoords] = chunk;
                    }
                }

                // Update neighbor pointers (needs active chunks mutex, but UpdateNeighborPointers handles it)
                UpdateNeighborPointers(chunkCoords);

                // Remove from queued chunks
                {
                    std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
                    m_queuedGenerateChunks.erase(chunkCoords);
                }

                // Assignment 5 Phase 6: Trigger cross-chunk lighting propagation FIRST
                // This queues edge blocks for lighting propagation
                chunk->OnActivate(this);

                // NOTE: Do NOT call SetIsMeshDirty(true) here!
                // The deferred mesh rebuild system will mark this chunk dirty
                // AFTER lighting stabilizes via ProcessDirtyChunkMeshes()
            }
            else
            {
                // Job completed but chunk is in unexpected state - handle error
                // This could happen if the job failed or chunk was modified externally
                // For now, we'll clean up and let the chunk be retried later

                // Remove from non-active chunks
                {
                    std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
                    m_nonActiveChunks.erase(chunk);
                }

                // Remove from queued chunks
                {
                    std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
                    m_queuedGenerateChunks.erase(chunkCoords);
                }

                // Reset chunk to ACTIVATING state so it can be retried
                chunk->SetState(ChunkState::ACTIVATING);
            }

            // Clean up the job
            {
                std::lock_guard<std::mutex> lock(m_jobListsMutex);
                m_chunkGenerationJobs.erase(m_chunkGenerationJobs.begin() + jobIndex);
            }
            delete job;
        } // End if (job is real ChunkGenerateJob)
    }
}

//----------------------------------------------------------------------------------------------------
void World::ProcessDirtyChunkMeshes()
{
    // CRITICAL FIX: Don't rebuild meshes while light propagation is in progress
    // This prevents "progressive brightening" bug where chunks appear dark and gradually brighten
    // as light propagates from neighboring chunks
    if (!m_dirtyLightQueue.empty())
    {
        return;  // Don't rebuild any meshes until light queue is empty
    }

    // Assignment 5 Phase 10 FIX: Lighting queue is now empty (lighting has stabilized)
    // Mark all chunks with changed lighting as mesh-dirty so they rebuild with correct lighting
    // This fixes the "inconsistent nighttime lighting" bug where some chunks stay bright
    {
        std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
        if (!m_chunksNeedingMeshRebuild.empty())
        {
            for (Chunk* chunk : m_chunksNeedingMeshRebuild)
            {
                if (chunk)
                {
                    chunk->SetIsMeshDirty(true);
                }
            }
            m_chunksNeedingMeshRebuild.clear();
        }
    }

    Vec3 cameraPos = GetCameraPosition();

    // Find up to 2 dirty chunks closest to camera
    std::vector<std::pair<float, Chunk*>> dirtyChunksWithDistance;

    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        for (auto const& chunkPair : m_activeChunks)
        {
            Chunk* chunk = chunkPair.second;
            if (chunk && chunk->GetIsMeshDirty() && chunk->GetState() == ChunkState::COMPLETE)
            {
                float distance = GetDistanceToChunkCenter(chunk->GetChunkCoords(), cameraPos);
                dirtyChunksWithDistance.emplace_back(distance, chunk);
            }
        }
    }

    // Sort by distance (closest first)
    std::sort(dirtyChunksWithDistance.begin(), dirtyChunksWithDistance.end());

    // Rebuild mesh for up to 2 closest dirty chunks
    int meshesRebuilt = 0;
    for (auto const& pair : dirtyChunksWithDistance)
    {
        if (meshesRebuilt >= 2)
        {
            break;
        }

        Chunk* chunk = pair.second;

        // Assignment 5 Phase 0 (Hidden Surface Removal): Only construct meshes for chunks with all 4 neighbors active
        // This prevents rendering incomplete chunk edges (blackness underwater/underground)
        // Reference: A5 specification line 97: "Only construct meshes for chunks with all 4 neighbors active"
        IntVec2 chunkCoords = chunk->GetChunkCoords();
        bool allNeighborsActive = true;
        {
            std::lock_guard<std::mutex> lock(m_activeChunksMutex);

            // Check all 4 horizontal neighbors (East, West, North, South)
            IntVec2 neighborOffsets[4] = {
                IntVec2(1, 0),   // East
                IntVec2(-1, 0),  // West
                IntVec2(0, 1),   // North
                IntVec2(0, -1)   // South
            };

            for (int i = 0; i < 4; ++i)
            {
                IntVec2 neighborCoords = chunkCoords + neighborOffsets[i];
                auto iter = m_activeChunks.find(neighborCoords);
                if (iter == m_activeChunks.end() || iter->second == nullptr ||
                    iter->second->GetState() != ChunkState::COMPLETE)
                {
                    allNeighborsActive = false;
                    break;
                }
            }
        }

        // Only submit chunk for mesh generation if all 4 neighbors are active
        if (allNeighborsActive)
        {
            // Submit mesh generation job instead of rebuilding on main thread
            SubmitChunkForMeshGeneration(chunk);
            ++meshesRebuilt;
        }
        // If neighbors not ready, leave chunk marked as dirty - it will be retried next frame
    }
}

//----------------------------------------------------------------------------------------------------
void World::SubmitChunkForGeneration(Chunk* chunk)
{
    if (!chunk)
    {
        return;
    }

    if (g_jobSystem == nullptr) return;

    IntVec2 chunkCoords = chunk->GetChunkCoords();

    // Check if we've reached the maximum number of pending generation jobs
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        if ((int)m_chunkGenerationJobs.size() >= MAX_PENDING_GENERATE_JOBS)
        {
            return; // Too many jobs in flight, try again next frame
        }
    }

    // Check if chunk is already queued for generation (with mutex protection)
    {
        std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
        if (m_queuedGenerateChunks.find(chunkCoords) != m_queuedGenerateChunks.end())
        {
            return; // Already queued
        }
    }

    // Set chunk state and submit job
    if (chunk->CompareAndSetState(ChunkState::ACTIVATING, ChunkState::TERRAIN_GENERATING))
    {
        ChunkGenerateJob* job = new ChunkGenerateJob(chunk);

        // Add chunk to non-active chunks (being processed by worker thread)
        {
            std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
            m_nonActiveChunks.insert(chunk);
        }

        // Add job to tracking lists
        {
            std::lock_guard<std::mutex> lock(m_jobListsMutex);
            m_chunkGenerationJobs.push_back(job);
        }

        // Mark as queued
        {
            std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
            m_queuedGenerateChunks.insert(chunkCoords);
        }

        g_jobSystem->SubmitJob(job);
    }
}

//----------------------------------------------------------------------------------------------------
void World::SubmitChunkForLoading(Chunk* chunk)
{
    if (!chunk)
    {
        return;
    }

    if (g_jobSystem == nullptr) return;

    // Check if we've reached the maximum number of pending load jobs
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        if ((int)m_chunkLoadJobs.size() >= MAX_PENDING_LOAD_JOBS)
        {
            return; // Too many load jobs in flight, try again next frame
        }
    }

    // Set chunk state and submit I/O job
    if (chunk->CompareAndSetState(ChunkState::ACTIVATING, ChunkState::LOADING))
    {
        ChunkLoadJob* job = new ChunkLoadJob(chunk);

        // Add chunk to non-active chunks (being processed by I/O worker thread)
        {
            std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
            m_nonActiveChunks.insert(chunk);
        }

        // Add job to tracking lists
        {
            std::lock_guard<std::mutex> lock(m_jobListsMutex);
            m_chunkLoadJobs.push_back(job);
        }

        g_jobSystem->SubmitJob(job);
    }
}

//----------------------------------------------------------------------------------------------------
void World::SubmitChunkForSaving(Chunk* chunk)
{
    if (!chunk)
    {
        return;
    }

    if (g_jobSystem == nullptr)
    {
        // Can't save asynchronously, delete chunk
        delete chunk;
        return;
    }

    // Check if we've reached the maximum number of pending save jobs
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        if ((int)m_chunkSaveJobs.size() >= MAX_PENDING_SAVE_JOBS)
        {
            // Too many save jobs in flight, save synchronously (fallback)
            chunk->SaveToDisk();
            delete chunk;
            return;
        }
    }

    // Set chunk state and submit I/O job
    chunk->SetState(ChunkState::SAVING);

    ChunkSaveJob* job = new ChunkSaveJob(chunk);

    // Add chunk to non-active chunks (being processed by I/O worker thread)
    {
        std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
        m_nonActiveChunks.insert(chunk);
    }

    // Add job to tracking lists
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        m_chunkSaveJobs.push_back(job);
    }

    g_jobSystem->SubmitJob(job);
    chunk->SetIsMeshDirty(false);
}

//----------------------------------------------------------------------------------------------------
void World::SubmitChunkForMeshGeneration(Chunk* chunk)
{
    if (!chunk)
    {
        return;
    }

    if (g_jobSystem == nullptr) return;

    // Check if we've reached the maximum number of pending mesh jobs
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        if ((int)m_chunkMeshJobs.size() >= MAX_PENDING_MESH_JOBS)
        {
            return; // Too many jobs in flight, try again next frame
        }
    }

    // Assignment 5 Phase 0 FIX: Pass World pointer for cross-chunk neighbor access
    // This enables BlockIterator::GetNeighbor() to properly navigate across chunk boundaries
    ChunkMeshJob* job = new ChunkMeshJob(chunk, this);

    // Add job to tracking lists
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        m_chunkMeshJobs.push_back(job);
    }

    // CRITICAL FIX: Mark chunk as clean IMMEDIATELY to prevent re-queuing while job is in flight
    // This fixes the oscillation bug where chunks alternate between bright/dark
    chunk->SetIsMeshDirty(false);

    g_jobSystem->SubmitJob(job);
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 4: Add block to dirty light queue for recalculation
//----------------------------------------------------------------------------------------------------
void World::AddToDirtyLightQueue(BlockIterator const& blockIter)
{
    if (!blockIter.IsValid()) return;

    // Assignment 5 Phase 7: O(1) duplicate detection using std::unordered_set
    // Check if block is already in the set - prevents infinite propagation loops
    if (m_dirtyLightSet.find(blockIter) != m_dirtyLightSet.end())
    {
        return;  // Already queued, skip
    }

    // Add to tracking set for O(1) duplicate detection
    m_dirtyLightSet.insert(blockIter);

    // Add to back of queue (FIFO order)
    m_dirtyLightQueue.push_back(blockIter);
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 4: Process dirty light queue with time budget
//----------------------------------------------------------------------------------------------------
void World::ProcessDirtyLighting(float const maxTimeSeconds)
{
    if (m_dirtyLightQueue.empty()) return;

    // Start timer
    double const startTime = Clock::GetSystemClock().GetTotalSeconds();

    // Process blocks until time budget exhausted or queue empty
    while (!m_dirtyLightQueue.empty())
    {
        // Check time budget
        double const currentTime = Clock::GetSystemClock().GetTotalSeconds();
        double const elapsedTime = currentTime - startTime;

        if (elapsedTime >= maxTimeSeconds)
        {
            break;  // Time budget exhausted
        }

        // Pop block from front of queue (FIFO order)
        BlockIterator blockIter = m_dirtyLightQueue.front();
        m_dirtyLightQueue.pop_front();

        // Assignment 5 Phase 7: Remove from tracking set (block is now being processed)
        m_dirtyLightSet.erase(blockIter);

        // Assignment 5 Phase 5: Recalculate lighting for this block
        RecalculateBlockLighting(blockIter);
    }

    // Assignment 5 Phase 7: Safety check - if queue is empty, set should also be empty
    if (m_dirtyLightQueue.empty() && !m_dirtyLightSet.empty())
    {
        m_dirtyLightSet.clear();
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 5: Influence map light propagation algorithm
//----------------------------------------------------------------------------------------------------
void World::RecalculateBlockLighting(BlockIterator const& blockIter)
{
    if (!blockIter.IsValid())
        return;

    Block* block = blockIter.GetBlock();
    if (!block)
        return;

    // Get block definition to check properties
    sBlockDefinition* blockDef = sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex);
    if (!blockDef)
        return;

    // Store old values to detect changes
    uint8_t const oldOutdoor = block->GetOutdoorLight();
    uint8_t const oldIndoor  = block->GetIndoorLight();

    //----------------------------------------------------------------------------------------------------
    // Calculate new outdoor light (skylight propagation)
    //----------------------------------------------------------------------------------------------------
    uint8_t newOutdoor = 0;

    if (block->IsSkyVisible())
    {
        // Direct skylight - always full brightness
        newOutdoor = 15;
    }
    else if (!blockDef->IsOpaque())
    {
        // A5 SPEC COMPLIANT: Only NON-OPAQUE blocks receive propagated outdoor light
        // Opaque blocks (including water/ice/leaves) ALWAYS get outdoor=0
        // This matches reference implementation World.cpp lines 798-803
        IntVec3 neighborOffsets[6] = {
            IntVec3(1, 0, 0),   // East
            IntVec3(-1, 0, 0),  // West
            IntVec3(0, 1, 0),   // North
            IntVec3(0, -1, 0),  // South
            IntVec3(0, 0, 1),   // Up
            IntVec3(0, 0, -1)   // Down
        };

        for (int i = 0; i < 6; i++)
        {
            BlockIterator neighborIter = blockIter.GetNeighbor(neighborOffsets[i]);
            if (neighborIter.IsValid())
            {
                Block* neighborBlock = neighborIter.GetBlock();
                if (neighborBlock)
                {
                    // A5 SPEC: Take light FROM neighbor if neighbor is NOT opaque OR neighbor is emissive
                    // This allows emissive blocks (glowstone) to provide light even though they're opaque
                    // Reference implementation World.cpp line 762
                    sBlockDefinition* neighborDef = sBlockDefinition::GetDefinitionByIndex(neighborBlock->m_typeIndex);
                    bool canProvideLight = neighborDef && (!neighborDef->IsOpaque() || neighborDef->IsEmissive());

                    if (canProvideLight)
                    {
                        uint8_t neighborLight = neighborBlock->GetOutdoorLight();
                        if (neighborLight > 0)
                        {
                            // Influence map: Take max of (neighbor - 1)
                            uint8_t propagatedLight = neighborLight - 1;
                            if (propagatedLight > newOutdoor)
                                newOutdoor = propagatedLight;
                        }
                    }
                }
            }
        }
    }
    // else: Opaque blocks (water/ice/leaves/stone/etc) ALWAYS get outdoor=0

    //----------------------------------------------------------------------------------------------------
    // Calculate new indoor light (emissive + propagation)
    //----------------------------------------------------------------------------------------------------
    // CRITICAL FIX: Start at 0, not existing value!
    // InitializeLighting() sets the INITIAL values correctly
    // But propagation must recalculate from scratch based on neighbors
    // Preserving old values prevents correct propagation
    uint8_t newIndoor = 0;

    if (blockDef->IsEmissive())
    {
        // Emissive blocks are light sources
        newIndoor = blockDef->GetEmissiveValue();
    }
    else if (!blockDef->IsOpaque())
    {
        // Transparent blocks: Find max neighbor indoor light - 1 (influence map)
        IntVec3 neighborOffsets[6] = {
            IntVec3(1, 0, 0),   // East
            IntVec3(-1, 0, 0),  // West
            IntVec3(0, 1, 0),   // North
            IntVec3(0, -1, 0),  // South
            IntVec3(0, 0, 1),   // Up
            IntVec3(0, 0, -1)   // Down
        };

        for (int i = 0; i < 6; i++)
        {
            BlockIterator neighborIter = blockIter.GetNeighbor(neighborOffsets[i]);
            if (neighborIter.IsValid())
            {
                Block* neighborBlock = neighborIter.GetBlock();
                if (neighborBlock)
                {
                    uint8_t neighborLight = neighborBlock->GetIndoorLight();
                    if (neighborLight > 0)
                    {
                        // Influence map: Take max of (neighbor - 1)
                        uint8_t propagatedLight = neighborLight - 1;
                        if (propagatedLight > newIndoor)
                            newIndoor = propagatedLight;
                    }
                }
            }
        }
    }
    // else: Opaque blocks stay at indoor light = 0

    //----------------------------------------------------------------------------------------------------
    // Update block lighting values
    //----------------------------------------------------------------------------------------------------
    block->SetOutdoorLight(newOutdoor);
    block->SetIndoorLight(newIndoor);

    //----------------------------------------------------------------------------------------------------
    // If lighting changed, propagate to neighbors by adding them to dirty queue
    //----------------------------------------------------------------------------------------------------
    if (newOutdoor != oldOutdoor || newIndoor != oldIndoor)
    {
        // TEMPORARY DEBUG: Log lighting changes to verify propagation algorithm
        // Phase 6 VALIDATION: ✅ PASSED - Commented out after successful validation
        // IntVec3 localCoords = blockIter.GetLocalCoords();
        // IntVec2 chunkCoords = blockIter.GetChunk() ? blockIter.GetChunk()->GetChunkCoords() : IntVec2(0, 0);
        // DebuggerPrintf("LIGHT: Chunk(%d,%d) Block(%d,%d,%d) outdoor %d->%d, indoor %d->%d\n",
        //                chunkCoords.x, chunkCoords.y,
        //                localCoords.x, localCoords.y, localCoords.z,
        //                oldOutdoor, newOutdoor, oldIndoor, newIndoor);

        // Assignment 5 Phase 10 FIX: Track chunks with changed lighting for DEFERRED mesh rebuild
        // DON'T mark mesh dirty immediately - causes progressive brightening during light propagation
        // Instead, add chunk to tracking set. ProcessDirtyChunkMeshes() will mark them dirty
        // AFTER the lighting queue empties (lighting has stabilized).
        // This prevents mesh rebuild starvation while preserving "wait for stable lighting" behavior.
        Chunk* chunk = blockIter.GetChunk();
        if (chunk)
        {
            std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
            m_chunksNeedingMeshRebuild.insert(chunk);
        }

        // Add only NON-OPAQUE neighbors to dirty queue for propagation
        // Assignment 5 Phase 7: Mesh rendering reads neighbor light values for each face
        // Opaque blocks stay at light=0 but their faces use neighbor light during rendering
        // This prevents infinite propagation loops through solid blocks
        IntVec3 neighborOffsets[6] = {
            IntVec3(1, 0, 0),   // East
            IntVec3(-1, 0, 0),  // West
            IntVec3(0, 1, 0),   // North
            IntVec3(0, -1, 0),  // South
            IntVec3(0, 0, 1),   // Up
            IntVec3(0, 0, -1)   // Down
        };

        for (int i = 0; i < 6; i++)
        {
            BlockIterator neighborIter = blockIter.GetNeighbor(neighborOffsets[i]);
            if (neighborIter.IsValid())
            {
                // Only add neighbor if it's non-opaque (can receive light)
                Block* neighborBlock = neighborIter.GetBlock();
                if (neighborBlock)
                {
                    sBlockDefinition* neighborDef = sBlockDefinition::GetDefinitionByIndex(neighborBlock->m_typeIndex);
                    if (neighborDef && !neighborDef->IsOpaque())
                    {
                        AddToDirtyLightQueue(neighborIter);
                    }
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 10: Mark chunk for DEFERRED mesh rebuild
// Called by OnActivate() to ensure chunks get mesh rebuild AFTER lighting stabilizes
// CRITICAL FIX: Don't mark mesh dirty immediately - wait for lighting to propagate first!
// The deferred system will mark chunks dirty only after lighting queue empties.
//----------------------------------------------------------------------------------------------------
void World::MarkChunkForMeshRebuild(Chunk* chunk)
{
    if (!chunk)
        return;

    // DEFERRED mesh rebuild - add to tracking set, DON'T mark dirty yet
    // ProcessDirtyChunkMeshes() will mark chunks dirty after lighting stabilizes
    // This ensures meshes are built with FINAL lighting values, not initial values
    std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
    m_chunksNeedingMeshRebuild.insert(chunk);

    // DO NOT call chunk->SetIsMeshDirty(true) here!
    // Let ProcessDirtyChunkMeshes() handle it after lighting queue empties
}
