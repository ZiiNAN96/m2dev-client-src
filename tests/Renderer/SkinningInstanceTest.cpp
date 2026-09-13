#include "EterGrnLib/StdAfx.h"
#include "EterGrnLib/ModelInstance.h"
#include "EterLib/ResourceManager.h"
#include "EterLib/Camera.h"
#include "PackLib/PackManager.h"
#include "EterLib/SourceResourceAudit.h"
#include <iostream>
#include <stdexcept>

float CCamera::CAMERA_MAX_DISTANCE=2500.f;
static void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
struct NativeAsset
{
    granny_file* file{};
    CGrannyModel* model{};
    explicit NativeAsset(const std::string& path)
    {
        file=GrannyReadEntireFile(path.c_str());Check(file!=nullptr,"Real GR2 file");
        auto* info=GrannyGetFileInfo(file);Check(info && info->ModelCount,"Real GR2 model");
        model=new CGrannyModel;
        Check(model->CreateFromGrannyModelPointer(info->Models[0]),"Native model construction");
    }
    ~NativeAsset() { if(model) model->Release(); if(file) GrannyFreeFile(file); }
};
struct TestInstance : CGrannyModelInstance
{
    void Verify()
    {
        auto palette=GetSkinningPalette(); Check(palette && palette->ready,"Native sampled CPU palette ready");
        const auto& source=*GetModel()->GetSkinningData();const auto& remaps=GetSkinningRemaps();
        auto& buffer=__GetDeformableVertexBufferRef();void* pointer{};
        Check(buffer.Lock(&pointer),"Existing CPU deform output available");
        const auto* output=static_cast<const TPNTVertex*>(pointer);
        bool parity=true;
        for(size_t m=0;m<source.meshes.size();++m) if(source.meshes[m]) {
            const auto& mesh=*source.meshes[m];Check(remaps[m]!=nullptr,"Native binding sidecar");
            for(size_t v=0;v<mesh.vertices.size();++v) {
                float expected[6]{};const auto& input=mesh.vertices[v];
                for(int k=0;k<4;++k) if(input.weights[k]) {
                    const auto& matrix=palette->matrices.at(remaps[m]->meshToSkeleton.at(input.indices[k]));
                    float weight=input.weights[k]*(1.f/255.f);
                    for(int c=0;c<3;++c) {
                        expected[c]+=weight*(input.position[0]*matrix[c]+input.position[1]*matrix[4+c]+input.position[2]*matrix[8+c]+matrix[12+c]);
                        expected[3+c]+=weight*(input.normal[0]*matrix[c]+input.normal[1]*matrix[4+c]+input.normal[2]*matrix[8+c]);
                    }
                }
                const float* actual=reinterpret_cast<const float*>(&output[mesh.deformVertexOffset+v]);
                for(int c=0;c<6;++c) parity&=std::isfinite(actual[c]) && std::abs(expected[c]-actual[c])<=
                    (c<3?1e-4f+2e-6f*std::max(1.f,std::abs(actual[c])):1e-5f);
                parity&=actual[6]==input.uv[0] && actual[7]==input.uv[1];
            }
        }
        buffer.Unlock();Check(parity,"Integrated sidecar vs unmodified native CPU output");
    }
};
int main(int argc,char** argv)
{
    try {
        Check(argc==2,"Real asset root required");
        CPackManager packs; CResourceManager resources;
        // Geometry/lifetime test: image factories intentionally absent; world fixture tests real textures separately.
        {
            std::string base=std::string(argv[1])+"/PC/ymir work/pc/warrior/";
            NativeAsset body(base+"warrior_novice.gr2"), armorA(base+"warrior_4-1.gr2"), armorB(base+"warrior_nahan.gr2");
            NativeAsset hairA(base+"hair/hair_1_1.gr2"),hairB(base+"hair/hair_2_1.gr2"),equivalentBody(base+"warrior_novice.gr2");
            NativeAsset mount(std::string(argv[1])+"/NPC/ymir work/npc/horse/horse_normal.gr2");
            TestInstance actor,clone,hair,hairClone,horse;
            CGrannyModelInstance* current=&actor;
            Math::Matrix world;Math::MatrixIdentity(&world);world._41=17;world._42=-9;
            horse.SetMainModelPointer(mount.model,nullptr);horse.Deform(&world);horse.Verify();
            size_t phases=0;
            for(auto* shape:{body.model,armorA.model,armorB.model,body.model}) {
                hair.Clear();hairClone.Clear();
                actor.SetMainModelPointer(shape,nullptr);clone.SetMainModelPointer(shape,nullptr);
                actor.Deform(&world);clone.Deform(&world);actor.Verify();clone.Verify();
                Check(actor.GetModel()->GetSkinningData()==clone.GetModel()->GetSkinningData(),"Static model data shared between actors");
                Check(actor.GetSkinningRemaps()[0]==clone.GetSkinningRemaps()[0],"Native bindings share immutable remap");
                Check(actor.GetSkinningPalette()!=clone.GetSkinningPalette(),"Independent actor poses");
                Check(actor.GetSkinningPalette()!=horse.GetSkinningPalette(),"Mount has independent skeleton/palette");
                for(auto* style:{hairA.model,hairB.model,hairA.model}) {
                    hair.SetLinkedModelPointer(style,nullptr,&current);hairClone.SetLinkedModelPointer(style,nullptr,&current);
                    hair.Deform(&world);hairClone.Deform(&world);hair.Verify();hairClone.Verify();
                    Check(hair.GetSkinningPalette()==actor.GetSkinningPalette(),"Hair references current body pose without copy");
                    Check(hair.GetSkinningRemaps()[0]==hairClone.GetSkinningRemaps()[0],"Hair remap deduplicated");
                    Check(hair.GetSkinningRemaps()[0]->destination==shape->GetSkinningData()->skeleton,"Shape-specific target skeleton");
                    ++phases;
                }
            }
            // Exercise the native pointer-to-current-LOD contract without changing its CPU remap.
            clone.SetMainModelPointer(equivalentBody.model,nullptr);clone.Deform(&world);
            current=&clone;hair.Deform(&world);hair.Verify();
            Check(hair.GetSkinningPalette()==clone.GetSkinningPalette(),"Equivalent LOD owner refresh");
            clone.SetMainModelPointer(armorA.model,nullptr);clone.Deform(&world);
            // Call skeleton-only update: the deliberately mismatched CPU binding must not be deformed.
            hair.UpdateSkeleton(&world,0);
            Check(hair.GetSkinningStatus()==Renderer::SkinDataStatus::DestinationChanged && !hair.GetSkinningPalette(),"Incompatible owner rejected explicitly");
            current=&actor;hair.Deform(&world);hair.Verify();
            Check(hair.GetSkinningPalette()==actor.GetSkinningPalette(),"Original owner restored after rejection");
            auto oldPalette=std::weak_ptr<const Renderer::BonePalette>(actor.GetSkinningPalette());
            hair.Clear();hairClone.Clear();actor.Clear();clone.Clear();horse.Clear();
            Check(oldPalette.expired(),"Clearing instances releases pose owner");
            Check(Renderer::liveBonePalettes==0 && Renderer::liveBoneRemaps==0,"No instance/remap resources after Clear");
            std::cout<<"PASS native shape/hair phases="<<phases<<" sharing, CPU parity, equivalent/incompatible/restored LOD, mount, Clear\n";
        }
        resources.Destroy();
        Check(Renderer::skinSidecarFailures==1,"Exactly one deliberate incompatible-LOD diagnostic, no other failures");
        Check(Renderer::liveSkinMeshes==0 && Renderer::liveBoneRemaps==0 && Renderer::liveBonePalettes==0,"No B2 resources after model unload");
        Renderer::WriteSourceResourceAudit(std::cout);return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n';return 1; }
}
