#pragma once
#include "GraphicsSettings.h"
#include <filesystem>

namespace Graphics
{
// I/O entry points are used only by Game Settings; renderer code includes no file API.
LoadResult LoadGraphicsSettingsFile(const std::filesystem::path&, const GraphicsSettings& defaults, std::string& error);
bool SaveGraphicsSettingsFile(const std::filesystem::path&, const GraphicsSettings&, std::string& error);
}
