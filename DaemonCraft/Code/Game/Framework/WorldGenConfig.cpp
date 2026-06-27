//----------------------------------------------------------------------------------------------------
// WorldGenConfig.cpp
// Implementation of WorldGenConfig with XML serialization (Assignment 4: Phase 5B.4)
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/WorldGenConfig.hpp"
//----------------------------------------------------------------------------------------------------
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Math/Curve1D.hpp"
#include <sstream>

//----------------------------------------------------------------------------------------------------
// Global WorldGenConfig Instance
//----------------------------------------------------------------------------------------------------
WorldGenConfig* g_worldGenConfig = nullptr;

//----------------------------------------------------------------------------------------------------
// Constructor - Initialize with default values
//----------------------------------------------------------------------------------------------------
WorldGenConfig::WorldGenConfig()
{
    InitializeDefaultCurves();
}

//----------------------------------------------------------------------------------------------------
// Static Factory Method - Create default instance
//----------------------------------------------------------------------------------------------------
WorldGenConfig WorldGenConfig::CreateDefault()
{
    WorldGenConfig config;
    // All default values are set in the struct declaration and InitializeDefaultCurves()
    return config;
}

//----------------------------------------------------------------------------------------------------
// Initialize Default Curves
//----------------------------------------------------------------------------------------------------
void WorldGenConfig::InitializeDefaultCurves()
{
    // Create default terrain shaping curves using SimpleMiner-specific presets
    // ARCHITECTURE FIX (2025-11-09): These functions were moved from Engine/Math/Curve1D.cpp
    // to maintain architectural purity - Engine should be game-agnostic, not contain
    // SimpleMiner-specific terrain generation logic
    continentalnessCurve = CreateDefaultContinentalnessCurve();
    erosionCurve         = CreateDefaultErosionCurve();
    peaksValleysCurve    = CreateDefaultPeaksValleysCurve();
}

//----------------------------------------------------------------------------------------------------
// Reset to Default Values
//----------------------------------------------------------------------------------------------------
void WorldGenConfig::ResetToDefaults()
{
    // Reset all parameters to struct defaults (declared in header)
    *this = WorldGenConfig();
}

//----------------------------------------------------------------------------------------------------
// XML Serialization - Save
//----------------------------------------------------------------------------------------------------
void WorldGenConfig::SaveToXML(std::string const& filepath) const
{
    // Create XML document
    XmlDocument doc;

    // Root element
    XmlElement* root = doc.NewElement("WorldGenConfig");
    doc.InsertFirstChild(root);

    // Biome Noise Parameters
    {
        XmlElement* biomeNoiseElement = doc.NewElement("BiomeNoise");
        root->InsertEndChild(biomeNoiseElement);

        // Temperature
        biomeNoiseElement->SetAttribute("temperatureScale", biomeNoise.temperatureScale);
        biomeNoiseElement->SetAttribute("temperatureOctaves", biomeNoise.temperatureOctaves);
        biomeNoiseElement->SetAttribute("temperaturePersistence", biomeNoise.temperaturePersistence);

        // Humidity
        biomeNoiseElement->SetAttribute("humidityScale", biomeNoise.humidityScale);
        biomeNoiseElement->SetAttribute("humidityOctaves", biomeNoise.humidityOctaves);
        biomeNoiseElement->SetAttribute("humidityPersistence", biomeNoise.humidityPersistence);

        // Continentalness
        biomeNoiseElement->SetAttribute("continentalnessScale", biomeNoise.continentalnessScale);
        biomeNoiseElement->SetAttribute("continentalnessOctaves", biomeNoise.continentalnessOctaves);
        biomeNoiseElement->SetAttribute("continentalnessPersistence", biomeNoise.continentalnessPersistence);

        // Erosion
        biomeNoiseElement->SetAttribute("erosionScale", biomeNoise.erosionScale);
        biomeNoiseElement->SetAttribute("erosionOctaves", biomeNoise.erosionOctaves);
        biomeNoiseElement->SetAttribute("erosionPersistence", biomeNoise.erosionPersistence);

        // Weirdness
        biomeNoiseElement->SetAttribute("weirdnessScale", biomeNoise.weirdnessScale);
        biomeNoiseElement->SetAttribute("weirdnessOctaves", biomeNoise.weirdnessOctaves);
        biomeNoiseElement->SetAttribute("weirdnessPersistence", biomeNoise.weirdnessPersistence);
    }

    // Density Parameters
    {
        XmlElement* densityElement = doc.NewElement("Density");
        root->InsertEndChild(densityElement);

        densityElement->SetAttribute("densityNoiseScale", density.densityNoiseScale);
        densityElement->SetAttribute("densityNoiseOctaves", density.densityNoiseOctaves);
        densityElement->SetAttribute("densityBiasPerBlock", density.densityBiasPerBlock);

        // Slides
        densityElement->SetAttribute("topSlideStart", density.topSlideStart);
        densityElement->SetAttribute("topSlideEnd", density.topSlideEnd);
        densityElement->SetAttribute("bottomSlideStart", density.bottomSlideStart);
        densityElement->SetAttribute("bottomSlideEnd", density.bottomSlideEnd);

        // Heights
        densityElement->SetAttribute("defaultTerrainHeight", density.defaultTerrainHeight);
        densityElement->SetAttribute("seaLevel", density.seaLevel);
    }

    // Curve Parameters
    {
        XmlElement* curvesElement = doc.NewElement("Curves");
        root->InsertEndChild(curvesElement);

        // Continentalness
        curvesElement->SetAttribute("continentalnessHeightMin", curves.continentalnessHeightMin);
        curvesElement->SetAttribute("continentalnessHeightMax", curves.continentalnessHeightMax);

        // Erosion
        curvesElement->SetAttribute("erosionScaleMin", curves.erosionScaleMin);
        curvesElement->SetAttribute("erosionScaleMax", curves.erosionScaleMax);

        // Peaks & Valleys
        curvesElement->SetAttribute("pvHeightMin", curves.pvHeightMin);
        curvesElement->SetAttribute("pvHeightMax", curves.pvHeightMax);
    }

    // Save curves with control points
    SaveCurveToXML(doc, *root, "ContinentalnessCurve", continentalnessCurve);
    SaveCurveToXML(doc, *root, "ErosionCurve", erosionCurve);
    SaveCurveToXML(doc, *root, "PeaksValleysCurve", peaksValleysCurve);

    // Cave Parameters
    {
        XmlElement* cavesElement = doc.NewElement("Caves");
        root->InsertEndChild(cavesElement);

        // Cheese
        cavesElement->SetAttribute("cheeseNoiseScale", caves.cheeseNoiseScale);
        cavesElement->SetAttribute("cheeseNoiseOctaves", caves.cheeseNoiseOctaves);
        cavesElement->SetAttribute("cheeseThreshold", caves.cheeseThreshold);
        cavesElement->SetAttribute("cheeseNoiseSeedOffset", caves.cheeseNoiseSeedOffset);

        // Spaghetti
        cavesElement->SetAttribute("spaghettiNoiseScale", caves.spaghettiNoiseScale);
        cavesElement->SetAttribute("spaghettiNoiseOctaves", caves.spaghettiNoiseOctaves);
        cavesElement->SetAttribute("spaghettiThreshold", caves.spaghettiThreshold);
        cavesElement->SetAttribute("spaghettiNoiseSeedOffset", caves.spaghettiNoiseSeedOffset);

        // Safety
        cavesElement->SetAttribute("minCaveDepthFromSurface", caves.minCaveDepthFromSurface);
        cavesElement->SetAttribute("minCaveHeightAboveLava", caves.minCaveHeightAboveLava);
    }

    // Tree Parameters
    {
        XmlElement* treesElement = doc.NewElement("Trees");
        root->InsertEndChild(treesElement);

        treesElement->SetAttribute("treeNoiseScale", trees.treeNoiseScale);
        treesElement->SetAttribute("treeNoiseOctaves", trees.treeNoiseOctaves);
        treesElement->SetAttribute("treePlacementThreshold", trees.treePlacementThreshold);
        treesElement->SetAttribute("minTreeSpacing", trees.minTreeSpacing);
    }

    // Carver Parameters
    {
        XmlElement* carversElement = doc.NewElement("Carvers");
        root->InsertEndChild(carversElement);

        // Ravines
        carversElement->SetAttribute("ravinePathNoiseScale", carvers.ravinePathNoiseScale);
        carversElement->SetAttribute("ravinePathNoiseOctaves", carvers.ravinePathNoiseOctaves);
        carversElement->SetAttribute("ravinePathThreshold", carvers.ravinePathThreshold);
        carversElement->SetAttribute("ravineNoiseSeedOffset", carvers.ravineNoiseSeedOffset);
        carversElement->SetAttribute("ravineWidthNoiseScale", carvers.ravineWidthNoiseScale);
        carversElement->SetAttribute("ravineWidthNoiseOctaves", carvers.ravineWidthNoiseOctaves);
        carversElement->SetAttribute("ravineWidthMin", carvers.ravineWidthMin);
        carversElement->SetAttribute("ravineWidthMax", carvers.ravineWidthMax);
        carversElement->SetAttribute("ravineDepthMin", carvers.ravineDepthMin);
        carversElement->SetAttribute("ravineDepthMax", carvers.ravineDepthMax);
        carversElement->SetAttribute("ravineEdgeFalloff", carvers.ravineEdgeFalloff);

        // Rivers
        carversElement->SetAttribute("riverPathNoiseScale", carvers.riverPathNoiseScale);
        carversElement->SetAttribute("riverPathNoiseOctaves", carvers.riverPathNoiseOctaves);
        carversElement->SetAttribute("riverPathThreshold", carvers.riverPathThreshold);
        carversElement->SetAttribute("riverNoiseSeedOffset", carvers.riverNoiseSeedOffset);
        carversElement->SetAttribute("riverWidthNoiseScale", carvers.riverWidthNoiseScale);
        carversElement->SetAttribute("riverWidthNoiseOctaves", carvers.riverWidthNoiseOctaves);
        carversElement->SetAttribute("riverWidthMin", carvers.riverWidthMin);
        carversElement->SetAttribute("riverWidthMax", carvers.riverWidthMax);
        carversElement->SetAttribute("riverDepthMin", carvers.riverDepthMin);
        carversElement->SetAttribute("riverDepthMax", carvers.riverDepthMax);
        carversElement->SetAttribute("riverEdgeFalloff", carvers.riverEdgeFalloff);
    }

    // Save to file
    doc.SaveFile(filepath.c_str());
}

//----------------------------------------------------------------------------------------------------
// XML Serialization - Load
//----------------------------------------------------------------------------------------------------
void WorldGenConfig::LoadFromXML(std::string const& filepath)
{
    XmlDocument doc;
    tinyxml2::XMLError result = doc.LoadFile(filepath.c_str());

    // If file doesn't exist or is invalid, keep current defaults
    if (result != tinyxml2::XML_SUCCESS)
    {
        return;
    }

    XmlElement* root = doc.FirstChildElement("WorldGenConfig");
    if (!root)
    {
        return;
    }

    // Load Biome Noise Parameters
    if (XmlElement* biomeNoiseElement = root->FirstChildElement("BiomeNoise"))
    {
        biomeNoise.temperatureScale = ParseXmlAttribute(*biomeNoiseElement, "temperatureScale", biomeNoise.temperatureScale);
        biomeNoise.temperatureOctaves = ParseXmlAttribute(*biomeNoiseElement, "temperatureOctaves", biomeNoise.temperatureOctaves);
        biomeNoise.temperaturePersistence = ParseXmlAttribute(*biomeNoiseElement, "temperaturePersistence", biomeNoise.temperaturePersistence);

        biomeNoise.humidityScale = ParseXmlAttribute(*biomeNoiseElement, "humidityScale", biomeNoise.humidityScale);
        biomeNoise.humidityOctaves = ParseXmlAttribute(*biomeNoiseElement, "humidityOctaves", biomeNoise.humidityOctaves);
        biomeNoise.humidityPersistence = ParseXmlAttribute(*biomeNoiseElement, "humidityPersistence", biomeNoise.humidityPersistence);

        biomeNoise.continentalnessScale = ParseXmlAttribute(*biomeNoiseElement, "continentalnessScale", biomeNoise.continentalnessScale);
        biomeNoise.continentalnessOctaves = ParseXmlAttribute(*biomeNoiseElement, "continentalnessOctaves", biomeNoise.continentalnessOctaves);
        biomeNoise.continentalnessPersistence = ParseXmlAttribute(*biomeNoiseElement, "continentalnessPersistence", biomeNoise.continentalnessPersistence);

        biomeNoise.erosionScale = ParseXmlAttribute(*biomeNoiseElement, "erosionScale", biomeNoise.erosionScale);
        biomeNoise.erosionOctaves = ParseXmlAttribute(*biomeNoiseElement, "erosionOctaves", biomeNoise.erosionOctaves);
        biomeNoise.erosionPersistence = ParseXmlAttribute(*biomeNoiseElement, "erosionPersistence", biomeNoise.erosionPersistence);

        biomeNoise.weirdnessScale = ParseXmlAttribute(*biomeNoiseElement, "weirdnessScale", biomeNoise.weirdnessScale);
        biomeNoise.weirdnessOctaves = ParseXmlAttribute(*biomeNoiseElement, "weirdnessOctaves", biomeNoise.weirdnessOctaves);
        biomeNoise.weirdnessPersistence = ParseXmlAttribute(*biomeNoiseElement, "weirdnessPersistence", biomeNoise.weirdnessPersistence);
    }

    // Load Density Parameters
    if (XmlElement* densityElement = root->FirstChildElement("Density"))
    {
        density.densityNoiseScale = ParseXmlAttribute(*densityElement, "densityNoiseScale", density.densityNoiseScale);
        density.densityNoiseOctaves = ParseXmlAttribute(*densityElement, "densityNoiseOctaves", density.densityNoiseOctaves);
        density.densityBiasPerBlock = ParseXmlAttribute(*densityElement, "densityBiasPerBlock", density.densityBiasPerBlock);

        density.topSlideStart = ParseXmlAttribute(*densityElement, "topSlideStart", density.topSlideStart);
        density.topSlideEnd = ParseXmlAttribute(*densityElement, "topSlideEnd", density.topSlideEnd);
        density.bottomSlideStart = ParseXmlAttribute(*densityElement, "bottomSlideStart", density.bottomSlideStart);
        density.bottomSlideEnd = ParseXmlAttribute(*densityElement, "bottomSlideEnd", density.bottomSlideEnd);

        density.defaultTerrainHeight = ParseXmlAttribute(*densityElement, "defaultTerrainHeight", density.defaultTerrainHeight);
        density.seaLevel = ParseXmlAttribute(*densityElement, "seaLevel", density.seaLevel);
    }

    // Load Curve Parameters
    if (XmlElement* curvesElement = root->FirstChildElement("Curves"))
    {
        curves.continentalnessHeightMin = ParseXmlAttribute(*curvesElement, "continentalnessHeightMin", curves.continentalnessHeightMin);
        curves.continentalnessHeightMax = ParseXmlAttribute(*curvesElement, "continentalnessHeightMax", curves.continentalnessHeightMax);

        curves.erosionScaleMin = ParseXmlAttribute(*curvesElement, "erosionScaleMin", curves.erosionScaleMin);
        curves.erosionScaleMax = ParseXmlAttribute(*curvesElement, "erosionScaleMax", curves.erosionScaleMax);

        curves.pvHeightMin = ParseXmlAttribute(*curvesElement, "pvHeightMin", curves.pvHeightMin);
        curves.pvHeightMax = ParseXmlAttribute(*curvesElement, "pvHeightMax", curves.pvHeightMax);
    }

    // Load curve objects
    if (XmlElement* continentalnessCurveElement = root->FirstChildElement("ContinentalnessCurve"))
    {
        continentalnessCurve = LoadCurveFromXML(*continentalnessCurveElement);
    }
    if (XmlElement* erosionCurveElement = root->FirstChildElement("ErosionCurve"))
    {
        erosionCurve = LoadCurveFromXML(*erosionCurveElement);
    }
    if (XmlElement* peaksValleysCurveElement = root->FirstChildElement("PeaksValleysCurve"))
    {
        peaksValleysCurve = LoadCurveFromXML(*peaksValleysCurveElement);
    }

    // Load Cave Parameters
    if (XmlElement* cavesElement = root->FirstChildElement("Caves"))
    {
        caves.cheeseNoiseScale = ParseXmlAttribute(*cavesElement, "cheeseNoiseScale", caves.cheeseNoiseScale);
        caves.cheeseNoiseOctaves = ParseXmlAttribute(*cavesElement, "cheeseNoiseOctaves", caves.cheeseNoiseOctaves);
        caves.cheeseThreshold = ParseXmlAttribute(*cavesElement, "cheeseThreshold", caves.cheeseThreshold);
        caves.cheeseNoiseSeedOffset = ParseXmlAttribute(*cavesElement, "cheeseNoiseSeedOffset", caves.cheeseNoiseSeedOffset);

        caves.spaghettiNoiseScale = ParseXmlAttribute(*cavesElement, "spaghettiNoiseScale", caves.spaghettiNoiseScale);
        caves.spaghettiNoiseOctaves = ParseXmlAttribute(*cavesElement, "spaghettiNoiseOctaves", caves.spaghettiNoiseOctaves);
        caves.spaghettiThreshold = ParseXmlAttribute(*cavesElement, "spaghettiThreshold", caves.spaghettiThreshold);
        caves.spaghettiNoiseSeedOffset = ParseXmlAttribute(*cavesElement, "spaghettiNoiseSeedOffset", caves.spaghettiNoiseSeedOffset);

        caves.minCaveDepthFromSurface = ParseXmlAttribute(*cavesElement, "minCaveDepthFromSurface", caves.minCaveDepthFromSurface);
        caves.minCaveHeightAboveLava = ParseXmlAttribute(*cavesElement, "minCaveHeightAboveLava", caves.minCaveHeightAboveLava);
    }

    // Load Tree Parameters
    if (XmlElement* treesElement = root->FirstChildElement("Trees"))
    {
        trees.treeNoiseScale = ParseXmlAttribute(*treesElement, "treeNoiseScale", trees.treeNoiseScale);
        trees.treeNoiseOctaves = ParseXmlAttribute(*treesElement, "treeNoiseOctaves", trees.treeNoiseOctaves);
        trees.treePlacementThreshold = ParseXmlAttribute(*treesElement, "treePlacementThreshold", trees.treePlacementThreshold);
        trees.minTreeSpacing = ParseXmlAttribute(*treesElement, "minTreeSpacing", trees.minTreeSpacing);
    }

    // Load Carver Parameters
    if (XmlElement* carversElement = root->FirstChildElement("Carvers"))
    {
        carvers.ravinePathNoiseScale = ParseXmlAttribute(*carversElement, "ravinePathNoiseScale", carvers.ravinePathNoiseScale);
        carvers.ravinePathNoiseOctaves = ParseXmlAttribute(*carversElement, "ravinePathNoiseOctaves", carvers.ravinePathNoiseOctaves);
        carvers.ravinePathThreshold = ParseXmlAttribute(*carversElement, "ravinePathThreshold", carvers.ravinePathThreshold);
        carvers.ravineNoiseSeedOffset = ParseXmlAttribute(*carversElement, "ravineNoiseSeedOffset", carvers.ravineNoiseSeedOffset);
        carvers.ravineWidthNoiseScale = ParseXmlAttribute(*carversElement, "ravineWidthNoiseScale", carvers.ravineWidthNoiseScale);
        carvers.ravineWidthNoiseOctaves = ParseXmlAttribute(*carversElement, "ravineWidthNoiseOctaves", carvers.ravineWidthNoiseOctaves);
        carvers.ravineWidthMin = ParseXmlAttribute(*carversElement, "ravineWidthMin", carvers.ravineWidthMin);
        carvers.ravineWidthMax = ParseXmlAttribute(*carversElement, "ravineWidthMax", carvers.ravineWidthMax);
        carvers.ravineDepthMin = ParseXmlAttribute(*carversElement, "ravineDepthMin", carvers.ravineDepthMin);
        carvers.ravineDepthMax = ParseXmlAttribute(*carversElement, "ravineDepthMax", carvers.ravineDepthMax);
        carvers.ravineEdgeFalloff = ParseXmlAttribute(*carversElement, "ravineEdgeFalloff", carvers.ravineEdgeFalloff);

        carvers.riverPathNoiseScale = ParseXmlAttribute(*carversElement, "riverPathNoiseScale", carvers.riverPathNoiseScale);
        carvers.riverPathNoiseOctaves = ParseXmlAttribute(*carversElement, "riverPathNoiseOctaves", carvers.riverPathNoiseOctaves);
        carvers.riverPathThreshold = ParseXmlAttribute(*carversElement, "riverPathThreshold", carvers.riverPathThreshold);
        carvers.riverNoiseSeedOffset = ParseXmlAttribute(*carversElement, "riverNoiseSeedOffset", carvers.riverNoiseSeedOffset);
        carvers.riverWidthNoiseScale = ParseXmlAttribute(*carversElement, "riverWidthNoiseScale", carvers.riverWidthNoiseScale);
        carvers.riverWidthNoiseOctaves = ParseXmlAttribute(*carversElement, "riverWidthNoiseOctaves", carvers.riverWidthNoiseOctaves);
        carvers.riverWidthMin = ParseXmlAttribute(*carversElement, "riverWidthMin", carvers.riverWidthMin);
        carvers.riverWidthMax = ParseXmlAttribute(*carversElement, "riverWidthMax", carvers.riverWidthMax);
        carvers.riverDepthMin = ParseXmlAttribute(*carversElement, "riverDepthMin", carvers.riverDepthMin);
        carvers.riverDepthMax = ParseXmlAttribute(*carversElement, "riverDepthMax", carvers.riverDepthMax);
        carvers.riverEdgeFalloff = ParseXmlAttribute(*carversElement, "riverEdgeFalloff", carvers.riverEdgeFalloff);
    }
}

//----------------------------------------------------------------------------------------------------
// Curve XML Serialization - Save
//----------------------------------------------------------------------------------------------------
void WorldGenConfig::SaveCurveToXML(XmlDocument& doc, XmlElement& parent, std::string const& name, PiecewiseCurve1D const& curve)
{
    XmlElement* curveElement = doc.NewElement(name.c_str());
    parent.InsertEndChild(curveElement);

    // Save all control points
    for (int i = 0; i < curve.GetNumPoints(); ++i)
    {
        PiecewiseCurve1D::ControlPoint point = curve.GetPoint(i);

        XmlElement* pointElement = doc.NewElement("Point");
        pointElement->SetAttribute("t", point.t);
        pointElement->SetAttribute("value", point.value);

        curveElement->InsertEndChild(pointElement);
    }
}

//----------------------------------------------------------------------------------------------------
// Curve XML Serialization - Load
//----------------------------------------------------------------------------------------------------
PiecewiseCurve1D WorldGenConfig::LoadCurveFromXML(XmlElement& element)
{
    std::vector<PiecewiseCurve1D::ControlPoint> points;

    // Load all control points
    XmlElement* pointElement = element.FirstChildElement("Point");
    while (pointElement)
    {
        float t     = ParseXmlAttribute(*pointElement, "t", 0.0f);
        float value = ParseXmlAttribute(*pointElement, "value", 0.0f);
        points.push_back({t, value});

        pointElement = pointElement->NextSiblingElement("Point");
    }

    // Return PiecewiseCurve1D with loaded points
    return PiecewiseCurve1D(points);
}

//----------------------------------------------------------------------------------------------------
// SimpleMiner-Specific Terrain Shaping Curve Presets
// ARCHITECTURE FIX (2025-11-09): Moved from Engine/Math/Curve1D.cpp
// Rationale: Engine layer should provide generic, reusable utilities for ANY game.
// These presets are specific to SimpleMiner's Minecraft-inspired voxel terrain generation
// (Assignment 4: World Generation) and should live in the game layer, not the engine layer.
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
PiecewiseCurve1D WorldGenConfig::CreateDefaultContinentalnessCurve()
{
	// Continentalness curve: Maps continentalness noise [-1, 1] to terrain height offset
	// Low continentalness (-1.0) = deep ocean (large negative offset, e.g., -30)
	// High continentalness (1.0) = far inland (positive offset, e.g., +20)
	// Based on Minecraft's continentalness spline behavior

	std::vector<PiecewiseCurve1D::ControlPoint> points = {
		{ -1.0f, -0.5f },   // Deep ocean: terrain drops significantly
		{ -0.5f, -0.3f },   // Ocean: still below sea level
		{ 0.0f, 0.0f },     // Coast: near sea level (neutral)
		{ 0.5f, 0.2f },     // Inland: slight elevation
		{ 1.0f, 0.4f }      // Far inland: higher elevation
	};

	return PiecewiseCurve1D(points);
}

//----------------------------------------------------------------------------------------------------
PiecewiseCurve1D WorldGenConfig::CreateDefaultErosionCurve()
{
	// Erosion curve: Maps erosion noise [-1, 1] to terrain scale multiplier
	// Low erosion (-1.0) = heavily eroded/flat terrain (reduce scale)
	// High erosion (1.0) = mountainous terrain (increase scale)
	// Based on Minecraft's erosion spline behavior

	std::vector<PiecewiseCurve1D::ControlPoint> points = {
		{ -1.0f, 1.4f },   // Neutral scale multiplier at center of [0.3, 2.5]
		{ -0.5f, 1.4f },   // Neutral scale multiplier at center of [0.3, 2.5]
		{ 0.0f, 1.4f },    // Neutral scale multiplier at center of [0.3, 2.5]
		{ 0.5f, 1.4f },    // Neutral scale multiplier at center of [0.3, 2.5]
		{ 1.0f, 1.4f }     // Neutral scale multiplier at center of [0.3, 2.5]
	};

	return PiecewiseCurve1D(points);
}

//----------------------------------------------------------------------------------------------------
PiecewiseCurve1D WorldGenConfig::CreateDefaultPeaksValleysCurve()
{
	// Peaks & Valleys curve: Maps PV noise [-1, 1] to terrain height modifier
	// PV = 1 - |( 3 * abs(W) ) - 2| where W is weirdness
	// Low PV (-1.0) = valleys (negative offset)
	// High PV (1.0) = peaks (positive offset)
	// Based on Minecraft's peaks & valleys spline behavior

	std::vector<PiecewiseCurve1D::ControlPoint> points = {
		{ -1.0f, -0.5f },   // Deep valleys: significant lowering
		{ -0.5f, -0.2f },   // Shallow valleys: slight lowering
		{ 0.0f, 0.0f },     // Neutral: no modifier
		{ 0.5f, 0.3f },     // Hills: moderate elevation
		{ 1.0f, 0.7f }      // Peaks: significant height increase
	};

	return PiecewiseCurve1D(points);
}
