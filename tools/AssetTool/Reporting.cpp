#include "Scene.h"
#include "AssetRuntime/GlTF/GlTFAssetProvider.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <span>

namespace ZiiNAN::AssetTool {
namespace {
std::string Number(double value) {
    if(!std::isfinite(value)) return "null";
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(9) << value; return out.str();
}
}
std::string PathUTF8(const std::filesystem::path& path)
{
    const auto s = path.generic_u8string();
    return std::string(s.begin(), s.end());
}
std::string EscapeJSON(const std::string& value)
{
    std::string result;
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') { result += '\\'; result += char(c); }
        else if (c < 32) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
        else result += char(c);
    }
    return result;
}
bool ReadBytes(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes, Report& report)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size == 0 || size > MaxFileBytes) {
        report.Error("input", PathUTF8(path), "File missing, empty, unreadable or exceeds 256 MiB."); return false;
    }
    std::ifstream stream(path, std::ios::binary);
    bytes.resize(static_cast<std::size_t>(size));
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        report.Error("input", PathUTF8(path), "Could not read the complete file."); return false;
    }
    return true;
}
std::string ToJSON(const Scene& scene, const Report& report)
{
    std::size_t vertices=0, indices=0, influenceMaximum=0, errors=0;
    for (const auto& mesh : scene.meshes) {
        vertices += mesh.vertices.size(); indices += mesh.indices.size();
        for (const auto& vertex : mesh.vertices) influenceMaximum=std::max(influenceMaximum,vertex.influences.size());
    }
    for (const auto& issue : report.issues) errors += issue.error;
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(9);
    out << "{\"version\":\"" << Version << "\",\"asset\":\"" << EscapeJSON(scene.name)
        << "\",\"type\":\"" << (!scene.skeleton.joints.empty() ? "SKINNED" : scene.animations.empty() ? "STATIC" : "ANIMATED_RIGID")
        << "\",\"meshes\":" << scene.meshes.size() << ",\"vertices\":" << vertices << ",\"indices\":" << indices
        << ",\"materials\":" << scene.materials.size() << ",\"textures\":" << scene.images.size()
        << ",\"bones\":" << scene.skeleton.joints.size() << ",\"animations\":" << scene.animations.size()
        << ",\"influenceMaximum\":" << influenceMaximum << ",\"metersPerSourceUnit\":" << Number(scene.sourceMetersPerUnit)
        << ",\"unitPolicy\":\"" << EscapeJSON(scene.unitPolicy) << "\",\"bounds\":";
    if (scene.bounds.valid) out << "{\"min\":[" << Number(scene.bounds.min[0]) << ',' << Number(scene.bounds.min[1]) << ',' << Number(scene.bounds.min[2])
        << "],\"max\":[" << Number(scene.bounds.max[0]) << ',' << Number(scene.bounds.max[1]) << ',' << Number(scene.bounds.max[2]) << "]}";
    else out << "null";
    const auto& s = scene.optimization;
    out << ",\"optimization\":{\"verticesBefore\":" << s.verticesBefore << ",\"verticesAfter\":" << s.verticesAfter
        << ",\"indicesBefore\":" << s.indicesBefore << ",\"indicesAfter\":" << s.indicesAfter
        << ",\"cacheMissRatioBefore\":" << Number(s.cacheMissRatioBefore) << ",\"cacheMissRatioAfter\":" << Number(s.cacheMissRatioAfter)
        << "},\"errors\":" << errors << ",\"warnings\":" << report.issues.size()-errors << ",\"issues\":[";
    bool first=true;
    for (const auto& issue : report.issues) {
        if (!first) out << ','; first=false;
        out << "{\"severity\":\"" << (issue.error ? "error" : "warning") << "\",\"code\":\"" << EscapeJSON(issue.code)
            << "\",\"context\":\"" << EscapeJSON(issue.context) << "\",\"message\":\"" << EscapeJSON(issue.message) << "\"}";
    }
    out << "]}"; return out.str();
}
bool ValidateGLB(const std::filesystem::path& path, Report& report, std::string* summary, bool json)
{
    std::vector<std::uint8_t> bytes;
    if (!ReadBytes(path, bytes, report)) return false;
    auto loaded = AssetRuntime::GetGlTFAssetProvider().Load(PathUTF8(path),
        std::as_bytes(std::span<const std::uint8_t>(bytes)));
    if (!loaded) { report.Error("runtime-glb",PathUTF8(path),loaded.diagnostic); return false; }
    std::set<const AssetRuntime::EncodedImage*> decoded;
    for(std::size_t m=0;m<loaded.asset.ModelCount();++m) {
        for(const auto& material:loaded.asset.Model(m).Get()->materials) for(const auto& encoded:material.embeddedImages) {
            if(!encoded || !decoded.insert(encoded.get()).second) continue;
            Image image; image.name=encoded->id; image.mime=encoded->mimeType; image.bytes.resize(encoded->bytes.size());
            std::transform(encoded->bytes.begin(),encoded->bytes.end(),image.bytes.begin(),[](std::byte b){return std::to_integer<std::uint8_t>(b);});
            if(!ValidateImage(image,report)) return false;
        }
    }
    if (summary) {
        const auto model=loaded.asset.Model(0);
        const auto& value=*model.Get();
        std::ostringstream out;
        std::size_t vertices=0, indices=0, influences=0;
        Bounds bounds;
        for (const auto& mesh : value.meshes) {
            vertices+=mesh.vertexCount; indices+=mesh.indexCount;
            influences=std::max(influences,std::size_t(mesh.skin.influencesPerVertex));
            if(mesh.bounds.valid) {
                Vec3 low{mesh.bounds.min[0]/100,mesh.bounds.min[2]/100,-mesh.bounds.max[1]/100};
                Vec3 high{mesh.bounds.max[0]/100,mesh.bounds.max[2]/100,-mesh.bounds.min[1]/100};
                if(!bounds.valid) { bounds={low,high,true}; }
                else for(std::size_t c=0;c<3;++c) { bounds.min[c]=std::min(bounds.min[c],low[c]); bounds.max[c]=std::max(bounds.max[c],high[c]); }
            }
        }
        const auto type=!value.renderable ? "SKINNED" : loaded.asset.AnimationCount() ? "ANIMATED_RIGID" : "STATIC";
        const auto bones=!value.renderable && value.skeleton ? value.skeleton->bones.size() : 0;
        if(!json) {
            out << "Asset: " << PathUTF8(path.filename()) << "\nType: " << type << "\nMeshes: " << value.meshes.size()
                << "\nVertices: " << vertices << "\nIndices: " << indices << "\nMaterials: " << value.materials.size()
                << "\nTextures: " << decoded.size() << "\nBones: " << bones << "\nAnimations: " << loaded.asset.AnimationCount()
                << "\nInfluence maximum: " << influences;
            if(bounds.valid) out << "\nBounds (meters): [" << bounds.min[0] << ',' << bounds.min[1] << ',' << bounds.min[2]
                << "] to [" << bounds.max[0] << ',' << bounds.max[1] << ',' << bounds.max[2] << ']';
            out << "\nRuntime renderable: " << (value.renderable ? "yes" : "no (metadata only)") << "\nErrors: 0\nWarnings: 0";
            *summary=out.str(); return true;
        }
        out << "{\"version\":\"" << Version << "\",\"asset\":\"" << EscapeJSON(PathUTF8(path.filename()))
            << "\",\"valid\":true,\"type\":\"" << type
            << "\",\"meshes\":" << value.meshes.size() << ",\"vertices\":" << vertices << ",\"indices\":" << indices
            << ",\"materials\":" << value.materials.size() << ",\"textures\":" << decoded.size()
            << ",\"bones\":" << bones << ",\"animations\":" << loaded.asset.AnimationCount()
            << ",\"influenceMaximum\":" << influences << ",\"bounds\":";
        if(bounds.valid) out << "{\"min\":[" << bounds.min[0] << ',' << bounds.min[1] << ',' << bounds.min[2]
            << "],\"max\":[" << bounds.max[0] << ',' << bounds.max[1] << ',' << bounds.max[2] << "]}";
        else out << "null";
        out << ",\"runtimeRenderable\":" << (value.renderable ? "true" : "false") << ",\"errors\":0,\"warnings\":0}";
        *summary=out.str();
    }
    return true;
}
}
