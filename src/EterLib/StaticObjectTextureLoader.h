#pragma once
#include "Renderer/TerrainTextureData.h"

// Object-only extension of the existing image path. Terrain decoding is unchanged.
Renderer::TerrainTexturePtr LoadStaticObjectTextureMemory(const void*, size_t, Renderer::ITextureUploader&);
Renderer::TerrainTexturePtr LoadStaticObjectTextureFile(const char*, Renderer::ITextureUploader&);
