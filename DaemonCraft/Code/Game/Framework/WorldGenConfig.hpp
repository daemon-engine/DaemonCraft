//----------------------------------------------------------------------------------------------------
// WorldGenConfig.hpp
// Configuration structure for all world generation parameters (Assignment 4: Phase 5B.4)
// Enables real-time parameter tuning via ImGui with XML persistence
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------
#include "Engine/Math/Curve1D.hpp"
#include <string>

//----------------------------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------------------------
namespace tinyxml2
{
	class XMLDocument;
	class XMLElement;
}

//----------------------------------------------------------------------------------------------------
// WorldGenConfig - Tunable world generation parameters
// Replaces hardcoded constexpr values in GameCommon.hpp with runtime-editable settings
// Persists to Run/Data/GameConfig.xml for configuration across app restarts
//----------------------------------------------------------------------------------------------------
struct WorldGenConfig
{
	//----------------------------------------------------------------------------------------------------
	// Biome Noise Parameters (6 layers)
	//----------------------------------------------------------------------------------------------------
	struct BiomeNoiseParams
	{
		// Temperature (T)
		float temperatureScale = 4096.f;
		int temperatureOctaves = 4;
		float temperaturePersistence = 0.5f;

		// Humidity (H)
		float humidityScale = 8192.f;
		int humidityOctaves = 4;
		float humidityPersistence = 0.5f;

		// Continentalness (C)
		float continentalnessScale = 400.f;
		int continentalnessOctaves = 4;
		float continentalnessPersistence = 0.5f;

		// Erosion (E)
		float erosionScale = 300.f;
		int erosionOctaves = 4;
		float erosionPersistence = 0.5f;

		// Weirdness (W) - used to calculate Peaks & Valleys
		float weirdnessScale = 350.f;
		int weirdnessOctaves = 3;
		float weirdnessPersistence = 0.5f;
	} biomeNoise;

	//----------------------------------------------------------------------------------------------------
	// 3D Density Terrain Parameters
	//----------------------------------------------------------------------------------------------------
	struct DensityParams
	{
		float densityNoiseScale = 200.f;
		int densityNoiseOctaves = 3;
		float densityBiasPerBlock = 0.10f;

		// Slides
		int topSlideStart = 100;
		int topSlideEnd = 120;
		int bottomSlideStart = 0;
		int bottomSlideEnd = 20;

		// Default terrain height (sea level)
		float defaultTerrainHeight = 80.f;
		float seaLevel = 80.f;
	} density;

	//----------------------------------------------------------------------------------------------------
	// Terrain Shaping Curves
	//----------------------------------------------------------------------------------------------------
	struct CurveParams
	{
		// Continentalness curve: Height offset based on ocean/inland distance
		float continentalnessHeightMin = -30.0f;
		float continentalnessHeightMax = 40.0f;

		// Erosion curve: Terrain wildness/scale multiplier
		float erosionScaleMin = 0.3f;
		float erosionScaleMax = 2.5f;

		// Peaks & Valleys curve: Additional height variation
		float pvHeightMin = -15.0f;
		float pvHeightMax = 25.0f;
	} curves;

	//----------------------------------------------------------------------------------------------------
	// Cave Parameters
	//----------------------------------------------------------------------------------------------------
	struct CaveParams
	{
		// Cheese caves (large caverns)
		float cheeseNoiseScale = 60.f;
		int cheeseNoiseOctaves = 2;
		float cheeseThreshold = 0.45f;
		int cheeseNoiseSeedOffset = 20;

		// Spaghetti caves (winding tunnels)
		float spaghettiNoiseScale = 30.f;
		int spaghettiNoiseOctaves = 3;
		float spaghettiThreshold = 0.65f;
		int spaghettiNoiseSeedOffset = 30;

		// Safety parameters
		int minCaveDepthFromSurface = 5;
		int minCaveHeightAboveLava = 3;
	} caves;

	//----------------------------------------------------------------------------------------------------
	// Tree Placement Parameters
	//----------------------------------------------------------------------------------------------------
	struct TreeParams
	{
		float treeNoiseScale = 10.f;
		int treeNoiseOctaves = 2;
		float treePlacementThreshold = 0.45f;  // Lowered from 0.8 to 0.45 for visibility
		int minTreeSpacing = 3;
	} trees;

	//----------------------------------------------------------------------------------------------------
	// Carver Parameters (Phase 5A)
	//----------------------------------------------------------------------------------------------------
	struct CarverParams
	{
		// Ravines
		float ravinePathNoiseScale = 800.f;
		int ravinePathNoiseOctaves = 3;
		float ravinePathThreshold = 0.85f;
		int ravineNoiseSeedOffset = 40;
		float ravineWidthNoiseScale = 50.f;
		int ravineWidthNoiseOctaves = 2;
		int ravineWidthMin = 3;
		int ravineWidthMax = 7;
		int ravineDepthMin = 40;
		int ravineDepthMax = 80;
		float ravineEdgeFalloff = 0.3f;

		// Rivers
		float riverPathNoiseScale = 600.f;
		int riverPathNoiseOctaves = 3;
		float riverPathThreshold = 0.70f;
		int riverNoiseSeedOffset = 50;
		float riverWidthNoiseScale = 40.f;
		int riverWidthNoiseOctaves = 2;
		int riverWidthMin = 5;
		int riverWidthMax = 12;
		int riverDepthMin = 3;
		int riverDepthMax = 8;
		float riverEdgeFalloff = 0.4f;
	} carvers;

	//----------------------------------------------------------------------------------------------------
	// Curve Objects (for terrain shaping)
	//----------------------------------------------------------------------------------------------------
	PiecewiseCurve1D continentalnessCurve;  // Maps continentalness to height offset
	PiecewiseCurve1D erosionCurve;          // Maps erosion to scale multiplier
	PiecewiseCurve1D peaksValleysCurve;     // Maps PV to height modifier

	//----------------------------------------------------------------------------------------------------
	// Methods
	//----------------------------------------------------------------------------------------------------
	WorldGenConfig();  // Constructor initializes with default values
	~WorldGenConfig() = default;

	// Create default instance with current hardcoded values from GameCommon.hpp
	static WorldGenConfig CreateDefault();

	// XML Serialization (saves to Run/Data/GameConfig.xml)
	void SaveToXML(std::string const& filepath) const;
	void LoadFromXML(std::string const& filepath);

	// Curve point serialization helpers (stores control points as arrays)
	// Uses forward-declared tinyxml2 types to avoid including XmlUtils.hpp in header
	static void SaveCurveToXML(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement& parent, std::string const& name, PiecewiseCurve1D const& curve);
	static PiecewiseCurve1D LoadCurveFromXML(tinyxml2::XMLElement& element);

	// Reset to default values
	void ResetToDefaults();

	// SimpleMiner-specific terrain shaping curve presets
	// Moved from Engine to maintain architectural purity (Engine should be game-agnostic)
	static PiecewiseCurve1D CreateDefaultContinentalnessCurve();
	static PiecewiseCurve1D CreateDefaultErosionCurve();
	static PiecewiseCurve1D CreateDefaultPeaksValleysCurve();

private:
	void InitializeDefaultCurves();
};

//----------------------------------------------------------------------------------------------------
// Global WorldGenConfig Instance
// Accessible from Chunk::GenerateTerrain() and ImGui debug window
//----------------------------------------------------------------------------------------------------
extern WorldGenConfig* g_worldGenConfig;
