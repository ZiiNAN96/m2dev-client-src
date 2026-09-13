#pragma once
#include "Renderer/ResourceData.h"
std::shared_ptr<Renderer::TextureResource> DecodeTextureSource(const void* data, size_t size, const char* asset);
