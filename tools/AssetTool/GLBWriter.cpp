#include "Scene.h"
#include "AssetRuntime/GlTF/GlTFAssetProvider.h"
#define CGLTF_WRITE_IMPLEMENTATION
#include "cgltf_write.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <deque>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>

namespace ZiiNAN::AssetTool {
namespace {
struct Writer {
    cgltf_data data{};
    cgltf_buffer buffer{};
    cgltf_scene scene{};
    cgltf_skin skin{};
    std::vector<cgltf_node> nodes;
    std::vector<std::vector<cgltf_node*>> children;
    std::vector<cgltf_node*> roots, joints;
    std::vector<cgltf_mesh> meshes;
    std::vector<cgltf_primitive> primitives;
    std::vector<std::vector<cgltf_attribute>> attributes;
    std::vector<cgltf_material> materials;
    std::vector<cgltf_image> images;
    std::vector<cgltf_texture> textures;
    std::vector<cgltf_accessor> accessors;
    std::vector<cgltf_buffer_view> views;
    std::vector<cgltf_animation> animations;
    std::vector<std::vector<cgltf_animation_channel>> channels;
    std::vector<std::vector<cgltf_animation_sampler>> samplers;
    std::vector<std::uint8_t> binary;
    std::deque<std::string> strings;

    char* Name(const std::string& value) { strings.push_back(EscapeJSON(value)); return strings.back().data(); }
    cgltf_buffer_view* View(const void* bytes, std::size_t size)
    {
        if (views.size() >= MaxObjects || binary.size() > MaxFileBytes - 4 || size > MaxFileBytes - binary.size() - 4)
            throw std::runtime_error("GLB buffer or buffer-view budget exceeded");
        while (binary.size()%4) binary.push_back(0);
        const auto offset=binary.size(); binary.resize(offset+size);
        if (size) std::memcpy(binary.data()+offset,bytes,size);
        views.emplace_back(); auto& view=views.back();
        view.buffer=&buffer; view.offset=offset; view.size=size;
        return &view;
    }
    cgltf_accessor* Accessor(const void* bytes, std::size_t count, cgltf_type type, cgltf_component_type component, std::size_t stride)
    {
        if (accessors.size()>=MaxObjects || count>MaxElements || count>MaxFileBytes/stride)
            throw std::runtime_error("GLB accessor budget exceeded");
        accessors.emplace_back(); auto& a=accessors.back();
        a.type=type; a.component_type=component; a.count=count; a.stride=stride;
        a.buffer_view=View(bytes,count*stride); return &a;
    }
    void Fill(const Scene& src)
    {
        accessors.reserve(MaxObjects); views.reserve(MaxObjects);
        data.asset.version=const_cast<char*>("2.0"); data.asset.generator=Name(std::string("ZiiNAN Asset Tool ")+Version);
        std::size_t attachments=0;
        for (const auto& n:src.nodes) attachments+=n.meshes.size();
        if (src.nodes.size()+attachments>MaxObjects) throw std::runtime_error("Export node budget exceeded");
        nodes.resize(src.nodes.size()+attachments); children.resize(nodes.size());
        meshes.resize(src.meshes.size()); primitives.resize(meshes.size()); attributes.resize(meshes.size());
        materials.resize(src.materials.size()); images.resize(src.images.size()); textures.resize(src.images.size());
        for (std::size_t i=0;i<images.size();++i) {
            const auto& input=src.images[i];
            images[i].name=Name(input.name); images[i].mime_type=Name(input.mime);
            images[i].buffer_view=View(input.bytes.data(),input.bytes.size()); textures[i].image=&images[i];
        }
        for (std::size_t i=0;i<materials.size();++i) {
            const auto& input=src.materials[i]; auto& m=materials[i];
            m.name=Name(input.name); m.has_pbr_metallic_roughness=true;
            std::copy(input.baseColor.begin(),input.baseColor.end(),m.pbr_metallic_roughness.base_color_factor);
            m.pbr_metallic_roughness.metallic_factor=0; m.pbr_metallic_roughness.roughness_factor=1;
            if (input.baseTexture>=0) m.pbr_metallic_roughness.base_color_texture.texture=&textures.at(input.baseTexture);
            m.alpha_mode=input.alpha==AlphaMode::Mask ? cgltf_alpha_mode_mask : input.alpha==AlphaMode::Blend ? cgltf_alpha_mode_blend : cgltf_alpha_mode_opaque;
            m.alpha_cutoff=input.alphaCutoff; m.double_sided=input.doubleSided;
        }
        for (std::size_t i=0;i<meshes.size();++i) {
            const auto& input=src.meshes[i]; auto& mesh=meshes[i]; auto& p=primitives[i]; auto& attrs=attributes[i];
            mesh.name=Name(input.name); mesh.primitives=&p; mesh.primitives_count=1;
            p.type=cgltf_primitive_type_triangles; p.material=&materials.at(input.material);
            p.indices=Accessor(input.indices.data(),input.indices.size(),cgltf_type_scalar,cgltf_component_type_r_32u,4);
            attrs.reserve(6);
            auto add=[&](const char* name,cgltf_attribute_type type,cgltf_accessor* accessor) {
                cgltf_attribute a{}; a.name=const_cast<char*>(name); a.type=type; a.data=accessor; attrs.push_back(a);
            };
            std::vector<Vec3> positions,normals; std::vector<Vec2> uv; std::vector<Vec4> tangent,weights;
            std::vector<std::array<std::uint16_t,4>> jointIndices;
            for (const auto& v:input.vertices) {
                positions.push_back(v.position); normals.push_back(v.normal); uv.push_back(v.uv); tangent.push_back(v.tangent);
                if (input.skinned) {
                    std::array<std::uint16_t,4> j{}; Vec4 w{};
                    for(std::size_t k=0;k<v.influences.size();++k) { j.at(k)=static_cast<std::uint16_t>(v.influences[k].joint); w.at(k)=v.influences[k].weight; }
                    jointIndices.push_back(j); weights.push_back(w);
                }
            }
            auto* pos=Accessor(positions.data(),positions.size(),cgltf_type_vec3,cgltf_component_type_r_32f,12);
            pos->has_min=pos->has_max=true;
            std::copy(input.bounds.min.begin(),input.bounds.min.end(),pos->min); std::copy(input.bounds.max.begin(),input.bounds.max.end(),pos->max);
            add("POSITION",cgltf_attribute_type_position,pos);
            if(input.hasNormals) add("NORMAL",cgltf_attribute_type_normal,Accessor(normals.data(),normals.size(),cgltf_type_vec3,cgltf_component_type_r_32f,12));
            if(input.hasUV) add("TEXCOORD_0",cgltf_attribute_type_texcoord,Accessor(uv.data(),uv.size(),cgltf_type_vec2,cgltf_component_type_r_32f,8));
            if(input.hasTangents) add("TANGENT",cgltf_attribute_type_tangent,Accessor(tangent.data(),tangent.size(),cgltf_type_vec4,cgltf_component_type_r_32f,16));
            if(input.skinned) {
                add("JOINTS_0",cgltf_attribute_type_joints,Accessor(jointIndices.data(),jointIndices.size(),cgltf_type_vec4,cgltf_component_type_r_16u,8));
                add("WEIGHTS_0",cgltf_attribute_type_weights,Accessor(weights.data(),weights.size(),cgltf_type_vec4,cgltf_component_type_r_32f,16));
            }
            p.attributes=attrs.data(); p.attributes_count=attrs.size();
        }
        std::size_t childIndex=src.nodes.size();
        for(std::size_t i=0;i<src.nodes.size();++i) {
            const auto& input=src.nodes[i]; auto& n=nodes[i]; n.name=Name(input.name);
            if(input.useTRS) {
                n.has_translation=n.has_rotation=n.has_scale=true;
                std::copy(input.translation.begin(),input.translation.end(),n.translation);
                std::copy(input.rotation.begin(),input.rotation.end(),n.rotation);
                std::copy(input.scale.begin(),input.scale.end(),n.scale);
            } else { n.has_matrix=true; std::copy(input.transform.begin(),input.transform.end(),n.matrix); }
            if(input.parent<0) roots.push_back(&n);
            else { n.parent=&nodes.at(input.parent); children.at(input.parent).push_back(&n); }
            for(const auto meshIndex:input.meshes) {
                auto& child=nodes[childIndex++]; child.parent=&n; child.mesh=&meshes.at(meshIndex);
                child.name=Name(src.meshes[meshIndex].name); children[i].push_back(&child);
                if(src.meshes[meshIndex].skinned) child.skin=&skin;
            }
        }
        for(std::size_t i=0;i<nodes.size();++i) { nodes[i].children=children[i].data(); nodes[i].children_count=children[i].size(); }
        if(!src.skeleton.joints.empty()) {
            std::vector<Matrix> binds;
            for(const auto& j:src.skeleton.joints) { joints.push_back(&nodes.at(j.node)); binds.push_back(j.inverseBind); }
            skin.joints=joints.data(); skin.joints_count=joints.size();
            if(src.skeleton.root>=0) skin.skeleton=&nodes.at(src.skeleton.root);
            skin.inverse_bind_matrices=Accessor(binds.data(),binds.size(),cgltf_type_mat4,cgltf_component_type_r_32f,64);
            data.skins=&skin; data.skins_count=1;
        }
        animations.resize(src.animations.size()); channels.resize(animations.size()); samplers.resize(animations.size());
        for(std::size_t i=0;i<animations.size();++i) {
            const auto& input=src.animations[i]; auto& a=animations[i]; a.name=Name(input.name);
            channels[i].resize(input.channels.size()); samplers[i].resize(input.channels.size());
            for(std::size_t c=0;c<input.channels.size();++c) {
                const auto& ci=input.channels[c]; auto& co=channels[i][c]; auto& sampler=samplers[i][c];
                co.target_node=&nodes.at(ci.node); co.sampler=&sampler;
                co.target_path=ci.path==AnimationPath::Rotation ? cgltf_animation_path_type_rotation : ci.path==AnimationPath::Scale ? cgltf_animation_path_type_scale : cgltf_animation_path_type_translation;
                sampler.interpolation=cgltf_interpolation_type_linear;
                sampler.input=Accessor(ci.times.data(),ci.times.size(),cgltf_type_scalar,cgltf_component_type_r_32f,4);
                sampler.input->has_min=sampler.input->has_max=true; sampler.input->min[0]=ci.times.front(); sampler.input->max[0]=ci.times.back();
                if(ci.path==AnimationPath::Rotation) sampler.output=Accessor(ci.values.data(),ci.values.size(),cgltf_type_vec4,cgltf_component_type_r_32f,16);
                else {
                    std::vector<Vec3> v; for(const auto& key:ci.values) v.push_back({key[0],key[1],key[2]});
                    sampler.output=Accessor(v.data(),v.size(),cgltf_type_vec3,cgltf_component_type_r_32f,12);
                }
            }
            a.channels=channels[i].data(); a.channels_count=channels[i].size(); a.samplers=samplers[i].data(); a.samplers_count=samplers[i].size();
        }
        buffer.size=binary.size();
        data.buffers=&buffer; data.buffers_count=1;
        data.buffer_views=views.data(); data.buffer_views_count=views.size(); data.accessors=accessors.data(); data.accessors_count=accessors.size();
        data.meshes=meshes.data(); data.meshes_count=meshes.size(); data.nodes=nodes.data(); data.nodes_count=nodes.size();
        data.materials=materials.data(); data.materials_count=materials.size(); data.images=images.data(); data.images_count=images.size();
        data.textures=textures.data(); data.textures_count=textures.size(); data.animations=animations.data(); data.animations_count=animations.size();
        scene.nodes=roots.data(); scene.nodes_count=roots.size(); data.scenes=&scene; data.scenes_count=1; data.scene=&scene;
    }
    std::vector<std::uint8_t> Bytes()
    {
        cgltf_options options{};
        const auto size=cgltf_write(&options,nullptr,0,&data);
        if(!size || size>MaxFileBytes-32 || binary.size()>MaxFileBytes-size-32) throw std::runtime_error("GLB output exceeds 256 MiB");
        std::vector<char> json(size);
        if(cgltf_write(&options,json.data(),json.size(),&data)!=size) throw std::runtime_error("cgltf writer size mismatch");
        json.pop_back(); while(json.size()%4) json.push_back(' '); while(binary.size()%4) binary.push_back(0);
        // cgltf owns JSON serialization; this bounded envelope uses filesystem-native Unicode I/O.
        std::vector<std::uint8_t> result;
        auto u32=[&](std::uint32_t n) { for(unsigned shift=0;shift<32;shift+=8) result.push_back(std::uint8_t(n>>shift)); };
        u32(0x46546c67); u32(2); u32(static_cast<std::uint32_t>(28+json.size()+binary.size()));
        u32(static_cast<std::uint32_t>(json.size())); u32(0x4e4f534a); result.insert(result.end(),json.begin(),json.end());
        u32(static_cast<std::uint32_t>(binary.size())); u32(0x004e4942); result.insert(result.end(),binary.begin(),binary.end());
        return result;
    }
};
}
bool WriteGLB(const Scene& source, const std::filesystem::path& path, Report& report)
{
    try {
        if constexpr(std::endian::native!=std::endian::little) throw std::runtime_error("Big-endian export is not supported");
        Scene checked=source; Report validation;
        if(!Validate(checked,validation)) { report.issues.insert(report.issues.end(),validation.issues.begin(),validation.issues.end()); return false; }
        Writer writer; writer.Fill(checked); const auto bytes=writer.Bytes();
        auto loaded=AssetRuntime::GetGlTFAssetProvider().Load("offline-output.glb",std::as_bytes(std::span<const std::uint8_t>(bytes)));
        if(!loaded) { report.Error("runtime-glb",PathUTF8(path),loaded.diagnostic); return false; }
        std::ofstream stream(path,std::ios::binary|std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())); stream.close();
        if(!stream) { report.Error("output",PathUTF8(path),"Cannot write the complete GLB output."); return false; }
        return true;
    } catch(const std::exception& error) { report.Error("export",PathUTF8(path),error.what()); return false; }
}
}
