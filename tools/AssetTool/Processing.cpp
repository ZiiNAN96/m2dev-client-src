#include "Scene.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace ZiiNAN::AssetTool {
namespace {
constexpr std::size_t MaxDepth = 256;
constexpr std::size_t MaxImageBytes = 32u * 1024u * 1024u;
constexpr double MaxCoordinate = 100000000.0;

template<std::size_t N> bool Finite(const std::array<float, N>& value)
{
    return std::all_of(value.begin(), value.end(), [](float v) { return std::isfinite(v); });
}
template<std::size_t N> double LengthSquared(const std::array<float, N>& value)
{
    double length = 0;
    for (float v : value) length += double(v) * v;
    return length;
}
bool Fail(Report& report, const char* code, const std::string& context, const std::string& message)
{
    report.Error(code, context, message);
    return false;
}
std::string MeshContext(const Scene& scene, std::size_t index)
{
    return "mesh[" + std::to_string(index) + "] " + scene.meshes[index].name;
}
std::string NodeContext(const Scene& scene, std::size_t index)
{
    return "node[" + std::to_string(index) + "] " + scene.nodes[index].name;
}
bool Affine(const Matrix& m)
{
    return Finite(m) && std::abs(m[3]) + std::abs(m[7]) + std::abs(m[11]) < 1e-6f &&
        std::abs(m[15] - 1) < 1e-6f;
}
Matrix Multiply(const Matrix& a, const Matrix& b)
{
    Matrix result{};
    for (std::size_t column = 0; column < 4; ++column)
        for (std::size_t row = 0; row < 4; ++row) {
            double value = 0;
            for (std::size_t k = 0; k < 4; ++k) value += double(a[k * 4 + row]) * b[column * 4 + k];
            result[column * 4 + row] = static_cast<float>(value);
        }
    return result;
}
Matrix LocalMatrix(const Node& node)
{
    if (!node.useTRS) return node.transform;
    const double x = node.rotation[0], y = node.rotation[1], z = node.rotation[2], w = node.rotation[3];
    Matrix m = Identity;
    m[0] = static_cast<float>((1 - 2 * (y*y + z*z)) * node.scale[0]);
    m[1] = static_cast<float>((2 * (x*y + z*w)) * node.scale[0]);
    m[2] = static_cast<float>((2 * (x*z - y*w)) * node.scale[0]);
    m[4] = static_cast<float>((2 * (x*y - z*w)) * node.scale[1]);
    m[5] = static_cast<float>((1 - 2 * (x*x + z*z)) * node.scale[1]);
    m[6] = static_cast<float>((2 * (y*z + x*w)) * node.scale[1]);
    m[8] = static_cast<float>((2 * (x*z + y*w)) * node.scale[2]);
    m[9] = static_cast<float>((2 * (y*z - x*w)) * node.scale[2]);
    m[10] = static_cast<float>((1 - 2 * (x*x + y*y)) * node.scale[2]);
    std::copy(node.translation.begin(), node.translation.end(), m.begin() + 12);
    return m;
}
double Determinant(const Matrix& m)
{
    return double(m[0]) * (double(m[5]) * m[10] - double(m[9]) * m[6]) -
        double(m[4]) * (double(m[1]) * m[10] - double(m[9]) * m[2]) +
        double(m[8]) * (double(m[1]) * m[6] - double(m[5]) * m[2]);
}
void Expand(Bounds& bounds, const Vec3& p)
{
    if (!bounds.valid) { bounds.min = bounds.max = p; bounds.valid = true; return; }
    for (std::size_t i = 0; i < 3; ++i) {
        bounds.min[i] = std::min(bounds.min[i], p[i]);
        bounds.max[i] = std::max(bounds.max[i], p[i]);
    }
}
bool ValidBounds(const Bounds& bounds)
{
    if (!bounds.valid || !Finite(bounds.min) || !Finite(bounds.max)) return false;
    for (std::size_t i = 0; i < 3; ++i) if (bounds.min[i] > bounds.max[i]) return false;
    return true;
}
bool Quaternion(const Vec4& q)
{
    return Finite(q) && std::abs(LengthSquared(q) - 1.0) < 0.002;
}
bool NormalizeQuaternion(Vec4& q)
{
    const double length = LengthSquared(q);
    if (!Finite(q) || !std::isfinite(length) || length <= 1e-30) return false;
    const double inverseLength = 1.0 / std::sqrt(length);
    for (float& value : q) value = static_cast<float>(value * inverseLength);
    return true;
}
void CanonicalQuaternionSign(Vec4& q)
{
    for (std::size_t index : {std::size_t(3), std::size_t(0), std::size_t(1), std::size_t(2)}) {
        if (q[index] == 0) continue;
        if (q[index] < 0) for (float& value : q) value = -value;
        break;
    }
}
bool ValidateGraph(const Scene& scene, std::vector<Matrix>& world, Report& report)
{
    std::vector<unsigned char> state(scene.nodes.size());
    std::vector<std::size_t> depth(scene.nodes.size());
    world.resize(scene.nodes.size());
    for (std::size_t n = 0; n < scene.nodes.size(); ++n) {
        const auto& node = scene.nodes[n];
        const auto context = NodeContext(scene, n);
        if (node.parent < -1 || (node.parent >= 0 && static_cast<std::size_t>(node.parent) >= scene.nodes.size()))
            return Fail(report, "node_parent", context, "Parent index is outside the node array.");
        if (node.useTRS) {
            if (!Finite(node.translation) || !Finite(node.scale) || !Quaternion(node.rotation))
                return Fail(report, "node_transform", context, "TRS must be finite with a normalized nonzero quaternion.");
        } else if (!Affine(node.transform))
            return Fail(report, "node_transform", context, "Node matrix must be finite and affine (column-major).");
        std::set<std::uint32_t> meshes;
        for (auto mesh : node.meshes) {
            if (mesh >= scene.meshes.size()) return Fail(report, "node_mesh", context, "Mesh index is out of range.");
            if (!meshes.insert(mesh).second) return Fail(report, "node_mesh", context, "The same mesh is attached twice to one node.");
        }
    }
    // ZiiNAN: Offline graph evaluation bounds hierarchy work and never recurses on source data.
    std::vector<std::size_t> chain;
    for (std::size_t start = 0; start < scene.nodes.size(); ++start) {
        if (state[start] == 2) continue;
        chain.clear();
        int current = static_cast<int>(start);
        while (current >= 0 && state[static_cast<std::size_t>(current)] != 2) {
            const auto index = static_cast<std::size_t>(current);
            if (state[index] == 1) return Fail(report, "node_cycle", NodeContext(scene, index), "Node hierarchy contains a cycle.");
            state[index] = 1;
            chain.push_back(index);
            if (chain.size() > MaxDepth) return Fail(report, "node_depth", NodeContext(scene, index), "Hierarchy exceeds 256 nodes in depth.");
            current = scene.nodes[index].parent;
        }
        while (!chain.empty()) {
            const auto index = chain.back(); chain.pop_back();
            const auto& node = scene.nodes[index];
            depth[index] = node.parent < 0 ? 1 : depth[static_cast<std::size_t>(node.parent)] + 1;
            if (depth[index] > MaxDepth) return Fail(report, "node_depth", NodeContext(scene, index), "Hierarchy exceeds 256 nodes in depth.");
            if (!node.meshes.empty() && depth[index] == MaxDepth)
                return Fail(report, "node_depth", NodeContext(scene, index), "Export mesh attachment would exceed the 256-node hierarchy depth.");
            const auto local = LocalMatrix(node);
            world[index] = node.parent < 0 ? local : Multiply(world[static_cast<std::size_t>(node.parent)], local);
            if (!Affine(world[index])) return Fail(report, "node_world", NodeContext(scene, index), "Composed node transform is non-finite or non-affine.");
            state[index] = 2;
        }
    }
    return true;
}
bool Degenerate(const Vec3& a, const Vec3& b, const Vec3& c)
{
    const double ux = double(b[0]) - a[0], uy = double(b[1]) - a[1], uz = double(b[2]) - a[2];
    const double vx = double(c[0]) - a[0], vy = double(c[1]) - a[1], vz = double(c[2]) - a[2];
    const double x = uy*vz - uz*vy, y = uz*vx - ux*vz, z = ux*vy - uy*vx;
    const double edge = std::max(ux*ux + uy*uy + uz*uz, vx*vx + vy*vy + vz*vz);
    return edge == 0 || x*x + y*y + z*z <= edge*edge*1e-24;
}
bool Preflight(const Scene& scene, Report& report)
{
    if (scene.meshes.size() > MaxObjects || scene.nodes.size() > MaxObjects || scene.animations.size() > MaxObjects ||
        scene.materials.size() > MaxObjects || scene.images.size() > MaxObjects || scene.skeleton.joints.size() > MaxObjects)
        return Fail(report, "scene_count", scene.name, "Too many objects to process safely.");
    std::size_t remaining = MaxFileBytes;
    auto charge = [&](std::size_t count, std::size_t stride) {
        if (count > remaining / stride) return false;
        remaining -= count * stride;
        return true;
    };
    for (const auto& mesh : scene.meshes) {
        if (mesh.vertices.size() > MaxElements || mesh.indices.size() > MaxElements)
            return Fail(report, "mesh_count", mesh.name, "Geometry stream count exceeds 4194304.");
        if (!charge(mesh.vertices.size(), sizeof(Vertex)) || !charge(mesh.indices.size(), sizeof(std::uint32_t)))
            return Fail(report, "processing_budget", scene.name, "Owned geometry exceeds the 256 MiB processing data budget.");
        for (const auto& vertex : mesh.vertices) {
            if (vertex.influences.size() > MaxObjects || !charge(vertex.influences.size(), sizeof(Influence)))
                return Fail(report, "processing_budget", mesh.name, "Influence count or owned processing data budget exceeded.");
        }
    }
    for (const auto& animation : scene.animations) {
        if (animation.channels.size() > MaxObjects) return Fail(report,"animation_channels",animation.name,"Too many animation channels.");
        for (const auto& channel : animation.channels) {
            if (channel.values.size() > MaxElements || channel.times.size() > MaxElements)
                return Fail(report,"animation_keys",animation.name,"Too many animation keys.");
            if (!charge(channel.values.size(), sizeof(Vec4)) || !charge(channel.times.size(), sizeof(float)))
                return Fail(report,"processing_budget",animation.name,"Animation streams exceed the 256 MiB processing data budget.");
        }
    }
    for (const auto& image : scene.images)
        if (!charge(image.bytes.size(), 1)) return Fail(report,"processing_budget",image.name,"Encoded images exceed the 256 MiB processing data budget.");
    return true;
}
using Double3 = std::array<double,3>;
Double3 Cross(const Double3& a, const Double3& b)
{
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}
double Dot(const Double3& a, const Double3& b)
{
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
bool Normalize(Double3& value)
{
    const double length = Dot(value,value);
    if (!std::isfinite(length) || length <= 1e-30) return false;
    const double inverse = 1.0/std::sqrt(length);
    for (auto& component : value) component *= inverse;
    return true;
}
bool GenerateAttributes(Scene& scene, const Options& options, Report& report)
{
    for (std::size_t m = 0; m < scene.meshes.size(); ++m) {
        auto& mesh = scene.meshes[m];
        const auto context = MeshContext(scene,m);
        const bool normals = !mesh.hasNormals && options.generateNormals;
        const bool tangents = !mesh.hasTangents && options.generateTangents;
        if (!normals && !tangents) continue;
        if (mesh.vertices.empty() || mesh.indices.empty() || mesh.indices.size()%3)
            return Fail(report,"mesh_empty",context,"Attribute generation requires a nonempty triangle mesh.");
        for (auto index : mesh.indices) if (index >= mesh.vertices.size())
            return Fail(report,"index_range",context,"Cannot generate attributes with an out-of-range triangle index.");
        for (const auto& vertex : mesh.vertices) if (!Finite(vertex.position) || (tangents && !Finite(vertex.uv)))
            return Fail(report,"vertex_finite",context,"Cannot generate attributes from non-finite positions or UVs.");
        if (tangents && (!mesh.hasUV || (!mesh.hasNormals && !normals)))
            return Fail(report,"tangent_channels",context,"Tangent generation requires UV0 and supplied or generated normals.");
        std::vector<Double3> accumulatedNormal(normals ? mesh.vertices.size() : 0);
        std::vector<Double3> accumulatedTangent(tangents ? mesh.vertices.size() : 0);
        std::vector<Double3> accumulatedBitangent(tangents ? mesh.vertices.size() : 0);
        std::size_t degenerateUV = 0;
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
            const auto a = mesh.indices[i], b = mesh.indices[i+1], c = mesh.indices[i+2];
            const auto& av = mesh.vertices[a]; const auto& bv = mesh.vertices[b]; const auto& cv = mesh.vertices[c];
            Double3 u{}, v{};
            for (std::size_t k = 0; k < 3; ++k) { u[k] = double(bv.position[k])-av.position[k]; v[k] = double(cv.position[k])-av.position[k]; }
            if (normals) {
                const auto normal = Cross(u,v);
                for (auto index : {a,b,c}) for (std::size_t k = 0; k < 3; ++k) accumulatedNormal[index][k] += normal[k];
            }
            if (tangents) {
                const double ux = double(bv.uv[0])-av.uv[0], uy = double(bv.uv[1])-av.uv[1];
                const double vx = double(cv.uv[0])-av.uv[0], vy = double(cv.uv[1])-av.uv[1];
                const double determinant = ux*vy-uy*vx;
                if (std::abs(determinant) <= 1e-20) { ++degenerateUV; continue; }
                for (auto index : {a,b,c}) for (std::size_t k = 0; k < 3; ++k) {
                    accumulatedTangent[index][k] += (u[k]*vy-v[k]*uy)/determinant;
                    accumulatedBitangent[index][k] += (v[k]*ux-u[k]*vx)/determinant;
                }
            }
        }
        std::size_t fallbackNormals = 0, fallbackTangents = 0;
        for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
            auto& vertex = mesh.vertices[i];
            if (normals) {
                auto normal = accumulatedNormal[i];
                if (!Normalize(normal)) { normal = {0,1,0}; ++fallbackNormals; }
                for (std::size_t k = 0; k < 3; ++k) vertex.normal[k] = static_cast<float>(normal[k]);
            }
            if (tangents) {
                Double3 normal{vertex.normal[0],vertex.normal[1],vertex.normal[2]};
                if (!Normalize(normal)) return Fail(report,"vertex_normal",context,"Tangent generation requires finite nonzero normals.");
                auto tangent = accumulatedTangent[i];
                const double projection = Dot(normal,tangent);
                for (std::size_t k = 0; k < 3; ++k) tangent[k] -= normal[k]*projection;
                if (!Normalize(tangent)) {
                    tangent = Cross(normal,std::abs(normal[2]) < .9 ? Double3{0,0,1} : Double3{0,1,0});
                    Normalize(tangent); ++fallbackTangents;
                }
                for (std::size_t k = 0; k < 3; ++k) vertex.tangent[k] = static_cast<float>(tangent[k]);
                vertex.tangent[3] = Dot(Cross(normal,tangent),accumulatedBitangent[i]) < 0 ? -1.0f : 1.0f;
            }
        }
        if (normals) { mesh.hasNormals = true; report.Warn("normals_generated",context,"Missing normals generated by area-weighted triangle accumulation; authored normals remain unchanged."); }
        if (tangents) { mesh.hasTangents = true; report.Warn("tangents_generated",context,"Missing tangents generated from UV0 and orthogonalized against normals."); }
        if (fallbackNormals) report.Warn("normal_fallback",context,std::to_string(fallbackNormals)+" unused or degenerate vertices received the canonical +Y fallback normal.");
        if (fallbackTangents || degenerateUV) report.Warn("tangent_fallback",context,std::to_string(degenerateUV)+" degenerate UV triangles; "+std::to_string(fallbackTangents)+" vertices received a deterministic orthogonal tangent.");
    }
    return true;
}
bool NormalizeDirections(Scene& scene, Report& report)
{
    for (std::size_t m = 0; m < scene.meshes.size(); ++m) {
        auto& mesh = scene.meshes[m];
        const auto context = MeshContext(scene,m);
        std::size_t normalChanges = 0, tangentChanges = 0;
        if (mesh.hasTangents && !mesh.hasNormals)
            return Fail(report,"vertex_tangent",context,"Tangents require a normal channel.");
        for (auto& vertex : mesh.vertices) {
            if (!mesh.hasNormals) continue;
            Double3 normal{vertex.normal[0],vertex.normal[1],vertex.normal[2]};
            if (!Finite(vertex.normal) || !Normalize(normal))
                return Fail(report,"vertex_normal",context,"Cannot normalize a zero or non-finite normal direction.");
            if (std::abs(LengthSquared(vertex.normal) - 1.0) > 1e-5) ++normalChanges;
            for (std::size_t k = 0; k < 3; ++k) vertex.normal[k] = static_cast<float>(normal[k]);
            if (!mesh.hasTangents) continue;
            if (!Finite(vertex.tangent) || (vertex.tangent[3] != 1 && vertex.tangent[3] != -1))
                return Fail(report,"vertex_tangent",context,"Tangent must be finite with a handedness of +1 or -1.");
            Double3 tangent{vertex.tangent[0],vertex.tangent[1],vertex.tangent[2]};
            if (!Normalize(tangent)) return Fail(report,"vertex_tangent",context,"Cannot normalize a zero tangent direction.");
            const auto perpendicular = Cross(normal,tangent);
            if (Dot(perpendicular,perpendicular) <= 1e-24)
                return Fail(report,"vertex_tangent",context,"Tangent direction is parallel to the vertex normal.");
            const double projection = Dot(normal,tangent);
            for (std::size_t k = 0; k < 3; ++k) tangent[k] -= normal[k]*projection;
            if (!Normalize(tangent)) return Fail(report,"vertex_tangent",context,"Tangent orthogonalization produced a zero direction.");
            bool changed = false;
            for (std::size_t k = 0; k < 3; ++k) {
                const auto value = static_cast<float>(tangent[k]);
                changed |= std::abs(value - vertex.tangent[k]) > 1e-5f;
                vertex.tangent[k] = value;
            }
            if (changed) ++tangentChanges;
        }
        if (normalChanges) report.Warn("normals_normalized",context,std::to_string(normalChanges)+" normal directions normalized to unit length without regenerating authored directions.");
        if (tangentChanges) report.Warn("tangents_normalized",context,std::to_string(tangentChanges)+" tangent directions normalized and orthogonalized against normals; handedness preserved.");
    }
    return true;
}
bool NormalizeWeights(Scene& scene, Report& report)
{
    for (std::size_t m = 0; m < scene.meshes.size(); ++m) {
        auto& mesh = scene.meshes[m];
        if (mesh.vertices.size() > MaxElements) return Fail(report, "mesh_count", MeshContext(scene,m), "Vertex count exceeds the runtime limit.");
        if (!mesh.skinned) continue;
        std::size_t reduced = 0, merged = 0, maximum = 0;
        for (std::size_t v = 0; v < mesh.vertices.size(); ++v) {
            auto& influences = mesh.vertices[v].influences;
            if (influences.size() > MaxObjects) return Fail(report, "skin_count", MeshContext(scene,m), "Vertex influence count exceeds the joint limit.");
            maximum = std::max(maximum, influences.size());
            std::map<std::uint32_t, double> accumulated;
            for (const auto& influence : influences) {
                if (influence.joint >= scene.skeleton.joints.size())
                    return Fail(report, "skin_joint", MeshContext(scene,m), "Vertex " + std::to_string(v) + " has an out-of-range joint.");
                if (!std::isfinite(influence.weight) || influence.weight < 0)
                    return Fail(report, "skin_weight", MeshContext(scene,m), "Vertex " + std::to_string(v) + " has a negative or non-finite weight.");
                if (influence.weight > 0) accumulated[influence.joint] += influence.weight;
            }
            std::vector<std::pair<std::uint32_t, double>> sorted(accumulated.begin(), accumulated.end());
            if (sorted.size() < influences.size()) ++merged;
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
                return a.second != b.second ? a.second > b.second : a.first < b.first;
            });
            if (sorted.size() > 4) { sorted.resize(4); ++reduced; }
            double sum = 0;
            for (const auto& entry : sorted) sum += entry.second;
            if (!std::isfinite(sum) || sum <= 1e-30)
                return Fail(report, "skin_sum", MeshContext(scene,m), "Vertex " + std::to_string(v) + " has zero or invalid total weight.");
            influences.clear();
            for (const auto& entry : sorted) influences.push_back({entry.first, static_cast<float>(entry.second / sum)});
            // Correct float rounding in the largest influence without changing tie order.
            float floatSum = 0;
            for (const auto& influence : influences) floatSum += influence.weight;
            influences.front().weight += 1.0f - floatSum;
        }
        if (merged) report.Warn("skin_merge", MeshContext(scene,m), std::to_string(merged) + " vertices merged duplicate joints or removed zero weights.");
        if (reduced) report.Warn("skin_reduce", MeshContext(scene,m), std::to_string(reduced) + " vertices reduced from up to " +
            std::to_string(maximum) + " influences to the four largest, ordered by weight then joint ID, and renormalized.");
    }
    return true;
}
bool NormalizeRotations(Scene& scene, Report& report)
{
    for (std::size_t n = 0; n < scene.nodes.size(); ++n) if (scene.nodes[n].useTRS) {
        if (!NormalizeQuaternion(scene.nodes[n].rotation))
            return Fail(report, "node_rotation", NodeContext(scene,n), "Cannot normalize a zero or non-finite quaternion.");
        CanonicalQuaternionSign(scene.nodes[n].rotation);
    }
    for (auto& animation : scene.animations) for (auto& channel : animation.channels) {
        if (channel.path != AnimationPath::Rotation) continue;
        Vec4 previous{};
        bool first = true;
        for (auto& value : channel.values) {
            if (!NormalizeQuaternion(value)) return Fail(report, "animation_rotation", animation.name, "Cannot normalize a zero or non-finite rotation key.");
            if (first) { CanonicalQuaternionSign(value); first = false; }
            else {
                double dot = 0;
                for (std::size_t i = 0; i < 4; ++i) dot += double(previous[i]) * value[i];
                if (dot < 0) for (float& component : value) component = -component;
            }
            previous = value;
        }
    }
    return true;
}
bool PreserveAnimationDuration(Scene& scene, Report& report)
{
    for (auto& animation : scene.animations) {
        std::size_t keys = 0, extended = 0;
        for (const auto& channel : animation.channels) {
            keys += channel.times.size();
            if (channel.times.back() < animation.duration) {
                if (channel.times.size() == MaxElements)
                    return Fail(report,"animation_budget",animation.name,"Preserving clip duration would exceed the channel key limit.");
                ++extended;
            }
        }
        if (extended > MaxElements - keys)
            return Fail(report,"animation_budget",animation.name,"Preserving clip duration would exceed the clip key limit.");
        for (auto& channel : animation.channels) if (channel.times.back() < animation.duration) {
            const auto value = channel.values.back();
            channel.times.push_back(animation.duration);
            channel.values.push_back(value);
        }
        if (extended) report.Warn("animation_hold",animation.name,std::to_string(extended) +
            " channels extended with a final held value to preserve the declared clip duration of " + std::to_string(animation.duration) +
            " seconds; glTF derives duration from key times.");
    }
    return true;
}
double CacheRatio(const Mesh& mesh)
{
    return meshopt_analyzeVertexCache(mesh.indices.data(), mesh.indices.size(), mesh.vertices.size(), 16, 0, 0).acmr;
}
void OptimizeMesh(Mesh& mesh, bool reorderTriangles)
{
    if (reorderTriangles)
        meshopt_optimizeVertexCache(mesh.indices.data(), mesh.indices.data(), mesh.indices.size(), mesh.vertices.size());
    std::vector<unsigned int> remap(mesh.vertices.size());
    const auto count = meshopt_optimizeVertexFetchRemap(remap.data(), mesh.indices.data(), mesh.indices.size(), mesh.vertices.size());
    std::vector<Vertex> vertices(count);
    for (std::size_t old = 0; old < mesh.vertices.size(); ++old)
        if (remap[old] != std::numeric_limits<unsigned int>::max()) vertices[remap[old]] = std::move(mesh.vertices[old]);
    meshopt_remapIndexBuffer(mesh.indices.data(), mesh.indices.data(), mesh.indices.size(), remap.data());
    mesh.vertices = std::move(vertices);
}
}

bool Validate(Scene& scene, Report& report)
{
    if (!report.Ok()) return false;
    if (!Preflight(scene, report)) return false;
    scene.bounds = {};
    if (scene.meshes.empty()) return Fail(report, "no_meshes", scene.name, "Scene has no meshes.");
    if (scene.nodes.empty()) return Fail(report, "no_nodes", scene.name, "Scene has no nodes.");
    if (scene.meshes.size() > MaxObjects || scene.nodes.size() > MaxObjects || scene.materials.size() > MaxObjects ||
        scene.images.size() > MaxObjects || scene.animations.size() > MaxObjects || scene.skeleton.joints.size() > MaxObjects)
        return Fail(report, "scene_count", scene.name, "Object count exceeds the E1-X runtime limit of 32768.");
    if (!std::isfinite(scene.sourceMetersPerUnit) || scene.sourceMetersPerUnit <= 0)
        return Fail(report, "unit_scale", scene.name, "Source meters-per-unit must be finite and positive.");
    for (std::size_t m = 0; m < scene.materials.size(); ++m) {
        const auto& material = scene.materials[m];
        if (!Finite(material.baseColor) || std::any_of(material.baseColor.begin(), material.baseColor.end(), [](float v) { return v < 0 || v > 1; }))
            return Fail(report, "material_color", material.name, "Base color components must be finite and in [0,1].");
        if (!std::isfinite(material.alphaCutoff) || material.alphaCutoff < 0)
            return Fail(report, "material_alpha", material.name, "Alpha cutoff must be finite and nonnegative.");
        if (material.alpha != AlphaMode::Opaque && material.alpha != AlphaMode::Mask && material.alpha != AlphaMode::Blend)
            return Fail(report, "material_alpha", material.name, "Unknown alpha mode.");
        if (material.baseTexture < -1 || (material.baseTexture >= 0 && static_cast<std::size_t>(material.baseTexture) >= scene.images.size()))
            return Fail(report, "material_texture", material.name, "Base texture index is out of range.");
    }
    for (auto& image : scene.images) {
        if(!image.packPath.empty()) {
            if(!image.packPath.starts_with("d:/ymir work/") || !image.packPath.ends_with(".dds") ||
               image.packPath.find("..")!=std::string::npos || image.packPath.find('\\')!=std::string::npos || !image.bytes.empty())
                return Fail(report,"pack_texture",image.name,"Expected a canonical DDS pack key without embedded bytes.");
            continue;
        }
        if (!image.width || !image.height || image.width > 8192 || image.height > 8192 || std::uint64_t(image.width) * image.height > 16777216)
            return Fail(report, "image_dimensions", image.name, "Image dimensions exceed the E1-X image limits or are zero.");
        if (image.bytes.empty() || image.bytes.size() > MaxImageBytes)
            return Fail(report, "image_size", image.name, "Encoded image is empty or exceeds 32 MiB.");
        const std::array<std::uint8_t,8> png{137,80,78,71,13,10,26,10};
        const bool isPNG = image.bytes.size() >= 24 && std::equal(png.begin(),png.end(),image.bytes.begin());
        const bool isJPEG = image.bytes.size() >= 4 && image.bytes[0] == 255 && image.bytes[1] == 216;
        if ((image.mime != "image/png" || !isPNG) && (image.mime != "image/jpeg" || !isJPEG))
            return Fail(report, "image_format", image.name, "Image MIME and PNG/JPEG signature must agree.");
        if (!ValidateImage(image, report)) return false;
    }
    std::vector<Matrix> world;
    if (!ValidateGraph(scene, world, report)) return false;
    if (scene.skeleton.root < -1 || (scene.skeleton.root >= 0 && static_cast<std::size_t>(scene.skeleton.root) >= scene.nodes.size()))
        return Fail(report, "skeleton_root", scene.name, "Skeleton root node is out of range.");
    if (scene.skeleton.joints.empty() && scene.skeleton.root >= 0)
        return Fail(report, "skeleton_empty", scene.name, "Skeleton root exists without any joints.");
    std::set<std::uint32_t> jointNodes;
    for (const auto& joint : scene.skeleton.joints) {
        if (joint.node >= scene.nodes.size()) return Fail(report, "joint_node", joint.name, "Joint node is out of range.");
        if (!jointNodes.insert(joint.node).second) return Fail(report, "joint_duplicate", joint.name, "Joint node occurs more than once in the skin.");
        if (!Affine(joint.inverseBind)) return Fail(report, "inverse_bind", joint.name, "Inverse bind matrix must be finite and affine.");
        if (std::abs(Determinant(joint.inverseBind)) <= 1e-20)
            return Fail(report, "inverse_bind", joint.name, "Inverse bind matrix is singular.");
        if (scene.skeleton.root >= 0) {
            int ancestor = static_cast<int>(joint.node);
            while (ancestor >= 0 && ancestor != scene.skeleton.root) ancestor = scene.nodes[static_cast<std::size_t>(ancestor)].parent;
            if (ancestor < 0) return Fail(report, "skeleton_root", joint.name, "Skeleton root must be an ancestor of every joint.");
        }
    }
    std::size_t totalVertices = 0, totalIndices = 0;
    std::vector<std::size_t> degenerates(scene.meshes.size());
    for (std::size_t m = 0; m < scene.meshes.size(); ++m) {
        auto& mesh = scene.meshes[m];
        const auto context = MeshContext(scene,m);
        mesh.bounds = {};
        if (mesh.vertices.empty() || mesh.indices.empty()) return Fail(report, "mesh_empty", context, "Mesh must contain vertices and triangles.");
        if (mesh.vertices.size() > MaxElements || mesh.indices.size() > MaxElements || mesh.indices.size() % 3)
            return Fail(report, "mesh_count", context, "Mesh count exceeds 4194304 or indices are not a triangle list.");
        totalVertices += mesh.vertices.size(); totalIndices += mesh.indices.size();
        if (totalVertices > MaxElements || totalIndices > MaxElements)
            return Fail(report, "scene_geometry_budget", scene.name, "Aggregate geometry exceeds the bounded offline processing budget of 4194304 vertices or indices.");
        if (mesh.material >= scene.materials.size()) return Fail(report, "mesh_material", context, "Material index is out of range.");
        if (!mesh.hasUV && scene.materials[mesh.material].baseTexture >= 0)
            return Fail(report, "mesh_uv", context, "A textured mesh requires UV0.");
        if (mesh.hasTangents && !mesh.hasNormals) return Fail(report, "mesh_tangent", context, "Tangents require a normal channel.");
        if (mesh.skinned && scene.skeleton.joints.empty()) return Fail(report, "skin_missing", context, "Skinned mesh has no skeleton.");
        for (std::size_t v = 0; v < mesh.vertices.size(); ++v) {
            const auto& vertex = mesh.vertices[v];
            if (!Finite(vertex.position)) return Fail(report, "vertex_position", context, "Vertex " + std::to_string(v) + " has a non-finite position.");
            for (float value : vertex.position) if (std::abs(double(value)) >= MaxCoordinate)
                return Fail(report, "vertex_range", context, "Position exceeds the E1-X coordinate range in meters.");
            if (mesh.hasNormals && (!Finite(vertex.normal) || std::abs(LengthSquared(vertex.normal) - 1.0) > 1e-4))
                return Fail(report, "vertex_normal", context, "Vertex " + std::to_string(v) + " normal must be finite and normalized to unit length.");
            if (mesh.hasUV && !Finite(vertex.uv)) return Fail(report, "vertex_uv", context, "Vertex UV is non-finite.");
            if(mesh.hasVertexExtras && (!Finite(vertex.color)||!Finite(vertex.uv1)||!Finite(vertex.pivot)||!Finite(vertex.cardPitchCos)||!Finite(vertex.cardPitchSin)||
               !std::isfinite(vertex.flexibility)||vertex.flexibility<0||vertex.flexibility>1||
               std::any_of(vertex.color.begin(),vertex.color.end(),[](float f){return f<0||f>1;})))
                return Fail(report,"vertex_extras",context,"Invalid auxiliary vertex channel.");
            if (mesh.hasTangents && (!Finite(vertex.tangent) || (vertex.tangent[3] != 1 && vertex.tangent[3] != -1) ||
                std::abs(LengthSquared(Vec3{vertex.tangent[0],vertex.tangent[1],vertex.tangent[2]}) - 1.0) > 1e-4))
                return Fail(report, "vertex_tangent", context, "Tangent must have a finite unit direction and handedness of +1 or -1.");
            if (mesh.hasTangents) {
                Double3 normal{vertex.normal[0],vertex.normal[1],vertex.normal[2]};
                Double3 tangent{vertex.tangent[0],vertex.tangent[1],vertex.tangent[2]};
                Normalize(normal); Normalize(tangent);
                const auto perpendicular = Cross(normal,tangent);
                if (Dot(perpendicular,perpendicular) <= 1e-24 || std::abs(Dot(normal,tangent)) > 1e-4)
                    return Fail(report, "vertex_tangent", context, "Tangent direction must be perpendicular to the vertex normal.");
            }
            if (!mesh.skinned && !vertex.influences.empty()) return Fail(report, "skin_classification", context, "Rigid mesh contains undeclared skin influences.");
            if (mesh.skinned) {
                if (vertex.influences.empty() || vertex.influences.size() > 4)
                    return Fail(report, "skin_count", context, "Normalized skin vertices must contain one to four influences.");
                double sum = 0;
                std::set<std::uint32_t> joints;
                for (const auto& influence : vertex.influences) {
                    if (influence.joint >= scene.skeleton.joints.size()) return Fail(report, "skin_joint", context, "Skin joint index is out of range.");
                    if (!std::isfinite(influence.weight) || influence.weight < 0 || influence.weight > 1)
                        return Fail(report, "skin_weight", context, "Skin weights must be finite and in [0,1].");
                    if (!joints.insert(influence.joint).second) return Fail(report, "skin_duplicate", context, "Duplicate joint influences require normalization.");
                    sum += influence.weight;
                }
                if (std::abs(sum - 1.0) > 1e-5) return Fail(report, "skin_sum", context, "Skin weights must sum to one after normalization.");
            }
            Expand(mesh.bounds, vertex.position);
        }
        for (const auto index : mesh.indices)
            if (index >= mesh.vertices.size()) return Fail(report, "index_range", context, "Triangle index is outside the vertex array.");
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3)
            if (Degenerate(mesh.vertices[mesh.indices[i]].position, mesh.vertices[mesh.indices[i+1]].position, mesh.vertices[mesh.indices[i+2]].position))
                ++degenerates[m];
        if (!ValidBounds(mesh.bounds)) return Fail(report, "mesh_bounds", context, "Derived mesh bounds are invalid.");
    }
    std::vector<std::size_t> references(scene.meshes.size());
    std::size_t instances = 0, bakedVertices = 0, bakedIndices = 0;
    for (std::size_t n = 0; n < scene.nodes.size(); ++n) for (auto m : scene.nodes[n].meshes) {
        const auto& mesh = scene.meshes[m];
        ++references[m]; ++instances;
        bakedVertices += mesh.vertices.size(); bakedIndices += mesh.indices.size();
        if (instances > MaxObjects - scene.nodes.size() || bakedVertices > MaxElements || bakedIndices > MaxElements)
            return Fail(report, "instance_budget", NodeContext(scene,n), "Expanded node instances exceed the bounded runtime geometry budget.");
        const auto determinant = Determinant(world[n]);
        if (!std::isfinite(determinant) || std::abs(determinant) <= 1e-20)
            return Fail(report, "mesh_transform", NodeContext(scene,n), "Mesh world transform is singular.");
        for (const auto& vertex : mesh.vertices) {
            Vec3 p{};
            for (std::size_t r = 0; r < 3; ++r) {
                double component = world[n][12+r];
                for (std::size_t c = 0; c < 3; ++c) component += double(world[n][c*4+r]) * vertex.position[c];
                if (!std::isfinite(component) || std::abs(component) >= MaxCoordinate)
                    return Fail(report, "world_position", NodeContext(scene,n), "Transformed vertex exceeds the E1-X coordinate bounds.");
                p[r] = static_cast<float>(component);
            }
            Expand(scene.bounds, p);
        }
        if (degenerates[m]) report.Warn("degenerate_triangles", MeshContext(scene,m) + " at " + NodeContext(scene,n),
            std::to_string(degenerates[m]) + " degenerate triangles retained; no geometry was silently removed.");
    }
    for (std::size_t m = 0; m < references.size(); ++m)
        if (!references[m]) return Fail(report, "mesh_unreferenced", MeshContext(scene,m), "Mesh is not attached to a scene node.");
    if (!ValidBounds(scene.bounds)) return Fail(report, "scene_bounds", scene.name, "Derived model bounds are invalid.");
    for (const auto& animation : scene.animations) {
        if (animation.channels.empty() || animation.channels.size() > MaxObjects)
            return Fail(report, "animation_channels", animation.name, "Animation must contain a bounded nonempty channel set.");
        if (!std::isfinite(animation.duration) || animation.duration < 0)
            return Fail(report, "animation_duration", animation.name, "Animation duration must be finite and nonnegative seconds.");
        std::set<std::pair<std::uint32_t, AnimationPath>> targets;
        std::size_t keys = 0;
        float endpoint = 0;
        for (const auto& channel : animation.channels) {
            if (channel.node >= scene.nodes.size()) return Fail(report, "animation_node", animation.name, "Channel node index is out of range.");
            if (!scene.nodes[channel.node].useTRS) return Fail(report, "animation_matrix", animation.name, "Animated nodes must export TRS rather than a matrix.");
            if (channel.path != AnimationPath::Translation && channel.path != AnimationPath::Rotation && channel.path != AnimationPath::Scale)
                return Fail(report, "animation_path", animation.name, "Unsupported animation path.");
            if (!targets.insert({channel.node, channel.path}).second)
                return Fail(report, "animation_duplicate", animation.name, "A node/path may only have one channel per clip.");
            if (channel.times.empty() || channel.times.size() != channel.values.size() || channel.times.size() > MaxElements)
                return Fail(report, "animation_keys", animation.name, "Channel needs matching nonempty time/value arrays within the key limit.");
            keys += channel.times.size();
            if (keys > MaxElements) return Fail(report, "animation_budget", animation.name, "Clip key count exceeds 4194304.");
            float previous = -1;
            for (std::size_t k = 0; k < channel.times.size(); ++k) {
                const auto time = channel.times[k];
                if (!std::isfinite(time) || time < 0 || time <= previous)
                    return Fail(report, "animation_time", animation.name, "Key times in seconds must be finite, nonnegative and strictly increasing as Float32.");
                previous = time;
                if (!Finite(channel.values[k])) return Fail(report, "animation_value", animation.name, "Channel contains non-finite key values.");
                if (channel.path == AnimationPath::Rotation && !Quaternion(channel.values[k]))
                    return Fail(report, "animation_rotation", animation.name, "Rotation keys must be normalized nonzero quaternions.");
            }
            endpoint = std::max(endpoint, channel.times.back());
        }
        if (animation.duration < endpoint) return Fail(report, "animation_duration", animation.name, "Clip duration cannot be shorter than its final key time.");
    }
    return true;
}

bool Process(Scene& scene, const Options& options, Report& report)
{
    if (!report.Ok()) return false;
    if (!Preflight(scene, report) || !NormalizeWeights(scene, report) || !NormalizeRotations(scene, report) ||
        !GenerateAttributes(scene, options, report) || !NormalizeDirections(scene, report) || !Validate(scene, report)) return false;
    if (!PreserveAnimationDuration(scene, report)) return false;
    scene.optimization = {};
    double weightedBefore = 0, weightedAfter = 0, triangles = 0;
    for (std::size_t m = 0; m < scene.meshes.size(); ++m) {
        auto& mesh = scene.meshes[m];
        scene.optimization.verticesBefore += mesh.vertices.size();
        scene.optimization.indicesBefore += mesh.indices.size();
        if (!mesh.skinned) {
            const double triangleCount = static_cast<double>(mesh.indices.size() / 3);
            weightedBefore += CacheRatio(mesh) * triangleCount;
            if (options.optimize) {
                const bool blend = scene.materials[mesh.material].alpha == AlphaMode::Blend;
                OptimizeMesh(mesh, !blend);
                if (blend) report.Warn("blend_order", MeshContext(scene,m), "Vertex fetch optimized with the original triangle order preserved for alpha blending.");
            }
            weightedAfter += CacheRatio(mesh) * triangleCount;
            triangles += triangleCount;
        }
        scene.optimization.verticesAfter += mesh.vertices.size();
        scene.optimization.indicesAfter += mesh.indices.size();
    }
    if (triangles) {
        scene.optimization.cacheMissRatioBefore = weightedBefore / triangles;
        scene.optimization.cacheMissRatioAfter = weightedAfter / triangles;
    }
    if (scene.optimization.verticesBefore != scene.optimization.verticesAfter)
        report.Warn("unused_vertices", scene.name, std::to_string(scene.optimization.verticesBefore - scene.optimization.verticesAfter) + " unreferenced vertices removed during fetch compaction; all triangle attributes are preserved.");
    Report verification;
    if (!Validate(scene, verification)) {
        for (auto& issue : verification.issues) if (issue.error) report.issues.push_back(std::move(issue));
        return false;
    }
    return true;
}

bool MakeStaticLOD(const Scene& source, float ratio, Scene& lod, Report& report)
{
    if (!report.Ok()) return false;
    if (!std::isfinite(ratio) || ratio <= 0 || ratio >= 1)
        return Fail(report, "lod_ratio", source.name, "LOD triangle ratio must be finite and strictly between zero and one.");
    if (!source.skeleton.joints.empty() || !source.animations.empty())
        return Fail(report, "lod_type", source.name, "E2-X LOD preparation only supports static assets without skin or animation.");
    lod = source;
    if (!Validate(lod, report)) return false;
    for (std::size_t m = 0; m < lod.meshes.size(); ++m) {
        auto& mesh = lod.meshes[m];
        if (mesh.skinned || lod.materials[mesh.material].alpha != AlphaMode::Opaque)
            return Fail(report, "lod_material", MeshContext(lod,m), "LOD simplification is limited to opaque static geometry; alpha geometry is preserved at LOD0.");
        std::vector<Vec3> positions;
        positions.reserve(mesh.vertices.size());
        for (const auto& vertex : mesh.vertices) positions.push_back(vertex.position);
        const auto originalCount = mesh.indices.size();
        const auto target = std::max<std::size_t>(3, static_cast<std::size_t>(double(originalCount / 3) * ratio) * 3);
        std::vector<unsigned int> indices(originalCount);
        float error = 0;
        // ZiiNAN: Static-only LOD preparation preserves attribute seams and never changes LOD0.
        const auto count = meshopt_simplify(indices.data(), mesh.indices.data(), originalCount,
            positions.front().data(), positions.size(), sizeof(Vec3), target, 0.01f, meshopt_SimplifyLockBorder, &error);
        if (!count || count % 3) return Fail(report, "lod_simplify", MeshContext(lod,m), "Simplification returned an invalid triangle count.");
        indices.resize(count);
        mesh.indices = std::move(indices);
        if (count > target) report.Warn("lod_target", MeshContext(lod,m), "Requested triangle ratio could not be reached within the 1% relative geometric error and locked-border policy.");
        report.Warn("lod_result", MeshContext(lod,m), std::to_string(originalCount / 3) + " -> " + std::to_string(count / 3) +
            " triangles, normalized geometric error " + std::to_string(error) + "; generated LOD needs visual review.");
    }
    Options options;
    options.optimize = true;
    return Process(lod, options, report);
}
}
