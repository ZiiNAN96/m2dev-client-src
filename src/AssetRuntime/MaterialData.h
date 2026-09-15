#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace AssetRuntime
{
struct EncodedImage;
enum class MaterialModel { Legacy, PBRMetallicRoughness };
enum class AlphaMode { Opaque, Mask, Blend };
enum class ColorSpace { Linear, SRGB };
enum class MaterialMap : unsigned { BaseColor, Normal, MetallicRoughness, Occlusion, Emissive, Count };
inline constexpr unsigned MaterialMapCount = unsigned(MaterialMap::Count);
inline constexpr float MinimumRoughness = .045f;
inline std::atomic_uint64_t materialOverrideApplications{};
struct MaterialTexture
{
    std::string path;
    std::shared_ptr<const EncodedImage> image;
    // UV0 affine transform, row pairs: u'=a*u+b*v+c, v'=d*u+e*v+f.
    std::array<float,6> uvTransform{1,0,0,0,1,0};
    bool Present() const { return image || !path.empty(); }
};
constexpr ColorSpace MapColorSpace(MaterialMap slot)
{ return slot == MaterialMap::BaseColor || slot == MaterialMap::Emissive ? ColorSpace::SRGB : ColorSpace::Linear; }
struct PBRMaterialData
{
    std::array<float,4> baseColor{1,1,1,1};
    std::array<float,3> emissive{};
    float metallic{}, roughness{.8f}, normalScale{1}, occlusionStrength{1};
    std::array<MaterialTexture,MaterialMapCount> maps;
    AlphaMode alpha{AlphaMode::Opaque};
    float alphaCutoff{.5f};
    bool doubleSided{};
};
// Optional parallel vertex stream. Zero tangent.w selects guarded derivative TBN.
// The original PNT/skin streams and Classic UVs retain their exact byte layout.
struct MaterialVertex { std::array<float,4> tangent{}; std::array<float,2> uv{}; };
PBRMaterialData ValidateMaterial(PBRMaterialData);
struct MaterialAsset;
PBRMaterialData ResolveMaterial(const MaterialAsset&);
// An asset-local, versioned override addresses stable model/material indices and name.
struct MaterialOverride
{
    std::uint32_t model{}, material{};
    std::string name;
    PBRMaterialData data;
};
struct MaterialOverrideResult
{
    std::vector<MaterialOverride> entries;
    std::string error;
    explicit operator bool() const { return error.empty(); }
};
MaterialOverrideResult ParseMaterialOverrides(std::string_view);
std::string SerializeMaterialOverrides(const std::vector<MaterialOverride>&);
bool ApplyMaterialOverride(MaterialAsset&, std::uint32_t model, std::uint32_t material,
                           const std::vector<MaterialOverride>&);
}
