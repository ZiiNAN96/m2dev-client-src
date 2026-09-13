#pragma once
#include <d3d9.h>
#include <functional>
#include "Renderer/ResourceData.h"

// A native handle is borrowed only in Legacy. The neutral source has shared ownership.
struct TextureBinding
{
    IDirect3DBaseTexture9* native = nullptr;
    std::shared_ptr<Renderer::TextureResource> source;
    TextureBinding() = default;
    TextureBinding(IDirect3DBaseTexture9* value) : native(value) {}
    explicit TextureBinding(std::shared_ptr<Renderer::TextureResource> value) : source(std::move(value)) {}
    const void* Identity() const { return source ? static_cast<const void*>(source.get()) : native; }
    explicit operator bool() const { return Identity() != nullptr; }
    bool operator==(const TextureBinding& other) const { return Identity() == other.Identity(); }
    bool operator!=(const TextureBinding& other) const { return !(*this == other); }
};
struct TextureBindingHash
{
    size_t operator()(const TextureBinding& binding) const { return std::hash<const void*>{}(binding.Identity()); }
};
