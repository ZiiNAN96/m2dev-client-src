#pragma once
#include "AssetRuntime/AssetRuntime.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <stdexcept>

// The verifier only links the native reader. Reference generation is a separate,
// explicitly invoked development executable; a test run cannot rewrite goldens.
namespace GR2Golden
{
using namespace AssetRuntime;
inline void Check(bool ok, const std::string& message)
{
    if (!ok) throw std::runtime_error(message);
}
struct Records
{
    std::istream* input{};
    std::ostream* output{};
    std::size_t count{};
    void Text(const std::string& actual)
    {
        ++count;
        if (output) *output << std::quoted(actual) << '\n';
        else {
            std::string expected;
            Check(bool(*input >> std::quoted(expected)), "golden truncated at " + std::to_string(count));
            Check(expected == actual, "golden record " + std::to_string(count) + ": " + actual + " != " + expected);
        }
    }
    template<class T> void Exact(T value) { Text(std::to_string(value)); }
    void Floats(std::span<const float> values, double tolerance)
    {
        for (float actual : values) {
            ++count;
            Check(std::isfinite(actual), "nonfinite actual at " + std::to_string(count));
            if (output) *output << std::setprecision(9) << actual << ' ';
            else {
                float expected{};
                Check(bool(*input >> expected) && std::isfinite(expected), "invalid golden float");
                Check(std::abs(double(actual) - expected) <= tolerance,
                    "golden float " + std::to_string(count) + " delta=" + std::to_string(std::abs(double(actual)-expected)));
            }
        }
        if (output) *output << '\n';
    }
    void Finish()
    {
        if (output) { output->flush(); Check(bool(*output), "golden write"); }
        else { *input >> std::ws; Check(input->eof(), "unconsumed golden records"); }
    }
};
inline std::vector<std::byte> Bytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    Check(bool(input), "missing fixture " + path.string());
    const auto size=input.tellg(); Check(size>0 && size<128*1024*1024, "fixture size");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0); input.read(reinterpret_cast<char*>(bytes.data()), size);
    Check(bool(input), "fixture read"); return bytes;
}
inline std::uint64_t Hash(std::span<const std::byte> bytes)
{
    std::uint64_t hash=14695981039346656037ull;
    for (auto b:bytes) { hash ^= std::to_integer<unsigned char>(b); hash *= 1099511628211ull; }
    return hash;
}
inline AssetHandle Load(AssetProvider& provider, const std::filesystem::path& root, const std::string& path)
{
    auto result=LoadModel(path, Bytes(root/path), provider);
    Check(bool(result), path + ": " + result.diagnostic); return std::move(result.asset);
}
inline constexpr const char* models[]{
    "PC/ymir work/pc/warrior/warrior_novice.gr2",
    "PC/ymir work/pc/warrior/hair/hair_1_1.gr2",
    "item/ymir work/item/weapon/00010.gr2",
    "Monster/ymir work/monster/wolf/wolf.gr2",
    "Monster/ymir work/monster/misterious_diseased_bosshost/misterious_diseased_bosshost.gr2",
    "NPC/ymir work/npc/horse/horse_normal.gr2",
    "Zone/ymir work/zone/n/obj/snow.m/snow-004-house2.gr2",
    "guild/ymir work/guild/facility/gongjakso/gongjakso.gr2"
};
inline void Static(Records& r, AssetProvider& provider, const std::filesystem::path& root)
{
    r.Text("F34-static-v1 FNV1a64 little-endian streams");
    for (const auto* path:models) {
        r.Text(path); r.Exact(Hash(Bytes(root/path)));
        auto asset=Load(provider,root,path);
        r.Exact(asset.ModelCount()); r.Exact(asset.AnimationCount());
        for (std::size_t m=0;m<asset.ModelCount();++m) {
            const auto& model=*asset.Model(m).Get();
            r.Text(model.name); r.Exact(model.meshes.size()); r.Exact(model.materials.size());
            r.Exact(int(model.deformation)); r.Exact(bool(model.skeleton));
            if (model.skeleton) {
                const auto& bones=model.skeleton->bones;
                r.Text(model.skeleton->name); r.Exact(bones.size());
                for (std::size_t b=0;b<bones.size();++b) {
                    const auto& bone=bones[b]; r.Text(bone.name); r.Exact(bone.id); r.Exact(bone.parentIndex);
                    if (b==0 || b==bones.size()/2 || b==bones.size()-1) {
                        r.Exact(bone.localBind.flags); r.Floats(bone.localBind.position,1e-6);
                        r.Floats(bone.localBind.orientation,1e-6); r.Floats(bone.localBind.scaleShear,1e-6);
                        r.Floats(bone.inverseBind,1e-6);
                    }
                }
            }
            for (const auto& material:model.materials) {
                r.Text(material.name); for(const auto& texture:material.textures) r.Text(texture);
                r.Exact(int(material.stage)); r.Exact(int(material.culling));
            }
            for (std::size_t i=0;i<model.meshes.size();++i) {
                const auto& mesh=model.meshes[i]; r.Text(mesh.name);
                r.Exact(mesh.vertexCount); r.Exact(mesh.indexCount); r.Exact(mesh.sourceVertexStride);
                r.Exact(int(mesh.vertexLayout)); r.Exact(int(mesh.deformation)); r.Exact(int(mesh.indexWidth));
                r.Exact(mesh.skin.boneNames.size());
                for (std::size_t b=0;b<mesh.skin.boneNames.size();++b) {
                    r.Text(mesh.skin.boneNames[b]); r.Exact(mesh.skin.meshToSkeleton.at(b));
                }
                std::vector<std::byte> vertices(mesh.vertexCount*VertexStride(mesh.vertexLayout));
                std::vector<std::byte> indices(mesh.indexCount*4);
                Check(asset.Get()->CopyVertices(m,i,mesh.vertexLayout,vertices)==AssetError::None,"golden vertex copy");
                Check(asset.Get()->CopyIndices(m,i,IndexWidth::UInt32,indices)==AssetError::None,"golden index copy");
                r.Exact(Hash(vertices)); r.Exact(Hash(indices));
                r.Exact(mesh.materialGroups.size());
                for (const auto& group:mesh.materialGroups) {
                    r.Exact(group.materialIndex); r.Exact(group.firstIndex); r.Exact(group.indexCount);
                }
            }
        }
    }
}
inline void Animation(Records& r, AssetProvider& provider, const std::filesystem::path& root)
{
    r.Text("F34-animation-v1 F1-X world/palette=2e-3 position=5e-3 normal=5e-5");
    struct Clip { unsigned model; const char* path; bool finite; };
    const Clip clips[]{
        {0,"PC/ymir work/pc/warrior/general/wait.gr2",false},
        {0,"PC/ymir work/pc/warrior/general/walk.gr2",false},
        {0,"PC/ymir work/pc/warrior/general/run.gr2",false},
        {0,"PC/ymir work/pc/warrior/general/attack.gr2",true},
        {3,"Monster/ymir work/monster/wolf/03.gr2",false},
        {4,"Monster/ymir work/monster/misterious_diseased_bosshost/20.gr2",false},
        {5,"NPC/ymir work/npc/horse/03.gr2",false}
    };
    for(const auto& spec:clips) {
        r.Text(models[spec.model]); r.Text(spec.path); r.Exact(Hash(Bytes(root/spec.path))); r.Exact(spec.finite);
        auto model=Load(provider,root,models[spec.model]),clip=Load(provider,root,spec.path);
        const auto& meta=*clip.Animation(0).Get(); r.Text(meta.name);
        r.Floats(std::span<const float>(&meta.duration,1),1e-7); r.Exact(meta.trackGroupCount);
        auto instance=model.Get()->CreateAnimationInstance(model.Model(0)); Check(bool(instance),"golden animation instance");
        Check(instance->SetMotion(clip.Animation(0),0,0,spec.finite?1:0,1)==AssetError::None,"golden motion bind");
        Check(instance->PreparePose(),"golden initial pose");
        const auto& source=*model.Model(0).Get(); const auto boneCount=source.skeleton->bones.size(); r.Exact(boneCount);
        for(float fraction:{0.f,.25f,.5f,.75f,1.f,1.0001f}) {
            r.Floats(std::span<const float>(&fraction,1),0);
            instance->SetClock(meta.duration*fraction);
            auto result=instance->Evaluate({}); Check(result.error==AssetError::None && result.pose.Valid(),"golden pose evaluation");
            const auto palette=instance->CompositePose().values;
            Check(palette.size()==boneCount*16,"golden palette size");
            for(std::size_t b:{std::size_t(0),boneCount/2,boneCount-1}) {
                r.Floats(instance->BoneWorldMatrix(static_cast<BoneId>(b)),2e-3);
                r.Floats(palette.subspan(b*16,16),2e-3);
            }
            for(std::size_t m=0;m<source.meshes.size();++m) {
                const auto& mesh=source.meshes[m];
                if(mesh.deformation!=Deformation::Skinned || !mesh.vertexCount) continue;
                std::vector<std::byte> vertices(mesh.vertexCount*40);
                Check(model.Get()->CopyVertices(0,m,VertexLayout::WeightedPositionNormalUV,vertices)==AssetError::None,"golden skin vertices");
                for(std::size_t v:{std::size_t(0),std::size_t(mesh.vertexCount/2),std::size_t(mesh.vertexCount-1)}) {
                    const auto* data=vertices.data()+v*40; float p[3],n[3];
                    std::memcpy(p,data,12); std::memcpy(n,data+20,12); std::array<float,3> position{},normal{};
                    for(unsigned k=0;k<4;++k) {
                        const auto weight=std::to_integer<unsigned char>(data[12+k]); if(!weight) continue;
                        const auto joint=mesh.skin.meshToSkeleton.at(std::to_integer<unsigned char>(data[16+k]));
                        Check(joint>=0 && std::size_t(joint)<boneCount,"golden vertex joint");
                        const auto mat=palette.subspan(joint*16,16); const float w=weight/255.f;
                        for(unsigned c=0;c<3;++c) {
                            position[c]+=w*(p[0]*mat[c]+p[1]*mat[4+c]+p[2]*mat[8+c]+mat[12+c]);
                            normal[c]+=w*(n[0]*mat[c]+n[1]*mat[4+c]+n[2]*mat[8+c]);
                        }
                    }
                    r.Floats(position,5e-3); r.Floats(normal,5e-5);
                }
            }
        }
    }
}
}
