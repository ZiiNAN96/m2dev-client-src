#pragma once
// Only included in builds with renderer diagnostics. Fixed wind time for native
// A/B captures; never enabled by the graphics menu or normal client startup.
namespace Renderer { inline bool lightingProofFrozen=false; }
