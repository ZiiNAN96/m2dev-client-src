#include "Scene.h"

#include <assimp/Importer.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/config.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include <stb_image.h>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace ZiiNAN::AssetTool {
namespace {
constexpr std::size_t MaxImageBytes = 32u * 1024u * 1024u;
constexpr std::size_t MaxImagePixels = 16u * 1024u * 1024u;
class ImportLogStream final : public Assimp::LogStream {
public:
    ImportLogStream(Report& report, std::string context) : report_(report), context_(std::move(context)) {}
    void write(const char* message) override {
        if (!message || !*message) return;
        if (count_++ >= 1024) {
            if (count_ == 1025) report_.Error("assimp_diagnostic_limit", context_, "More than 1024 importer diagnostics; further messages are suppressed and this source is rejected.");
            return;
        }
        std::string text(message);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        report_.Warn("assimp_source", context_, std::move(text));
    }
private:
    Report& report_;
    std::string context_;
    std::size_t count_{};
};
class ScopedImportLogging {
public:
    ScopedImportLogging(Report& report, const std::string& context) : created_(Assimp::DefaultLogger::isNullLogger()) {
        try {
            if (created_) Assimp::DefaultLogger::create("", Assimp::Logger::NORMAL, 0);
            logger_ = Assimp::DefaultLogger::get();
            auto stream = std::make_unique<ImportLogStream>(report, context);
            if (!logger_->attachStream(stream.get(), Mask)) throw std::runtime_error("Cannot attach importer diagnostic stream.");
            stream_ = stream.release();
        } catch (...) {
            if (created_ && logger_ == Assimp::DefaultLogger::get()) Assimp::DefaultLogger::kill();
            throw;
        }
    }
    ~ScopedImportLogging() {
        if (logger_->detachStream(stream_, Mask)) delete stream_;
        if (created_) Assimp::DefaultLogger::kill();
    }
    ScopedImportLogging(const ScopedImportLogging&) = delete;
    ScopedImportLogging& operator=(const ScopedImportLogging&) = delete;
private:
    static constexpr unsigned Mask = Assimp::Logger::Warn | Assimp::Logger::Err;
    Assimp::Logger* logger_{};
    ImportLogStream* stream_{};
    bool created_{};
};
std::string Lower(std::string value) {
    for (auto& c : value) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return value;
}
std::filesystem::path SourcePath(std::string text) {
    std::replace(text.begin(), text.end(), '\\', '/');
    return std::filesystem::u8path(text);
}
Matrix CopyMatrix(const aiMatrix4x4& m) {
    return {m.a1,m.b1,m.c1,m.d1,m.a2,m.b2,m.c2,m.d2,m.a3,m.b3,m.c3,m.d3,m.a4,m.b4,m.c4,m.d4};
}
bool Near(const Matrix& a, const Matrix& b) {
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!std::isfinite(a[i]) || !std::isfinite(b[i]) || std::abs(a[i]-b[i]) > 1e-5f * std::max({1.f,std::abs(a[i]),std::abs(b[i])})) return false;
    return true;
}

// ZiiNAN: Offline asset pipeline. Assimp file access uses bounded, Unicode-capable filesystem paths.
class MemoryStream final : public Assimp::IOStream {
public:
    explicit MemoryStream(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
    std::size_t Read(void* buffer, std::size_t size, std::size_t count) override {
        if (!buffer || !size) return 0;
        count = std::min(count, (bytes_.size() - position_) / size);
        if (count) std::memcpy(buffer, bytes_.data() + position_, count * size);
        position_ += count * size;
        return count;
    }
    std::size_t Write(const void*, std::size_t, std::size_t) override { return 0; }
    aiReturn Seek(std::size_t offset, aiOrigin origin) override {
        if (origin != aiOrigin_SET && origin != aiOrigin_CUR && origin != aiOrigin_END) return aiReturn_FAILURE;
        const std::size_t base = origin == aiOrigin_CUR ? position_ : (origin == aiOrigin_END ? bytes_.size() : 0);
        std::size_t next{};
        if (offset > std::numeric_limits<std::size_t>::max() / 2) {
            const std::size_t backwards = std::size_t(0) - offset;
            if (backwards > base) return aiReturn_FAILURE;
            next = base - backwards;
        } else {
            if (offset > bytes_.size() - base) return aiReturn_FAILURE;
            next = base + offset;
        }
        position_ = next;
        return aiReturn_SUCCESS;
    }
    std::size_t Tell() const override { return position_; }
    std::size_t FileSize() const override { return bytes_.size(); }
    void Flush() override {}
private:
    std::vector<std::uint8_t> bytes_;
    std::size_t position_{};
};
class SourceIO final : public Assimp::IOSystem {
public:
    explicit SourceIO(std::filesystem::path base, Report& report) : base_(std::move(base)), report_(report) {}
    bool Exists(const char* name) const override {
        std::error_code ec;
        return std::filesystem::is_regular_file(Resolve(name), ec);
    }
    char getOsSeparator() const override { return '/'; }
    Assimp::IOStream* Open(const char* name, const char* mode = "rb") override {
        if (!name || !mode || mode[0] != 'r' || std::strchr(mode, '+')) return nullptr;
        const auto path = Resolve(name);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            if (Lower(PathUTF8(path.extension())) == ".mtl")
                report_.Error("missing_material_library", PathUTF8(path.filename()), "OBJ material library is missing; refusing default-material substitution.");
            return nullptr;
        }
        std::vector<std::uint8_t> bytes;
        if (!ReadBytes(path, bytes, report_)) return nullptr;
        if (bytes.size() > MaxFileBytes - bytesRead_) {
            report_.Error("source_dependency_limit", PathUTF8(path.filename()), "Combined importer reads exceed the 256 MiB safety limit.");
            return nullptr;
        }
        bytesRead_ += bytes.size();
        return new MemoryStream(std::move(bytes));
    }
    void Close(Assimp::IOStream* stream) override { delete stream; }
    bool ComparePaths(const char* a, const char* b) const override { return Resolve(a).lexically_normal() == Resolve(b).lexically_normal(); }
private:
    std::filesystem::path Resolve(const char* name) const {
        auto path = SourcePath(name ? name : "");
        return path.is_absolute() ? path : base_ / path;
    }
    std::filesystem::path base_;
    Report& report_;
    std::size_t bytesRead_{};
};

bool DecodeImage(Image& image, bool& transparent, Report& report) {
    static constexpr std::uint8_t png[]{137,80,78,71,13,10,26,10};
    const bool isPNG = image.bytes.size() >= sizeof(png) && !std::memcmp(image.bytes.data(), png, sizeof(png));
    const bool isJPEG = image.bytes.size() >= 3 && image.bytes[0] == 0xff && image.bytes[1] == 0xd8 && image.bytes[2] == 0xff;
    if (!isPNG && !isJPEG) {
        report.Error("image_format", image.name, "Only PNG and JPEG image payloads are supported.");
        return false;
    }
    if (image.bytes.size() > MaxImageBytes) {
        report.Error("image_size", image.name, "Encoded image exceeds the E1-X 32 MiB limit.");
        return false;
    }
    int width{}, height{}, channels{};
    const auto length = static_cast<int>(image.bytes.size());
    if (!stbi_info_from_memory(image.bytes.data(), length, &width, &height, &channels) ||
        width <= 0 || height <= 0 || width > 8192 || height > 8192 || static_cast<std::size_t>(width) > MaxImagePixels / static_cast<std::size_t>(height)) {
        report.Error("image_dimensions", image.name, "Corrupt image header, dimension over 8192 or more than 16 million pixels (E1-X limits).");
        return false;
    }
    auto* pixels = stbi_load_from_memory(image.bytes.data(), length, &width, &height, &channels, 4);
    if (!pixels) {
        report.Error("image_decode", image.name, "PNG/JPEG decoder rejected the image payload.");
        return false;
    }
    transparent = false;
    const auto pixelsCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (std::size_t i = 0; i < pixelsCount; ++i) if (pixels[i * 4 + 3] != 255) { transparent = true; break; }
    stbi_image_free(pixels);
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.mime = isPNG ? "image/png" : "image/jpeg";
    return true;
}
void EncodePNG(void* context, void* bytes, int count) {
    auto& output = *static_cast<std::vector<std::uint8_t>*>(context);
    if (count < 0 || static_cast<std::size_t>(count) > MaxImageBytes - output.size()) throw std::runtime_error("Embedded PNG exceeds the E1-X 32 MiB limit.");
    const auto* begin = static_cast<const std::uint8_t*>(bytes);
    output.insert(output.end(), begin, begin + count);
}
bool LoadImage(const aiScene& source, const aiString& reference, const std::filesystem::path& directory,
               Image& image, bool& transparent, Report& report) {
    const std::string ref(reference.C_Str());
    image.name = PathUTF8(SourcePath(ref).filename());
    if (const aiTexture* embedded = source.GetEmbeddedTexture(reference.C_Str())) {
        if (!embedded->pcData || !embedded->mWidth) {
            report.Error("embedded_texture", image.name, "Embedded texture is empty.");
            return false;
        }
        if (embedded->mHeight == 0) {
            if (embedded->mWidth > MaxImageBytes) {
                report.Error("embedded_texture_size", image.name, "Embedded texture exceeds safety limit.");
                return false;
            }
            const auto* begin = reinterpret_cast<const std::uint8_t*>(embedded->pcData);
            image.bytes.assign(begin, begin + embedded->mWidth);
        } else {
            if (embedded->mWidth > 8192 || embedded->mHeight > 8192 || embedded->mWidth > MaxImagePixels / embedded->mHeight) {
                report.Error("embedded_texture_size", image.name, "Raw embedded texture exceeds dimension or decoded-byte limit.");
                return false;
            }
            const auto count = static_cast<std::size_t>(embedded->mWidth) * embedded->mHeight;
            std::vector<std::uint8_t> rgba(count * 4);
            for (std::size_t i = 0; i < count; ++i) {
                const auto& texel = embedded->pcData[i];
                rgba[i*4] = texel.r; rgba[i*4+1] = texel.g; rgba[i*4+2] = texel.b; rgba[i*4+3] = texel.a;
            }
            if (!stbi_write_png_to_func(EncodePNG, &image.bytes, static_cast<int>(embedded->mWidth), static_cast<int>(embedded->mHeight), 4, rgba.data(), static_cast<int>(embedded->mWidth * 4))) {
                report.Error("embedded_texture_encode", image.name, "Could not encode raw embedded image as PNG.");
                return false;
            }
            image.name = "embedded_" + image.name + ".png";
        }
    } else {
        auto path = SourcePath(ref);
        if (!path.is_absolute()) path = directory / path;
        if (!ReadBytes(path, image.bytes, report)) {
            report.Error("texture_path", image.name, "Texture paths resolve relative to source directory with exact spelling; verify case on case-sensitive filesystems.");
            return false;
        }
    }
    return DecodeImage(image, transparent, report);
}

bool Materials(const aiScene& source, const std::filesystem::path& directory, Scene& scene, Report& report) {
    std::unordered_map<std::string, std::pair<int,bool>> images;
    std::size_t imageBytes{};
    for (unsigned i = 0; i < source.mNumMaterials; ++i) {
        if (!source.mMaterials[i]) { report.Error("material_missing", std::to_string(i), "Null material."); return false; }
        const auto& input = *source.mMaterials[i];
        Material output;
        aiString name;
        if (input.Get(AI_MATKEY_NAME, name) == AI_SUCCESS) output.name = name.C_Str();
        if (output.name.empty()) output.name = "material_" + std::to_string(i);
        aiColor4D color(1,1,1,1);
        if (input.Get(AI_MATKEY_BASE_COLOR, color) != AI_SUCCESS) input.Get(AI_MATKEY_COLOR_DIFFUSE, color);
        output.baseColor = {color.r,color.g,color.b,color.a};
        float opacity = 1.f;
        input.Get(AI_MATKEY_OPACITY, opacity);
        output.baseColor[3] *= opacity;
        int twoSided{};
        input.Get(AI_MATKEY_TWOSIDED, twoSided);
        output.doubleSided = twoSided != 0;
        aiString alphaMode;
        const bool explicitAlpha = input.Get("$mat.gltf.alphaMode", 0, 0, alphaMode) == AI_SUCCESS;
        if (explicitAlpha) {
            const std::string alpha(alphaMode.C_Str());
            if (alpha == "MASK") output.alpha = AlphaMode::Mask;
            else if (alpha == "BLEND") output.alpha = AlphaMode::Blend;
            else if (alpha != "OPAQUE") report.Error("alpha_mode", output.name, "Unsupported authored alpha mode.");
        } else if (output.baseColor[3] < 1.f) output.alpha = AlphaMode::Blend;
        input.Get("$mat.gltf.alphaCutoff", 0, 0, output.alphaCutoff);
        const auto textureType = input.GetTextureCount(aiTextureType_BASE_COLOR) ? aiTextureType_BASE_COLOR : aiTextureType_DIFFUSE;
        const auto textureCount = input.GetTextureCount(textureType);
        if (textureCount > 1) report.Error("layered_base_texture", output.name, "Multiple base texture layers cannot be represented by one glTF base texture.");
        if (textureCount) {
            aiString reference;
            aiTextureMapping mapping = aiTextureMapping_UV;
            unsigned uvIndex{};
            ai_real blend = 1;
            aiTextureOp operation = aiTextureOp_Multiply;
            aiTextureMapMode wrap[2]{aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
            if (input.GetTexture(textureType, 0, &reference, &mapping, &uvIndex, &blend, &operation, wrap) != AI_SUCCESS) {
                report.Error("base_texture", output.name, "Cannot read base texture metadata.");
                return false;
            }
            if (mapping != aiTextureMapping_UV || uvIndex != 0 || blend != 1 || operation != aiTextureOp_Multiply)
                report.Error("texture_mapping", output.name, "E1-X requires a UV0 base texture with multiply operation and full texture weight.");
            if (wrap[0] != aiTextureMapMode_Wrap || wrap[1] != aiTextureMapMode_Wrap)
                report.Error("texture_sampler", output.name, "E1-X supports repeat addressing only; clamp/mirror/decal are rejected.");
            const std::string key(reference.C_Str());
            auto found = images.find(key);
            if (found == images.end()) {
                Image image;
                bool transparent{};
                if (!LoadImage(source, reference, directory, image, transparent, report)) return false;
                if (image.bytes.size() > MaxFileBytes - imageBytes) { report.Error("images_size", output.name, "Combined embedded image payloads exceed 256 MiB."); return false; }
                imageBytes += image.bytes.size();
                const auto index = static_cast<int>(scene.images.size());
                scene.images.push_back(std::move(image));
                found = images.emplace(key, std::make_pair(index, transparent)).first;
            }
            output.baseTexture = found->second.first;
            if (!explicitAlpha && found->second.second && output.alpha == AlphaMode::Opaque) {
                output.alpha = AlphaMode::Blend;
                report.Warn("texture_alpha_policy", output.name, "PNG transparency without authored alpha mode uses BLEND; explicit cutout authoring is needed for MASK.");
            }
        }
        for (unsigned type = aiTextureType_NONE + 1; type <= AI_TEXTURE_TYPE_MAX; ++type) {
            const auto current = static_cast<aiTextureType>(type);
            if (current == textureType || !input.GetTextureCount(current)) continue;
            report.Warn("unsupported_material_texture", output.name, "Texture semantic " + std::to_string(type) + " is outside the E1-X base-color material contract and is omitted.");
        }
        aiColor3D emissive;
        if (input.Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS && (emissive.r != 0 || emissive.g != 0 || emissive.b != 0))
            report.Warn("emissive_not_rendered", output.name, "Emissive metadata is outside this E1-X material export contract.");
        scene.materials.push_back(std::move(output));
    }
    return report.Ok();
}

using NodeNames = std::unordered_map<std::string, std::vector<std::uint32_t>>;
bool Nodes(const aiScene& source, Scene& scene, NodeNames& names, std::vector<const aiNode*>& sourceNodes, Report& report) {
    struct Pending { const aiNode* input; int parent; unsigned depth; };
    std::vector<Pending> pending{{source.mRootNode,0,1}};
    std::unordered_set<const aiNode*> visited;
    while (!pending.empty()) {
        const auto current = pending.back(); pending.pop_back();
        if (!current.input || !visited.insert(current.input).second || current.depth > 255 || scene.nodes.size() >= MaxObjects) {
            report.Error("node_hierarchy", "scene", "Null/cyclic/shared node, depth over 255 or too many nodes."); return false;
        }
        const auto& input = *current.input;
        if (input.mNumMeshes > MaxObjects || input.mNumChildren > MaxObjects ||
            (input.mNumMeshes && !input.mMeshes) || (input.mNumChildren && !input.mChildren)) {
            report.Error("node_counts", input.mName.C_Str(), "Invalid mesh or child array."); return false;
        }
        Node output;
        output.name = input.mName.C_Str();
        output.parent = current.parent;
        output.transform = CopyMatrix(input.mTransformation);
        for (unsigned i = 0; i < input.mNumMeshes; ++i) {
            if (input.mMeshes[i] >= source.mNumMeshes) { report.Error("node_mesh_range", output.name, "Mesh reference is outside scene."); return false; }
            output.meshes.push_back(input.mMeshes[i]);
        }
        const auto index = static_cast<std::uint32_t>(scene.nodes.size());
        names[output.name].push_back(index);
        scene.nodes.push_back(std::move(output));
        sourceNodes.push_back(current.input);
        for (unsigned i = input.mNumChildren; i > 0; --i) {
            const auto* child = input.mChildren[i-1];
            if (!child || child->mParent != current.input) { report.Error("node_parent", input.mName.C_Str(), "Child has inconsistent parent."); return false; }
            pending.push_back({child,static_cast<int>(index),current.depth+1});
        }
    }
    return true;
}
bool UniqueNode(const NodeNames& names, const aiString& name, std::uint32_t& result, Report& report) {
    const auto found = names.find(name.C_Str());
    if (found == names.end() || found->second.size() != 1) {
        report.Error("node_name_mapping", name.C_Str(), "Bone/animation target must identify exactly one scene node."); return false;
    }
    result = found->second.front();
    return true;
}
bool Meshes(const aiScene& source, const NodeNames& names, const std::vector<bool>& authoredTangents, const Options& options, Scene& scene, Report& report) {
    std::unordered_map<std::uint32_t,std::uint32_t> joints;
    std::size_t totalVertices{}, totalIndices{}, totalInfluences{};
    for (unsigned i = 0; i < source.mNumMeshes; ++i) {
        if (!source.mMeshes[i]) { report.Error("mesh_missing", std::to_string(i), "Null mesh."); return false; }
        const auto& input = *source.mMeshes[i];
        Mesh output;
        output.name = input.mName.length ? input.mName.C_Str() : "mesh_" + std::to_string(i);
        if (!input.mNumVertices || !input.mVertices || !input.mNumFaces || !input.mFaces ||
            input.mNumVertices > MaxElements - totalVertices || input.mNumFaces > (MaxElements - totalIndices) / 3 || input.mNumBones > MaxObjects) {
            report.Error("mesh_counts", output.name, "Empty mesh, missing data or scene element limit exceeded."); return false;
        }
        totalVertices += input.mNumVertices;
        totalIndices += static_cast<std::size_t>(input.mNumFaces) * 3;
        if (input.mNumAnimMeshes) report.Error("morph_targets", output.name, "Morph target meshes are outside E2-X.");
        for (unsigned c = 0; c < AI_MAX_NUMBER_OF_COLOR_SETS; ++c)
            if (input.HasVertexColors(c)) report.Error("vertex_colors", output.name, "Vertex colors are not supported by E1-X and cannot be silently discarded.");
        for (unsigned c = 1; c < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++c)
            if (input.HasTextureCoords(c)) report.Error("extra_uv_set", output.name, "Only UV0 can be preserved by the E1-X contract.");
        if (input.HasTextureCoords(0) && input.mNumUVComponents[0] > 2)
            report.Error("uv_dimensions", output.name, "Three-component source texture coordinates are unsupported.");
        output.material = input.mMaterialIndex;
        output.hasNormals = input.HasNormals();
        output.hasUV = input.HasTextureCoords(0);
        output.hasTangents = input.HasTangentsAndBitangents();
        if (output.hasTangents && !output.hasNormals) { report.Error("tangent_normals", output.name, "Tangent data requires normals."); return false; }
        output.skinned = input.HasBones();
        if (options.generateTangents && !output.hasTangents)
            report.Warn("tangents_unavailable", output.name, "Tangent generation needs usable normals and UV0; no tangent stream was generated.");
        output.vertices.resize(input.mNumVertices);
        for (unsigned v = 0; v < input.mNumVertices; ++v) {
            auto& vertex = output.vertices[v];
            const auto p = input.mVertices[v]; vertex.position = {p.x,p.y,p.z};
            if (output.hasNormals) { const auto n = input.mNormals[v]; vertex.normal = {n.x,n.y,n.z}; }
            if (output.hasUV) { const auto uv = input.mTextureCoords[0][v]; vertex.uv = {uv.x,uv.y}; }
            if (output.hasTangents) {
                auto normal = input.mNormals[v]; normal.Normalize();
                auto tangent = input.mTangents[v]; tangent -= normal * (normal * tangent); tangent.Normalize();
                float handedness = ((normal ^ tangent) * input.mBitangents[v]) < 0 ? -1.f : 1.f;
                if (authoredTangents[i]) handedness = -handedness;
                vertex.tangent = {tangent.x,tangent.y,tangent.z,handedness};
            }
        }
        for (unsigned f = 0; f < input.mNumFaces; ++f) {
            const auto& face = input.mFaces[f];
            if (face.mNumIndices != 3 || !face.mIndices) { report.Error("topology", output.name, "Only triangle faces are supported after offline triangulation."); return false; }
            for (unsigned j = 0; j < 3; ++j) {
                if (face.mIndices[j] >= input.mNumVertices) { report.Error("index_range", output.name, "Triangle index exceeds vertex count."); return false; }
                output.indices.push_back(face.mIndices[j]);
            }
        }
        for (unsigned b = 0; b < input.mNumBones; ++b) {
            if (!input.mBones || !input.mBones[b]) { report.Error("bone_missing", output.name, "Null bone."); return false; }
            const auto& bone = *input.mBones[b];
            std::uint32_t node{};
            if (!UniqueNode(names, bone.mName, node, report)) return false;
            const auto offset = CopyMatrix(bone.mOffsetMatrix);
            auto found = joints.find(node);
            if (found == joints.end()) {
                if (scene.skeleton.joints.size() >= MaxObjects) { report.Error("joint_limit", output.name, "Too many joints."); return false; }
                const auto joint = static_cast<std::uint32_t>(scene.skeleton.joints.size());
                scene.skeleton.joints.push_back({bone.mName.C_Str(),node,offset});
                found = joints.emplace(node,joint).first;
            } else if (!Near(scene.skeleton.joints[found->second].inverseBind, offset)) {
                report.Error("multiple_bind_spaces", output.name, "Meshes use different inverse binds for the same joint; separate skeleton export is required."); return false;
            }
            if (bone.mNumWeights > MaxElements * 16 - totalInfluences || (bone.mNumWeights && !bone.mWeights)) {
                report.Error("weight_count", output.name, "Missing weights or influence safety limit exceeded."); return false;
            }
            totalInfluences += bone.mNumWeights;
            for (unsigned w = 0; w < bone.mNumWeights; ++w) {
                const auto weight = bone.mWeights[w];
                if (weight.mVertexId >= input.mNumVertices || !std::isfinite(weight.mWeight) || weight.mWeight < 0) {
                    report.Error("weight_invalid", output.name, "Bone weight references an invalid vertex or is negative/non-finite."); return false;
                }
                output.vertices[weight.mVertexId].influences.push_back({found->second,weight.mWeight});
            }
        }
        scene.meshes.push_back(std::move(output));
    }
    if (!scene.skeleton.joints.empty()) scene.skeleton.root = 0;
    return report.Ok();
}

bool Animations(const aiScene& source, const NodeNames& names, const std::vector<const aiNode*>& sourceNodes,
                const Options& options, Scene& scene, Report& report) {
    std::size_t totalKeys{};
    for (unsigned i = 0; i < source.mNumAnimations; ++i) {
        if (!source.mAnimations[i]) { report.Error("animation_missing", std::to_string(i), "Null animation."); return false; }
        const auto& input = *source.mAnimations[i];
        Animation output;
        output.name = input.mName.length ? input.mName.C_Str() : "animation_" + std::to_string(i);
        if (scene.sourceFormat == "dae" || scene.sourceFormat == "fbx")
            report.Warn("source_animation_interpolation", output.name, "Pinned Assimp may linearize authored STEP/Bezier curves before exposing keys. Export preserves its sampled TRS keys, not guaranteed original curve interpolation; bake source animation when exact motion is required.");
        const auto rate = options.ticksPerSecond > 0 ? options.ticksPerSecond : input.mTicksPerSecond;
        if (!std::isfinite(rate) || rate <= 0) { report.Error("animation_timebase", output.name, "Source ticks-per-second is missing/invalid; provide --ticks-per-second explicitly."); return false; }
        if (options.ticksPerSecond > 0) report.Warn("animation_timebase_override", output.name, "Source timebase is explicitly overridden by the command line.");
        if (!std::isfinite(input.mDuration) || input.mDuration < 0 || input.mDuration / rate > std::numeric_limits<float>::max()) {
            report.Error("animation_duration", output.name, "Invalid animation duration."); return false;
        }
        output.duration = static_cast<float>(input.mDuration / rate);
        if (input.mNumMeshChannels || input.mNumMorphMeshChannels) report.Error("animation_morph_mesh", output.name, "Mesh/morph animation channels are outside E2-X.");
        if (!input.mNumChannels || input.mNumChannels > MaxObjects || !input.mChannels) { report.Error("animation_channels", output.name, "No node channels or invalid channel count."); return false; }
        for (unsigned c = 0; c < input.mNumChannels; ++c) {
            if (!input.mChannels[c]) { report.Error("animation_channel_missing", output.name, "Null animation channel."); return false; }
            const auto& channel = *input.mChannels[c];
            std::uint32_t nodeIndex{};
            if (!UniqueNode(names, channel.mNodeName, nodeIndex, report)) return false;
            auto& node = scene.nodes[nodeIndex];
            if (!node.useTRS) {
                aiVector3D scale, translation;
                aiQuaternion rotation;
                sourceNodes[nodeIndex]->mTransformation.Decompose(scale, rotation, translation);
                const aiMatrix4x4 recomposed(scale, rotation, translation);
                if (!Near(node.transform, CopyMatrix(recomposed))) { report.Error("animated_shear", node.name, "Animated node transform cannot be represented losslessly as glTF TRS."); return false; }
                node.translation = {translation.x,translation.y,translation.z};
                node.scale = {scale.x,scale.y,scale.z};
                node.rotation = {rotation.x,rotation.y,rotation.z,rotation.w};
                node.useTRS = true;
            }
            if (channel.mPreState == aiAnimBehaviour_LINEAR || channel.mPreState == aiAnimBehaviour_REPEAT ||
                channel.mPostState == aiAnimBehaviour_LINEAR || channel.mPostState == aiAnimBehaviour_REPEAT)
                report.Warn("animation_extrapolation", node.name, "glTF holds channel endpoints outside each key range; source repeat/linear extrapolation is not exported, including gaps inside the declared clip.");
            const auto boundaryGap = [&](const auto* keys, unsigned count, bool before) {
                return keys && count && (before ? keys[0].mTime > 0 : keys[count-1].mTime < input.mDuration);
            };
            for (const bool before : {true,false}) {
                const auto behaviour = before ? channel.mPreState : channel.mPostState;
                if (behaviour == aiAnimBehaviour_DEFAULT &&
                    (boundaryGap(channel.mPositionKeys,channel.mNumPositionKeys,before) ||
                     boundaryGap(channel.mScalingKeys,channel.mNumScalingKeys,before) ||
                     boundaryGap(channel.mRotationKeys,channel.mNumRotationKeys,before)))
                    report.Warn("animation_default_boundary", node.name, "Source DEFAULT behavior outside a channel key range uses the bind transform; GLB uses an endpoint hold. Check this clip's boundary motion.");
            }
            auto vectors = [&](const aiVectorKey* keys, unsigned count, AnimationPath path) {
                if (!count) return true;
                if (!keys || count > MaxElements - totalKeys) { report.Error("animation_keys", node.name, "Missing key array or scene key limit exceeded."); return false; }
                totalKeys += count;
                Channel result; result.node = nodeIndex; result.path = path;
                for (unsigned k = 0; k < count; ++k) {
                    if (keys[k].mInterpolation != aiAnimInterpolation_Linear) {
                        report.Error("animation_interpolation", node.name, "Vector keys with STEP/spline/unsupported interpolation cannot be exported as LINEAR without changing motion."); return false;
                    }
                    const double seconds = keys[k].mTime / rate;
                    if (!std::isfinite(seconds) || seconds < 0 || seconds > std::numeric_limits<float>::max()) { report.Error("animation_time", node.name, "Invalid animation time."); return false; }
                    result.times.push_back(static_cast<float>(seconds));
                    const auto value = keys[k].mValue;
                    result.values.push_back({value.x,value.y,value.z,0});
                }
                output.channels.push_back(std::move(result));
                return true;
            };
            if (!vectors(channel.mPositionKeys, channel.mNumPositionKeys, AnimationPath::Translation) ||
                !vectors(channel.mScalingKeys, channel.mNumScalingKeys, AnimationPath::Scale)) return false;
            if (channel.mNumRotationKeys) {
                if (!channel.mRotationKeys || channel.mNumRotationKeys > MaxElements - totalKeys) { report.Error("animation_keys", node.name, "Missing rotation keys or scene key limit exceeded."); return false; }
                totalKeys += channel.mNumRotationKeys;
                Channel result; result.node = nodeIndex; result.path = AnimationPath::Rotation;
                for (unsigned k = 0; k < channel.mNumRotationKeys; ++k) {
                    const auto& key = channel.mRotationKeys[k];
                    if (key.mInterpolation != aiAnimInterpolation_Linear && key.mInterpolation != aiAnimInterpolation_Spherical_Linear) {
                        report.Error("animation_interpolation", node.name, "Rotation keys require linear/spherical-linear interpolation; STEP/spline/unsupported modes are rejected."); return false;
                    }
                    const double seconds = key.mTime / rate;
                    const auto value = key.mValue;
                    const double length = std::sqrt(static_cast<double>(value.x)*value.x + static_cast<double>(value.y)*value.y + static_cast<double>(value.z)*value.z + static_cast<double>(value.w)*value.w);
                    if (!std::isfinite(seconds) || seconds < 0 || seconds > std::numeric_limits<float>::max() || !std::isfinite(length) || length <= 1e-12) { report.Error("animation_quaternion", node.name, "Invalid quaternion or key time."); return false; }
                    result.times.push_back(static_cast<float>(seconds));
                    Vec4 quaternion{static_cast<float>(value.x/length),static_cast<float>(value.y/length),static_cast<float>(value.z/length),static_cast<float>(value.w/length)};
                    if (!result.values.empty()) {
                        const auto& previous = result.values.back();
                        if (quaternion[0]*previous[0]+quaternion[1]*previous[1]+quaternion[2]*previous[2]+quaternion[3]*previous[3] < 0)
                            for (auto& component : quaternion) component = -component;
                    }
                    result.values.push_back(quaternion);
                }
                output.channels.push_back(std::move(result));
            }
        }
        scene.animations.push_back(std::move(output));
    }
    return report.Ok();
}
}

bool IsSupportedSource(const std::filesystem::path& path) {
    const auto extension = Lower(PathUTF8(path.extension()));
    return extension == ".obj" || extension == ".dae" || extension == ".fbx" || extension == ".ply" || extension == ".stl";
}

bool ValidateImage(Image& image, Report& report) {
    const auto width = image.width, height = image.height;
    const auto mime = image.mime;
    bool transparent{};
    if (!DecodeImage(image, transparent, report)) return false;
    if ((!mime.empty() && mime != image.mime) || (width && width != image.width) || (height && height != image.height)) {
        report.Error("image_metadata", image.name, "Declared image MIME/dimensions disagree with decoded image payload.");
        return false;
    }
    return true;
}

bool Import(const std::filesystem::path& path, const Options& options, Scene& scene, Report& report) {
    scene = {};
    try {
        if (!IsSupportedSource(path)) { report.Error("source_extension", PathUTF8(path.filename()), "Supported source extensions: .obj .dae .fbx .ply .stl. GR2 conversion is excluded."); return false; }
        if (!std::isfinite(options.sourceMetersPerUnit) || options.sourceMetersPerUnit < 0 ||
            !std::isfinite(options.ticksPerSecond) || options.ticksPerSecond < 0) { report.Error("import_options", "options", "Unit scale and timebase must be finite and nonnegative."); return false; }
        std::error_code ec;
        const auto absolute = std::filesystem::absolute(path, ec);
        if (ec) { report.Error("source_path", PathUTF8(path), "Could not resolve source path."); return false; }
        const auto size = std::filesystem::file_size(absolute, ec);
        if (ec || !size || size > MaxFileBytes) { report.Error("source_file", PathUTF8(path.filename()), "Source missing, unreadable, empty or larger than 256 MiB."); return false; }
        // Imports are serial in this standalone tool; any pre-existing logger/streams remain intact.
        ScopedImportLogging logging(report,PathUTF8(path.filename()));
        Assimp::Importer importer;
        importer.SetIOHandler(new SourceIO(absolute.parent_path(), report));
        importer.SetPropertyBool(AI_CONFIG_IMPORT_NO_SKELETON_MESHES, true);
        importer.SetPropertyBool(AI_CONFIG_IMPORT_REMOVE_EMPTY_BONES, false);
        importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, true);
        importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_IGNORE_UP_DIRECTION, true);
        importer.SetPropertyBool(AI_CONFIG_IMPORT_COLLADA_IGNORE_UP_DIRECTION, false);
        importer.SetPropertyBool(AI_CONFIG_IMPORT_COLLADA_IGNORE_UNIT_SIZE, options.sourceMetersPerUnit > 0);
        unsigned flags = aiProcess_ValidateDataStructure | aiProcess_Triangulate | aiProcess_TransformUVCoords | aiProcess_FlipUVs;
        if (options.generateNormals) flags |= aiProcess_GenSmoothNormals;
        if (options.generateTangents) flags |= aiProcess_CalcTangentSpace;
        const auto* source = importer.ReadFile(PathUTF8(absolute), aiProcess_ValidateDataStructure);
        if (!source) { report.Error("assimp_import", PathUTF8(path.filename()), importer.GetErrorString()); return false; }
        if (!source->mRootNode || !source->mNumMeshes || !source->mMeshes || source->mNumMeshes > MaxObjects ||
            !source->mNumMaterials || !source->mMaterials || source->mNumMaterials > MaxObjects ||
            source->mNumTextures > MaxObjects || source->mNumAnimations > MaxObjects || (source->mNumAnimations && !source->mAnimations)) {
            report.Error("scene_structure", PathUTF8(path.filename()), "Missing scene data or scene object limits exceeded."); return false;
        }
        std::vector<bool> authoredTangents(source->mNumMeshes);
        for (unsigned i = 0; i < source->mNumMeshes; ++i) {
            const auto* mesh = source->mMeshes[i];
            if (!mesh) { report.Error("mesh_missing", std::to_string(i), "Null mesh."); return false; }
            if (mesh->mNumVertices > MaxElements || mesh->mNumFaces > MaxElements / 3 || mesh->mNumBones > MaxObjects) {
                report.Error("source_mesh_limit", mesh->mName.C_Str(), "Source mesh exceeds element limits before postprocessing."); return false;
            }
            authoredTangents[i] = mesh->HasTangentsAndBitangents();
            if (authoredTangents[i] && mesh->mMaterialIndex < source->mNumMaterials && source->mMaterials[mesh->mMaterialIndex]) {
                const auto& material = *source->mMaterials[mesh->mMaterialIndex];
                for (unsigned type = aiTextureType_NONE + 1; type <= AI_TEXTURE_TYPE_MAX; ++type) {
                    aiUVTransform transform;
                    if (material.Get(AI_MATKEY_UVTRANSFORM(static_cast<aiTextureType>(type), 0), transform) == AI_SUCCESS &&
                        (transform.mRotation != 0 || transform.mScaling.x != 1 || transform.mScaling.y != 1)) {
                        report.Error("authored_tangent_uv_transform", mesh->mName.C_Str(), "Authored tangents with UV rotation/scale require rebaking in the source authoring tool."); return false;
                    }
                }
            }
        }
        source = importer.ApplyPostProcessing(flags);
        if (!source) { report.Error("assimp_processing", PathUTF8(path.filename()), importer.GetErrorString()); return false; }
        if (source->mNumMeshes != authoredTangents.size()) { report.Error("mesh_mapping", scene.name, "Postprocessing changed the source mesh mapping unexpectedly."); return false; }
        scene.name = PathUTF8(path.stem());
        scene.sourceFormat = Lower(PathUTF8(path.extension())).substr(1);
        double rootScale = 1;
        Matrix rootBasis = Identity;
        if (scene.sourceFormat == "fbx") {
            float sourceCentimetersMetadata = 1;
            if (!source->mMetaData || !source->mMetaData->Get("UnitScaleFactor", sourceCentimetersMetadata)) {
                report.Error("fbx_units", scene.name, "FBX unit metadata was not provided by the pinned importer."); return false;
            }
            const double sourceCentimeters = sourceCentimetersMetadata;
            if (!std::isfinite(sourceCentimeters) || sourceCentimeters <= 0) { report.Error("fbx_units", scene.name, "Invalid FBX UnitScaleFactor."); return false; }
            scene.sourceMetersPerUnit = options.sourceMetersPerUnit > 0 ? options.sourceMetersPerUnit : sourceCentimeters * .01;
            rootScale = scene.sourceMetersPerUnit;
            scene.unitPolicy = options.sourceMetersPerUnit > 0 ? "FBX explicit meters/source-unit override; signed source axes retained" : "FBX UnitScaleFactor centimeters/source-unit multiplied by 0.01; signed source axes and units applied once at canonical root";
            std::int32_t up=1, front=2, right=0, upSign=1, frontSign=1, rightSign=1;
            if (!source->mMetaData->Get("UpAxis", up) || !source->mMetaData->Get("FrontAxis", front) || !source->mMetaData->Get("CoordAxis", right) ||
                !source->mMetaData->Get("UpAxisSign", upSign) || !source->mMetaData->Get("FrontAxisSign", frontSign) || !source->mMetaData->Get("CoordAxisSign", rightSign) ||
                up < 0 || up > 2 || front < 0 || front > 2 || right < 0 || right > 2 || up == front || up == right || front == right ||
                (upSign != -1 && upSign != 1) || (frontSign != -1 && frontSign != 1) || (rightSign != -1 && rightSign != 1)) {
                report.Error("fbx_axes", scene.name, "FBX axis indices must be distinct in [0,2] and their signs must be +/-1."); return false;
            }
            rootBasis = {};
            rootBasis[static_cast<std::size_t>(right)*4] = static_cast<float>(rightSign);
            rootBasis[static_cast<std::size_t>(up)*4+1] = static_cast<float>(upSign);
            rootBasis[static_cast<std::size_t>(front)*4+2] = static_cast<float>(frontSign);
            rootBasis[15] = 1;
        } else if (scene.sourceFormat == "dae") {
            rootScale = options.sourceMetersPerUnit > 0 ? options.sourceMetersPerUnit : 1;
            const auto& basis = source->mRootNode->mTransformation;
            // COLLADA visual_scene has no authored transform: this root basis holds unit scale and up-axis rotation.
            scene.sourceMetersPerUnit = options.sourceMetersPerUnit > 0 ? options.sourceMetersPerUnit :
                std::sqrt(static_cast<double>(basis.a1)*basis.a1 + static_cast<double>(basis.b1)*basis.b1 + static_cast<double>(basis.c1)*basis.c1);
            scene.unitPolicy = options.sourceMetersPerUnit > 0 ? "COLLADA explicit meters/source-unit override; importer unit scale disabled" : "COLLADA <unit meter> and up_axis applied once by Assimp root; resulting scene uses meters";
        } else {
            rootScale = options.sourceMetersPerUnit > 0 ? options.sourceMetersPerUnit : 1;
            scene.sourceMetersPerUnit = rootScale;
            scene.unitPolicy = options.sourceMetersPerUnit > 0 ? "Explicit meters/source-unit; RH Y-up authoring convention" : "Unitless source defaults to 1 meter/unit and RH Y-up; use --meters-per-unit when authored otherwise";
            if (!options.sourceMetersPerUnit) report.Warn("source_unit_default", scene.name, scene.unitPolicy);
        }
        if (!std::isfinite(rootScale) || rootScale <= 0 || rootScale > std::numeric_limits<float>::max() || static_cast<float>(rootScale) == 0 ||
            !std::isfinite(scene.sourceMetersPerUnit) || scene.sourceMetersPerUnit <= 0) { report.Error("unit_scale", scene.name, "Canonical/source scale is outside finite positive range."); return false; }
        Node root;
        root.name = "ZiiNAN_Canonical_Meters";
        root.transform = rootBasis;
        for (std::size_t c = 0; c < 3; ++c) for (std::size_t r = 0; r < 3; ++r) root.transform[c*4+r] *= static_cast<float>(rootScale);
        scene.nodes.push_back(std::move(root));
        NodeNames names;
        std::vector<const aiNode*> sourceNodes{nullptr};
        if (!Nodes(*source, scene, names, sourceNodes, report) || !Materials(*source, absolute.parent_path(), scene, report) ||
            !Meshes(*source, names, authoredTangents, options, scene, report) || !Animations(*source, names, sourceNodes, options, scene, report)) return false;
        if (source->mNumCameras) report.Warn("cameras_omitted", scene.name, "Source cameras are outside this model-only export contract.");
        if (source->mNumLights) report.Warn("lights_omitted", scene.name, "Source lights are outside this model-only export contract.");
        return report.Ok();
    } catch (const std::bad_alloc&) {
        report.Error("allocation", PathUTF8(path.filename()), "Importer allocation failed within configured source/scene limits.");
    } catch (const std::exception& error) {
        report.Error("import_exception", PathUTF8(path.filename()), error.what());
    }
    return false;
}
}
