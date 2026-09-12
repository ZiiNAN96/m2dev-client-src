#pragma once
#include "Renderer/TerrainRenderData.h"

// One synchronous read/upload per map. No change to the existing image resource cache.
Renderer::TerrainTexturePtr LoadTerrainTextureFile(const char* filename, Renderer::ITextureUploader& renderer);
Renderer::TerrainTexturePtr LoadTerrainTextureMemory(const void* data, size_t size, Renderer::ITextureUploader& renderer);
