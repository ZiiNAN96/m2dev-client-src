#include "AssetRuntime/GR2/GR2Reader.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs=std::filesystem;
using namespace AssetRuntime::GR2;
std::vector<std::byte> ReadFile(const fs::path& path)
{
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) throw std::runtime_error("cannot open input");
    const auto size=input.tellg(); Require(size>0 && size<=std::streamoff(MaximumFileBytes),"file size limit");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size)); input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!input) throw std::runtime_error("incomplete file read");
    return bytes;
}
int main(int argc,char** argv)
{
    try {
        if(argc<2 || argc>3) { std::cerr<<"Usage: GR2CorpusScanner asset-root-or-file [report-directory]\n"; return 2; }
        const fs::path root=argv[1]; std::vector<fs::path> paths;
        if(fs::is_regular_file(root)) paths.push_back(root);
        else for(const auto& entry:fs::recursive_directory_iterator(root)) if(entry.is_regular_file()) {
            auto extension=entry.path().extension().string(); for(auto& c:extension) if(c>='A'&&c<='Z') c+='a'-'A';
            if(extension==".gr2") paths.push_back(entry.path());
        }
        std::sort(paths.begin(),paths.end());
        std::ofstream files,summary;
        if(argc==3) { fs::create_directories(argv[2]); files.open(fs::path(argv[2])/"files.tsv"); summary.open(fs::path(argv[2])/"summary.tsv"); if(!files||!summary) throw std::runtime_error("cannot create reports"); }
        std::map<std::string,std::size_t> counts;
        const auto started=std::chrono::steady_clock::now();
        for(const auto& path:paths) {
            std::string category="parsed",diagnostic;
            try {
                const auto bytes=ReadFile(path); const auto header=Inspect(bytes);
                ++counts["version/"+std::to_string(header.version)]; ++counts["tag/"+std::to_string(header.tag)];
                ++counts["section-count/"+std::to_string(header.sections.size())];
                for(std::size_t i=0;i<header.sections.size();++i) {
                    const auto& s=header.sections[i]; ++counts["compression/"+std::to_string(s.compression)]; ++counts["alignment/"+std::to_string(s.alignment)];
                    counts["relocations"]+=s.fixupCount; counts["marshalling-records"]+=s.marshalCount;
                    if(s.expanded) ++counts["nonempty-section/"+std::to_string(i)];
                }
                File file(bytes); auto content=Read(file);
                counts["models"]+=content.models.size(); counts["animations"]+=content.animations.size();
                for(const auto& model:content.models) { counts["meshes"]+=model.meshes.size(); counts["materials"]+=model.materials.size(); if(model.skeleton) counts["bones"]+=model.skeleton->bones.size(); }
                for(const auto& model:content.models) {
                    if(model.skeleton) ++counts["skeletons"];
                    for(const auto& material:model.materials) for(const auto& path:material.textures) if(!path.empty()) ++counts["texture-paths"];
                    for(const auto& mesh:model.meshes) { counts["bone-bindings"]+=mesh.skin.boneNames.size(); ++counts["index-width/"+std::to_string(int(mesh.indexWidth))]; }
                }
                for(const auto& animation:content.animationData) for(const auto& group:animation.groups) { ++counts["track-groups"]; counts["transform-tracks"]+=group.tracks.size(); ++counts["accumulation-flags/"+std::to_string(group.accumulationFlags)]; }
                for(const auto& [name,count]:content.vertexFormats) counts["vertex/"+name]+=count;
                for(const auto& [name,count]:content.curveFormats) counts["curve/"+name]+=count;
            } catch(const Error& error) {
                switch(error.kind) {
                case Failure::UnsupportedVersion: category="unsupported version"; break;
                case Failure::UnsupportedCompression: category="unsupported compression"; break;
                case Failure::UnsupportedType: category="unsupported curve/type"; break;
                case Failure::Malformed: category="malformed"; break;
                default: category="other failure";
                } diagnostic=error.what();
            } catch(const std::exception& error) { category="other failure"; diagnostic=error.what(); }
            ++counts["status/"+category]; if(!diagnostic.empty()) ++counts["reason/"+diagnostic];
            if(files) files<<path.lexically_relative(fs::is_directory(root)?root:root.parent_path()).generic_string()<<'\t'<<category<<'\t'<<diagnostic<<'\n';
            else std::cout<<path.filename().string()<<'\t'<<category<<'\t'<<diagnostic<<'\n';
        }
        counts["total"]=paths.size();
        for(const auto& [key,value]:counts) { std::cout<<key<<'\t'<<value<<'\n'; if(summary) summary<<key<<'\t'<<value<<'\n'; }
        std::cout<<"elapsed_ms\t"<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<'\n';
        return paths.empty()?2:0; // Coverage failures are report data, never hidden as test success.
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 2; }
}
