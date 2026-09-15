#include "VegetationSource.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <windows.h>
namespace fs=std::filesystem;
using namespace ZiiNAN;
std::string Key(const fs::path&path){const auto text=Vegetation::NormalizeKey(path.generic_string());const auto pos=text.find("ymir work/");if(pos==text.npos)throw std::runtime_error("source must reside below ymir work, or pass an explicit legacy key");return "d:/"+text.substr(pos);}
bool Convert(const fs::path&input,const fs::path&output,std::string key,Vegetation::Registry&registry){
    VegetationTool::VegetationSource source;std::string error;if(!VegetationTool::Extract(input,key,source,error)){std::cerr<<key<<": "<<error<<'\n';return false;}
    const auto geometry=output/source.metadata.geometry;auto meta=geometry;meta.replace_extension(".zveg");fs::create_directories(geometry.parent_path());AssetTool::Report report;
    if(!AssetTool::WriteGLB(source.scene,geometry,report)){for(const auto&i:report.issues)if(i.error)std::cerr<<key<<": "<<i.message<<'\n';return false;}
    std::ofstream metadata(meta,std::ios::binary);metadata<<Vegetation::SerializeMetadata(source.metadata);metadata.close();if(!metadata)return false;
    auto relative=source.metadata.geometry;relative.replace(relative.size()-4,4,".zveg");const auto added=registry.Add(key,relative);if(!added){std::cerr<<added.error<<'\n';return false;}
    Vegetation::Runtime runtime;runtime.registry=registry;auto load=runtime.Load(key,[&](std::string_view name,std::vector<std::byte>&bytes){std::ifstream in(output/fs::path(name),std::ios::binary|std::ios::ate);if(!in)return false;auto size=in.tellg();if(size<0||size>128*1024*1024)return false;bytes.resize(std::size_t(size));in.seekg(0);return bool(in.read(reinterpret_cast<char*>(bytes.data()),size));});
    if(!load){std::cerr<<key<<": runtime validation: "<<load.error<<'\n';return false;}
    std::cout<<"converted "<<key<<" meshes="<<source.scene.meshes.size()<<" lods="<<source.lodCounts[0]<<','<<source.lodCounts[1]<<','<<source.lodCounts[2]<<" reconstructedNormals="<<source.reconstructedNormals<<" unusedNormals="<<source.unusedNormals<<'\n';return true;
}
int Run(int argc,char**argv){
    try{
        if(argc<3)throw std::runtime_error("usage: ZiiNANVegetationTool inspect tree.spt | convert tree.spt output [legacy-key] | batch asset-directory output");
        const std::string mode=argv[1];const fs::path input=argv[2];
        if(mode=="inspect"){VegetationTool::VegetationSource source;std::string error;if(!VegetationTool::Extract(input,Key(input),source,error))throw std::runtime_error(error);std::cout<<Vegetation::SerializeMetadata(source.metadata);return 0;}
        if(argc<4)throw std::runtime_error("output directory required");const auto output=fs::absolute(argv[3]);fs::create_directories(output/"vegetation");Vegetation::Registry registry;
        unsigned total=0,failed=0;
        if(mode=="convert"){total=1;failed=!Convert(input,output,argc>4?argv[4]:Key(input),registry);}
        else if(mode=="batch"){std::vector<fs::path>paths;for(const auto&e:fs::recursive_directory_iterator(input))if(e.is_regular_file()&&Vegetation::NormalizeKey(e.path().extension().string())==".spt")paths.push_back(e.path());std::sort(paths.begin(),paths.end());for(const auto&p:paths){++total;failed+=!Convert(p,output,Key(p),registry);}}
        else throw std::runtime_error("unknown vegetation tool command");
        if(failed==0){std::ofstream out(output/"vegetation/registry.json",std::ios::binary);out<<registry.Serialize();if(!out)throw std::runtime_error("registry write failed");}
        std::cout<<"total="<<total<<" converted="<<total-failed<<" failed="<<failed<<'\n';return failed?1:0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
int main(int argc,char**argv){
    // Faults in the closed legacy binary must terminate only the offline conversion with failure.
    __try{return Run(argc,argv);}__except(EXCEPTION_EXECUTE_HANDLER){std::cerr<<"legacy SDK rejected corrupt input (structured exception)\n";return 1;}
}
