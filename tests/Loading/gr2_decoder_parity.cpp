// Compile with /I <saved pre-change GR2 directory>. Baseline files are generated
// from the recorded commit into the build directory, never a production fallback.
#include "AssetRuntime/GR2/GR2File.h"
#include "AssetRuntime/AnimationStallAudit.h"
#define GR2 GR2Baseline
#include <GR2File.h>
#include <GR2File.cpp>
#include <GR2Compression.cpp>
#undef GR2
#include "AssetRuntime/GR2/GR2File.cpp"
#include "AssetRuntime/GR2/GR2Compression.cpp"
#include <filesystem>
#include <iostream>

int main(int argc,char** argv)
{
    try {
        if(argc!=2)return 2;
        // Deterministic exhaustive queries over small/large CDF intervals,
        // including duplicates, empty ranges and the one-past-end sentinel.
        std::size_t searches=0;
        for(unsigned size:{0u,1u,2u,3u,4u,7u,15u,16u,31u,64u,257u}) {
            std::vector<std::uint32_t> ranges;
            for(unsigned i=0;i<size;++i)ranges.push_back((i/2)*16384/std::max(1u,size/2));
            for(unsigned value=0;value<=16384;++value) {
                const auto got=AssetRuntime::GR2::SymbolUpperBound(ranges.begin(),ranges.end(),value);
                if(got!=std::upper_bound(ranges.begin(),ranges.end(),value))return 4;
                ++searches;
            }
            for(std::size_t begin=0;begin<=ranges.size();++begin) {
                const auto end=std::min(ranges.size(),begin+15);
                for(unsigned value:{0u,1u,255u,256u,8191u,8192u,16383u,16384u}) {
                    const auto first=ranges.begin()+begin,last=ranges.begin()+end;
                    if(AssetRuntime::GR2::SymbolUpperBound(first,last,value)!=std::upper_bound(first,last,value))return 4;
                    ++searches;
                }
            }
        }
        std::ifstream list(argv[1]);if(!list)return 2;
        std::string path;
        double before=0,after=0;
        std::size_t files=0,sections=0,bytes=0,rejected=0,innerRejected=0;
        while(std::getline(list,path)) {
            std::ifstream input(std::filesystem::u8path(path),std::ios::binary|std::ios::ate);
            if(!input)return 2;
            std::vector<std::byte> source(static_cast<std::size_t>(input.tellg()));
            input.seekg(0);input.read(reinterpret_cast<char*>(source.data()),source.size());if(!input)return 2;
            const auto h=AssetRuntime::GR2::Inspect(source);
            const auto old=AssetRuntime::GR2Baseline::Inspect(source);
            for(std::size_t i=0;i<h.sections.size();++i) {
                const auto& s=h.sections[i];auto payload=std::span(source).subspan(s.offset,s.compressed);
                auto start=MapLoadTrace::Clock::now();
                const auto expected=AssetRuntime::GR2Baseline::Decompress(old.sections[i],payload);
                before+=MapLoadTrace::Ms(MapLoadTrace::Clock::now()-start);
                start=MapLoadTrace::Clock::now();const auto actual=AssetRuntime::GR2::Decompress(s,payload);
                after+=MapLoadTrace::Ms(MapLoadTrace::Clock::now()-start);
                if(expected!=actual){std::cerr<<"MISMATCH "<<path<<" section "<<i;return 1;}
                ++sections;bytes+=actual.size();
                if(!s.compressed)continue;
                auto truncated=payload.first(payload.size()/2);
                bool a=false,b=false;
                try{AssetRuntime::GR2Baseline::Decompress(old.sections[i],truncated);}catch(const AssetRuntime::GR2Baseline::Error&){a=true;}
                try{AssetRuntime::GR2::Decompress(s,truncated);}catch(const AssetRuntime::GR2::Error&){b=true;}
                if(!a||!b)return 3;++rejected;
                // Adjust the declared compressed size too: this exercises the
                // arithmetic stream/backreference checks, not just span length.
                auto os=old.sections[i];auto ns=s;os.compressed=ns.compressed=static_cast<uint32_t>(truncated.size());
                a=b=false;
                try{AssetRuntime::GR2Baseline::Decompress(os,truncated);}catch(const AssetRuntime::GR2Baseline::Error&){a=true;}
                try{AssetRuntime::GR2::Decompress(ns,truncated);}catch(const AssetRuntime::GR2::Error&){b=true;}
                if(!a||!b){std::cerr<<"truncated accepted "<<path<<" section "<<i;return 3;}++innerRejected;
            }
            ++files;
        }
        if(!files)return 2;
        std::cout<<"PASS searches="<<searches<<" files="<<files<<" sections="<<sections<<" equalBytes="<<bytes
                 <<" truncatedRejected="<<rejected<<" adjustedSizeTruncatedRejected="<<innerRejected
                 <<" beforeMs="<<before<<" afterMs="<<after<<'\n';
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
