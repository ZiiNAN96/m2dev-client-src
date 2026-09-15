#include "AssetRuntime/GR2/GR2AssetProvider.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

static void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(int argc,char** argv)
{
    try {
        Check(argc==3,"original GR2 path and output sidecar required");
        std::ifstream input(argv[1],std::ios::binary|std::ios::ate);Check(bool(input),"original GR2 exists");
        std::vector<std::byte> bytes(static_cast<std::size_t>(input.tellg()));input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
        auto original=AssetRuntime::GetGR2AssetProvider().Load(argv[1],bytes);
        auto overridden=AssetRuntime::GetGR2AssetProvider().Load(argv[1],bytes);
        Check(bool(original)&&bool(overridden),"native GR2 production provider");
        const auto* a=original.asset.Model(0).Get();Check(a&&!a->materials.empty(),"legacy material identity");
        AssetRuntime::MaterialOverride entry;entry.name=a->materials[0].name;
        entry.data.metallic=.75f;entry.data.roughness=.4f;entry.data.normalScale=.75f;
        entry.data.emissive={.035f,.01f,0};entry.data.maps[1].path="g1x/normal.png";
        entry.data.maps[2].path="g1x/metal_rough.png";entry.data.maps[3].path="g1x/occlusion.png";
        const auto text=AssetRuntime::SerializeMaterialOverrides({entry});
        Check(overridden.asset.Get()->ApplyMaterialOverrides(text),"neutral sidecar resolves exact model/material/name");
        const auto* b=overridden.asset.Model(0).Get();
        Check(a->materials[0].model==AssetRuntime::MaterialModel::Legacy&&b->materials[0].model==AssetRuntime::MaterialModel::PBRMetallicRoughness,"optional modern material only");
        Check(a->materials[0].textures==b->materials[0].textures&&a->meshes.size()==b->meshes.size(),"legacy texture identities and meshes unchanged");
        for(std::size_t i=0;i<a->meshes.size();++i){
            const auto& left=a->meshes[i];const auto& right=b->meshes[i];
            Check(left.vertexCount==right.vertexCount&&left.indexCount==right.indexCount,"same geometry layout");
        }
        std::ofstream output(argv[2]);output<<text;Check(bool(output),"versioned proof sidecar written");
        std::cout<<"PASS original GR2 read twice without mutation; optional sidecar identity="<<entry.name<<" model=0 material=0\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
