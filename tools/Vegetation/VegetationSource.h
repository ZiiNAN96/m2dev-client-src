#pragma once
#include "AssetTool/Scene.h"
#include "Vegetation/VegetationRuntime.h"
namespace ZiiNAN::VegetationTool {
// SDK ownership ends at extraction. Geometry uses the established offline scene.
struct VegetationSource {
    AssetTool::Scene scene;
    Vegetation::Metadata metadata;
    std::string legacyKey;
    std::array<unsigned,3> lodCounts{};
    unsigned reconstructedNormals{},unusedNormals{};
};
bool Extract(const std::filesystem::path&,std::string_view legacyKey,VegetationSource&,std::string& error);
}
