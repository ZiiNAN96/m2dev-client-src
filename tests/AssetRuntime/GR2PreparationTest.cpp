#include "GR2Fixtures.h"
#include "GR2Golden.h"
#include "AssetRuntime/GR2/GR2Preparation.h"
#include "AssetRuntime/GR2/GR2AssetProvider.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include <atomic>
#include <sstream>

using namespace AssetRuntime;
using GR2Golden::Check;
namespace {
// Exact field serialization; never compare padding, pointer values or hashes.
struct Snapshot {
    std::string bytes;
    template<class T> void Value(const T& value) {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>);
        bytes.append(reinterpret_cast<const char*>(&value),sizeof(value));
    }
    void Value(const std::string& value) { Value(value.size()); bytes.append(value); }
    template<class T,std::size_t N> void Value(const std::array<T,N>& value) { for(const auto& x:value) Value(x); }
    template<class T> void Value(const std::vector<T>& value) { Value(value.size()); for(const auto& x:value) Value(x); }
    void Value(const LocalTransform& x) { Value(x.flags); Value(x.position); Value(x.orientation); Value(x.scaleShear); }
    void Value(const AnimationRuntime::LocalTransform& x) { Value(x.translation); Value(x.rotation); Value(x.scaleShear); }
    void Value(const Bounds& x) { Value(x.min); Value(x.max); Value(x.valid); }
    void Value(const GR2::Curve& x) { Value(x.degree); Value(x.dimension); Value(x.knots); Value(x.controls); }
    void Value(const GR2::Vertex& x) { Value(x.position); Value(x.weights); Value(x.joints); Value(x.normal); Value(x.uv); Value(x.uv1); }
    explicit Snapshot(const GR2::Contents& c) {
        Value(c.models.size()); Value(c.animations.size()); Value(c.modelData.size()); Value(c.animationData.size());
        for(const auto& m:c.models) {
            Value(m.name); Value(m.animations); Value(m.deformation); Value(m.renderable); Value(m.preferredIndexWidth);
            Value(bool(m.skeleton));
            if(m.skeleton) {
                const auto& s=*m.skeleton; Value(s.name); Value(s.rootIndex); Value(s.bones.size());
                for(const auto& b:s.bones) { Value(b.id); Value(b.parentIndex); Value(b.name); Value(b.localBind); Value(b.inverseBind); }
            }
            Value(m.materials.size());
            for(const auto& x:m.materials) {
                Value(x.name); Value(x.textures); Value(x.matchingTextures); Value(x.hasMatchingTextures);
                Value(x.stage); Value(x.culling); Value(x.alphaTest); Value(x.blending); Value(x.depthWrite);
                Value(x.specular); Value(x.specularPower); Value(x.sphereMapIndex);
                Value(x.baseColorFactor); Value(x.alphaCutoff); Value(x.explicitRenderState); Value(x.model);
                Value(x.roughness); Value(x.metallic); Value(x.normalScale); Value(x.occlusionStrength);
                Value(x.emissiveColor); Value(x.alphaMode); Value(x.doubleSided);
            }
            Value(m.meshes.size());
            for(const auto& x:m.meshes) {
                Value(x.name); Value(x.vertexCount); Value(x.indexCount); Value(x.sourceVertexStride); Value(x.vertexAttributes);
                Value(x.topology); Value(x.indexWidth); Value(x.vertexLayout); Value(x.deformation); Value(x.bounds);
                Value(x.materialBindings); Value(x.materialGroups.size());
                for(const auto& g:x.materialGroups) { Value(g.materialIndex); Value(g.firstIndex); Value(g.indexCount); }
                const auto& s=x.skin; Value(s.boneNames); Value(s.meshToSkeleton); Value(s.boneBounds);
                Value(s.influencesPerVertex); Value(s.weightOffset); Value(s.indexOffset);
                Value(s.normalizedByteWeights); Value(s.byteBoneIndices); Value(s.validRemap); Value(s.jointIndices); Value(s.jointWeights);
                Value(x.twoSided); Value(x.tangents); Value(x.vertexExtrasChannels);
            }
        }
        for(const auto& m:c.modelData) {
            Value(m.initialPlacement); Value(m.meshes.size());
            for(const auto& mesh:m.meshes) { Value(mesh.vertices); Value(mesh.indices); }
            Value(bool(m.skeleton));
            if(m.skeleton) {
                Value(m.skeleton->BindingId()); Value(m.skeleton->Bones().size());
                for(const auto& b:m.skeleton->Bones()) { Value(b.name); Value(b.parent); Value(b.localBind); Value(b.inverseBind); }
                for(auto index:m.skeleton->EvaluationOrder()) Value(index);
            }
        }
        for(const auto& a:c.animations) {
            Value(a.name); Value(a.duration); Value(a.timeStep); Value(a.trackGroupCount); Value(a.metadataOnly);
            Value(a.textEvents.size()); for(const auto& e:a.textEvents) { Value(e.text); Value(e.time); }
            Value(a.channels.size()); for(const auto& x:a.channels) {
                Value(x.targetNode); Value(x.targetName); Value(x.path); Value(x.interpolation); Value(x.keyframeCount);
            }
        }
        for(const auto& a:c.animationData) {
            Value(a.groups.size());
            for(const auto& g:a.groups) {
                Value(g.name); Value(g.initialPlacement); Value(g.accumulationFlags); Value(g.loopTranslation); Value(bool(g.periodicLoop));
                if(g.periodicLoop) { const auto& p=*g.periodicLoop; Value(p.radius); Value(p.dAngle); Value(p.dZ); Value(p.basisX); Value(p.basisY); Value(p.axis); }
                Value(g.tracks.size()); for(const auto& t:g.tracks) { Value(t.name); Value(t.translation); Value(t.rotation); Value(t.scale); }
            }
        }
        for(const auto* map:{&c.vertexFormats,&c.curveFormats}) { Value(map->size()); for(const auto& [k,v]:*map) { Value(k); Value(v); } }
    }
};
auto Execute=[](std::function<void()> work) { return std::async(std::launch::async,std::move(work)); };
void Safety() {
    const auto bytes=GR2Fixtures::Builder{}.Bytes();
    for(unsigned run=0;run<4;++run) {
        GR2::Preparation batch;
        std::size_t reads=0;
        for(unsigned i=0;i<24;++i) {
            const auto id="fixture/"+std::to_string(i)+".gr2";
            batch.Add(id,[&] { ++reads; return bytes; });
            batch.Add("FIXTURE\\"+std::to_string(i)+".GR2",[&]() -> std::vector<std::byte> { throw std::runtime_error("duplicate read"); });
        }
        auto invalid=bytes; invalid.resize(invalid.size()/2);
        batch.Add("invalid.gr2",[&] { return invalid; });
        auto malformed=bytes; malformed[0]=std::byte{};
        batch.Add("malformed.gr2",[&] { return malformed; });
        batch.Run(4,Execute);
        Check(reads==24,"single flight input reads");
        const auto reference=Snapshot(GR2::Read(GR2::File(bytes))).bytes;
        for(unsigned i=0;i<24;++i) {
            auto result=GR2::Preparation::Take("fixture/"+std::to_string(i)+".gr2");
            Check(result && !result->error && Snapshot(result->contents).bytes==reference,"deterministic repeated batch");
        }
        for(const auto* id:{"invalid.gr2","malformed.gr2"}) {
            auto result=GetGR2AssetProvider().Load(id,GR2::Preparation::Payload(id));
            Check(!result && result.error==AssetError::InvalidAsset && !result.diagnostic.empty(),"worker error propagation");
        }
    }
    {
        GR2::Preparation batch; batch.Add("valid.gr2",[&]{return bytes;}); batch.Add("second.gr2",[&]{return bytes;});
        unsigned submissions=0; std::atomic_uint ended{}; bool failed=false;
        try { batch.Run(4,[&](std::function<void()> task) {
            if(++submissions==2) throw std::runtime_error("injected submission failure");
            return Execute([&,task=std::move(task)] { task(); ++ended; });
        }); } catch(const std::runtime_error&) { failed=true; }
        // On very small CPUs the batch legitimately submits only one lane.
        Check((submissions==1 || failed) && ended==1,"submission failure joins existing task");
    }
    AnimationStallAudit::enabled=true;
    { GR2::Preparation batch; Check(!batch.Enabled(),"owner-only audit serial path"); }
    AnimationStallAudit::enabled=false;
    Check(!GR2::liveReaderDocuments && !liveDocuments && !AnimationRuntime::GetLifetimeCounts().skeletons,"zero live resources");
}
void Corpus(const char* listPath) {
    std::ifstream list(listPath); Check(bool(list),"corpus list");
    std::vector<std::string> paths; std::string path;
    while(std::getline(list,path)) { if(!path.empty() && path.back()=='\r') path.pop_back(); if(!path.empty()) paths.push_back(path); }
    std::size_t sections=0,outputBytes=0,models=0,meshes=0,materials=0,skeletons=0,bones=0,animations=0,rejected=0;
    for(std::size_t base=0;base<paths.size();base+=16) {
        GR2::Preparation batch;
        const auto end=std::min(base+16,paths.size());
        std::vector<std::vector<std::byte>> inputs;
        for(auto i=base;i<end;++i) inputs.push_back(GR2Golden::Bytes(std::filesystem::u8path(paths[i])));
        std::vector<GR2::File> references;
        for(const auto& input:inputs) references.emplace_back(input);
        for(auto i=base;i<end;++i) batch.Add(paths[i],[&,i]{return inputs[i-base];});
        for(auto i=base;i<end;++i) batch.Add(paths[i]+"-truncated.gr2",[&,i]{
            auto bytes=inputs[i-base]; bytes.resize(bytes.size()/2); return bytes;
        });
        batch.Run(4,Execute);
        // Independently compare every expanded section from concurrent File
        // decoding against the serial reader, then compare the actual batch IR.
        std::atomic_size_t next{base}; std::vector<std::future<void>> checks;
        std::vector<std::size_t> byteCounts(end-base),sectionCounts(end-base);
        for(unsigned lane=0;lane<4;++lane) checks.push_back(Execute([&] {
            for(;;) {
                const auto i=next.fetch_add(1); if(i>=end) return;
                GR2::File parallel(inputs[i-base]); const auto& serial=references[i-base];
                for(std::uint32_t s=0;s<serial.header.sections.size();++s) {
                    const auto size=serial.header.sections[s].expanded;
                    const auto a=serial.Bytes({s,0},size),b=parallel.Bytes({s,0},size);
                    Check(std::equal(a.begin(),a.end(),b.begin(),b.end()),"section byte equality");
                    byteCounts[i-base]+=size; ++sectionCounts[i-base];
                }
            }
        }));
        for(auto& check:checks) check.get();
        for(auto i=base;i<end;++i) {
            const auto serial=GR2::Read(references[i-base]);
            auto parallel=GR2::Preparation::Take(paths[i]);
            Check(parallel && !parallel->error && Snapshot(serial).bytes==Snapshot(parallel->contents).bytes,"corpus IR equality: "+paths[i]);
            auto truncated=GR2::Preparation::Take(paths[i]+"-truncated.gr2");
            Check(truncated && bool(truncated->error),"truncated corpus rejected without blocking valid jobs"); ++rejected;
            models+=serial.models.size(); animations+=serial.animations.size();
            for(const auto& model:serial.models) { meshes+=model.meshes.size(); materials+=model.materials.size(); if(model.skeleton) { ++skeletons; bones+=model.skeleton->bones.size(); } }
            sections+=sectionCounts[i-base]; outputBytes+=byteCounts[i-base];
        }
    }
    Check(!paths.empty(),"empty corpus");
    std::cout<<"CORPUS files="<<paths.size()<<" sections="<<sections<<" bytes="<<outputBytes<<" models="<<models<<" meshes="<<meshes<<" materials="<<materials<<" skeletons="<<skeletons<<" bones="<<bones<<" animations="<<animations<<" truncatedRejected="<<rejected<<'\n';
}
}
int main(int argc,char** argv) {
    try {
        Safety(); if(argc==2) Corpus(argv[1]);
        Check(!GR2::liveReaderDocuments && !liveDocuments && !AnimationRuntime::GetLifetimeCounts().skeletons,"final resources");
        std::cout<<"PASS parallel GR2 preparation; repeats=4 invalid/truncated=reject single-flight=pass resources=0\n";
    }
    catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
