// Same reader executable linked once with each revision, using a measured list.
#include "AssetRuntime/GR2/GR2Reader.h"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    std::ifstream list(argv[1]);if(!list)return 2;
    std::string path;std::size_t files=0,models=0,meshes=0,skeletons=0,bones=0,animations=0;
    try {
        while(std::getline(list,path)) {
            std::ifstream input(std::filesystem::u8path(path),std::ios::binary|std::ios::ate);if(!input)return 2;
            std::vector<std::byte> bytes(static_cast<std::size_t>(input.tellg()));input.seekg(0);
            input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());if(!input)return 2;
            AssetRuntime::GR2::File file(bytes);const auto content=AssetRuntime::GR2::Read(file);
            std::size_t m=0,s=0,b=0;
            for(const auto& model:content.models) {m+=model.meshes.size();if(model.skeleton){++s;b+=model.skeleton->bones.size();}}
            std::cout<<path<<'\t'<<content.models.size()<<'\t'<<m<<'\t'<<s<<'\t'<<b<<'\t'<<content.animations.size()<<'\n';
            ++files;models+=content.models.size();meshes+=m;skeletons+=s;bones+=b;animations+=content.animations.size();
        }
        std::cout<<"TOTAL files="<<files<<" models="<<models<<" meshes="<<meshes<<" skeletons="<<skeletons<<" bones="<<bones<<" animations="<<animations<<'\n';
        return files?0:2;
    }catch(const std::exception& e){std::cerr<<path<<": "<<e.what()<<'\n';return 1;}
}
