#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// ZiiNAN: Offline asset pipeline. This owning model has no importer or runtime types.
namespace ZiiNAN::AssetTool {
using Vec2 = std::array<float, 2>;
using Vec3 = std::array<float, 3>;
using Vec4 = std::array<float, 4>;
using Matrix = std::array<float, 16>;
inline constexpr Matrix Identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
inline constexpr std::size_t MaxElements = 4'194'304;
inline constexpr std::size_t MaxObjects = 32'768;
inline constexpr std::size_t MaxFileBytes = 256u * 1024u * 1024u;
inline constexpr const char* Version = "E2-X.1";
struct Bounds { Vec3 min{}, max{}; bool valid{}; };
struct Issue { bool error{}; std::string code, context, message; };
struct Report {
    std::vector<Issue> issues;
    void Error(std::string code, std::string context, std::string message) { issues.push_back({true,std::move(code),std::move(context),std::move(message)}); }
    void Warn(std::string code, std::string context, std::string message) { issues.push_back({false,std::move(code),std::move(context),std::move(message)}); }
    bool Ok() const { for (const auto& i : issues) if (i.error) return false; return true; }
};
struct Options {
    bool generateNormals{true};
    bool generateTangents{};
    bool optimize{true};
    double sourceMetersPerUnit{}; // 0: metadata, or documented OBJ/PLY/STL meter default.
    double ticksPerSecond{}; // 0: require a valid source timebase for animation.
    std::vector<float> lodRatios; // static-only preparation, separate GLBs; never replaces LOD0.
};
struct Influence { std::uint32_t joint{}; float weight{}; };
struct Vertex {
    Vec3 position{}, normal{};
    Vec2 uv{};
    Vec4 tangent{};
    std::vector<Influence> influences;
    Vec4 color{1,1,1,1};Vec2 uv1{};Vec3 pivot{};float flexibility{};Vec3 cardPitchCos{},cardPitchSin{};
};
struct Mesh {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t material{};
    bool hasNormals{}, hasUV{}, hasTangents{}, skinned{};
    bool hasVertexExtras{};
    Bounds bounds;
};
enum class AlphaMode { Opaque, Mask, Blend };
struct Material {
    std::string name;
    Vec4 baseColor{1,1,1,1};
    int baseTexture{-1};
    AlphaMode alpha{AlphaMode::Opaque};
    float alphaCutoff{0.5f};
    bool doubleSided{};
};
struct Image { std::string name, mime; std::vector<std::uint8_t> bytes; std::uint32_t width{}, height{}; std::string packPath; };
struct Node {
    std::string name;
    int parent{-1};
    Matrix transform{Identity};
    bool useTRS{};
    Vec3 translation{}, scale{1,1,1};
    Vec4 rotation{0,0,0,1};
    std::vector<std::uint32_t> meshes;
};
struct Joint { std::string name; std::uint32_t node{}; Matrix inverseBind{Identity}; };
struct Skeleton { std::vector<Joint> joints; int root{-1}; };
enum class AnimationPath { Translation, Rotation, Scale };
struct Channel { std::uint32_t node{}; AnimationPath path{}; std::vector<float> times; std::vector<Vec4> values; };
struct Animation { std::string name; float duration{}; std::vector<Channel> channels; };
struct OptimizationStats {
    std::size_t verticesBefore{}, verticesAfter{}, indicesBefore{}, indicesAfter{};
    double cacheMissRatioBefore{}, cacheMissRatioAfter{};
};
struct Scene {
    std::string name, sourceFormat, unitPolicy;
    double sourceMetersPerUnit{1};
    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Image> images;
    Skeleton skeleton;
    std::vector<Animation> animations;
    Bounds bounds;
    OptimizationStats optimization;
};
bool IsSupportedSource(const std::filesystem::path& path);
bool Import(const std::filesystem::path& path, const Options& options, Scene& scene, Report& report);
bool ValidateImage(Image& image, Report& report);
bool Validate(Scene& scene, Report& report);
bool Process(Scene& scene, const Options& options, Report& report);
bool MakeStaticLOD(const Scene& source, float ratio, Scene& lod, Report& report);
bool WriteGLB(const Scene& scene, const std::filesystem::path& path, Report& report);
bool ValidateGLB(const std::filesystem::path& path, Report& report, std::string* summary = nullptr, bool json = true);
std::string ToJSON(const Scene& scene, const Report& report);
std::string EscapeJSON(const std::string& value);
std::string PathUTF8(const std::filesystem::path& path);
bool ReadBytes(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes, Report& report);
}
