#pragma once
#include <cstddef>
#include <functional>
#include "Renderer/ResourceData.h"

// ZiiNAN: Removed final D3D9 compile-time dependency. Bindings own CPU sources.
struct TextureBinding
{

    std::shared_ptr<Renderer::TextureResource> source;
    TextureBinding() = default;
    TextureBinding(std::nullptr_t) {}
    explicit TextureBinding(std::shared_ptr<Renderer::TextureResource> value) : source(std::move(value)) {}
    const void* Identity() const { return source.get(); }
    explicit operator bool() const { return Identity() != nullptr; }
    bool operator==(const TextureBinding& other) const { return Identity() == other.Identity(); }
    bool operator!=(const TextureBinding& other) const { return !(*this == other); }
};
struct TextureBindingHash
{
    size_t operator()(const TextureBinding& binding) const { return std::hash<const void*>{}(binding.Identity()); }
};
