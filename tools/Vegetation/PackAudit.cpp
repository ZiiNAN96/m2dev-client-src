// Read selected production pack bytes; no changes to assets or packs.
#include "PackLib/Pack.h"
#include "AssetRuntime/GR2/GR2Reader.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
namespace fs=std::filesystem;
std::string Lower(std::string s){for(auto& c:s){if(c=='\\')c='/';else if(c>='A'&&c<='Z')c+=32;}return s;}
bool Relevant(const std::string& s,bool textures){return s.ends_with(".spt")||s.ends_with(".prt")||s.ends_with("areadata.txt")||s.ends_with("objectdata.txt")||s.ends_with(".msm")||
    (textures&&(s.ends_with(".prb")||s.ends_with(".gr2")||s.ends_with("setting.txt")||s.starts_with("textureset/")||s.ends_with("tile.raw")||s.ends_with("water.wtr")));}
int main(int argc,char**argv){
    try{
        if(argc!=4 && !(argc==5 && std::string(argv[4])=="--texture-impact"))throw std::runtime_error("usage: VegetationPackAudit client-directory pack-order.txt output.json [--texture-impact]");
        const bool textures=argc==5;
        const fs::path root=fs::absolute(argv[1]);
        std::map<std::string,fs::path> sourceDirs;for(const auto& d:fs::directory_iterator(root/"assets"))if(d.is_directory())sourceDirs[Lower(d.path().filename().string())]=d.path();
        struct Entry{std::shared_ptr<CPack> pack;TPackFileEntry file;std::string package;unsigned versions{};};
        std::map<std::string,Entry> entries;
        rapidjson::StringBuffer buffer;rapidjson::Writer<rapidjson::StringBuffer>w(buffer);w.StartObject();w.Key("missingPacks");w.StartArray();
        std::ifstream order(argv[2]);if(!order)throw std::runtime_error("pack order missing");std::string name;
        while(std::getline(order,name)){
            if(!name.empty()&&name.back()=='\r')name.pop_back();if(name.empty())continue;
            auto pack=std::make_shared<CPack>();if(!pack->Load((root/"pack"/(name+".pck")).string())){w.String(name.c_str());continue;}
            for(const auto& e:pack->GetIndex()){
                const std::string path=Lower(e.file_name);auto& dst=entries[path];const unsigned versions=dst.versions+1;dst={pack,e,name,versions};
            }
        }w.EndArray();w.Key("files");w.StartArray();std::size_t compared=0;
        for(const auto& [path,e]:entries){
            if(!Relevant(path,textures))continue;
            TPackFile bytes;if(!e.pack->GetFile(e.file,bytes))throw std::runtime_error("pack entry read failed: "+path);
            const auto relative=fs::path(path.starts_with("d:/")?path.substr(3):path);
            if(relative.is_absolute()||relative.has_root_name())throw std::runtime_error("unsafe pack source path");
            for(const auto& p:relative)if(p=="..")throw std::runtime_error("unsafe pack source component");
            const fs::path source=sourceDirs.at(Lower(e.package))/relative;
            std::ifstream input(source,std::ios::binary);std::vector<uint8_t> original((std::istreambuf_iterator<char>(input)),{});
            const bool same=input.good()||input.eof()?original==bytes:false;
            w.StartObject();w.Key("path");w.String(path.c_str());w.Key("pack");w.String(e.package.c_str());w.Key("versions");w.Uint(e.versions);
            w.Key("source");w.String(source.generic_string().c_str());w.Key("bytes");w.Uint64(bytes.size());w.Key("identical");w.Bool(same);
            if(path.ends_with(".gr2")) {
                try {
                    AssetRuntime::GR2::File file(std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()),bytes.size()));
                    const auto content=AssetRuntime::GR2::Read(file);
                    w.Key("textures");w.StartArray();
                    for(const auto& model:content.models)for(const auto& material:model.materials)
                        for(const auto& texture:material.textures)if(!texture.empty())w.String(Lower(texture).c_str());
                    w.EndArray();
                } catch(const std::exception& error) {w.Key("parseError");w.String(error.what());}
            } else if(path.ends_with("tile.raw")) {
                std::array<unsigned,256> counts{};for(auto value:bytes)++counts[value];
                w.Key("tileCounts");w.StartArray();for(auto count:counts)w.Uint(count);w.EndArray();
            } else if(path.ends_with("water.wtr")) {
                // Presence is recorded; this does not assert visible water coverage.
            } else if(!path.ends_with(".spt")){w.Key("text");w.String(reinterpret_cast<const char*>(bytes.data()),static_cast<unsigned>(bytes.size()));}
            w.EndObject();++compared;
        }
        w.EndArray();w.Key("availablePaths");w.StartArray();for(const auto& [path,e]:entries)w.String(path.c_str());w.EndArray();w.EndObject();
        std::ofstream output(argv[3],std::ios::binary);output<<buffer.GetString()<<'\n';if(!output)throw std::runtime_error("cannot write pack evidence");
        std::cout<<"PASS compared="<<compared<<" effectiveEntries="<<entries.size()<<'\n';return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
