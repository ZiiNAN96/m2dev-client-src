// Compare decoded production pack entries; no pack mutation or runtime changes.
#include "EterGrnLib/StdAfx.h"
#include "PackLib/Pack.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

namespace fs=std::filesystem;
static void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static TPackFile Read(const fs::path& path)
{
    std::ifstream file(path,std::ios::binary|std::ios::ate);Require(bool(file),"source file missing");
    const auto size=file.tellg();Require(size>=0&&size<256*1024*1024,"source size");TPackFile bytes(static_cast<std::size_t>(size));
    file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),size);Require(bool(file),"source read");return bytes;
}
int main(int argc,char**argv)
{
    try {
        Require(argc==5||argc==6,"source pack source-directory report.tsv OR compare old-pack new-pack report.tsv allowed-virtual-path");
        const bool source=std::string_view(argv[1])=="source";Require(source?argc==5:(argc==6&&std::string_view(argv[1])=="compare"),"mode");
        CPack before,after;Require(before.Load(argv[2]),"original pack load");
        std::map<std::string,TPackFileEntry> other;
        if(!source){Require(after.Load(argv[3]),"new pack load");for(const auto& e:after.GetIndex())Require(other.emplace(e.file_name,e).second,"duplicate new entry");Require(other.size()==before.GetIndex().size(),"entry count changed");}
        std::ofstream report(argv[4]);Require(bool(report),"report");report<<"virtual_path\told_size\tnew_size\tidentical\n";
        std::set<std::string> paths;std::size_t changed=0;
        for(const auto& entry:before.GetIndex()) {
            const std::string name=entry.file_name;Require(paths.insert(name).second,"duplicate original entry");
            TPackFile original,replacement;Require(before.GetFile(entry,original),"original entry load");
            if(source) {
                const fs::path relative=name.starts_with("d:/")?name.substr(3):name;
                Require(!relative.is_absolute()&&!relative.has_root_name(),"source path is not relative");
                for(const auto& component:relative)Require(component!="..","source path traversal");
                replacement=Read(fs::path(argv[3])/relative);
            } else {const auto it=other.find(name);Require(it!=other.end()&&after.GetFile(it->second,replacement),"new entry missing");}
            const bool identical=original==replacement;report<<name<<'\t'<<original.size()<<'\t'<<replacement.size()<<'\t'<<identical<<'\n';
            if(!identical){++changed;Require(!source&&name==argv[5],"unexpected content change");}
        }
        if(source){std::size_t files=0;for(const auto& e:fs::recursive_directory_iterator(argv[3]))files+=e.is_regular_file();Require(files==paths.size(),"source contains extra entries");}
        else Require(changed==1,"expected exactly one asset replacement");
        report.flush();Require(bool(report),"report write");std::cout<<"PASS entries="<<paths.size()<<" changed="<<changed<<" mode="<<argv[1]<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
