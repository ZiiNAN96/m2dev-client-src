#include "AuditCommon.h"
#include <basisu_transcoder.h>
#include <zstd.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
using namespace CLib;
static volatile uint64_t checksum{};
static void WriteDDS(const std::string& path,uint32_t width,uint32_t height,uint32_t dxgi,const std::vector<std::vector<uint8_t>>& levels) {
    uint32_t dds[37]{};dds[0]=0x20534444;dds[1]=124;dds[2]=0xA1007;dds[3]=height;dds[4]=width;
    dds[5]=uint32_t(levels[0].size());dds[7]=uint32_t(levels.size());dds[19]=32;dds[20]=4;dds[21]=0x30315844;
    dds[27]=0x401008;dds[32]=dxgi;dds[33]=3;dds[35]=1;
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<char*>(dds),sizeof(dds));
    for(const auto& bytes:levels)file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());Check(bool(file),"DDS output");
}
static void ExtractBaseline(const char* inputPath,const char* prefix,const std::string& semantic) {
    Check(semantic=="normal"||semantic=="color"||semantic=="data","Baseline semantic");
    auto input=Read(inputPath);basist::basisu_transcoder_init();basist::ktx2_transcoder source;
    Check(source.init(input.data(),uint32_t(input.size()))&&source.start_transcoding(),"Source KTX2");
    std::vector<std::vector<uint8_t>> levels(source.get_levels());
    for(uint32_t mip=0;mip<source.get_levels();++mip){
        const auto w=std::max(1u,source.get_width()>>mip),h=std::max(1u,source.get_height()>>mip);
        std::vector<uint8_t> rgba(w*h*4);
        Check(source.transcode_image_level(mip,0,0,rgba.data(),w*h,basist::transcoder_texture_format::cTFRGBA32),"Original mip decode");
        Check(stbi_write_png((std::string(prefix)+"-original-mip"+std::to_string(mip)+".png").c_str(),int(w),int(h),4,rgba.data(),int(w*4))!=0,"Original mip output");
        const uint32_t bw=(w+3)/4,bh=(h+3)/4;levels[mip].resize(bw*bh*16);
        // Preserve all authored normal components, including negative Z.
        // BC5 positive-Z reconstruction would change this source's content.
        Check(source.transcode_image_level(mip,0,0,levels[mip].data(),bw*bh,basist::transcoder_texture_format::cTFBC7_RGBA),"Baseline BC7");
    }
    WriteDDS(std::string(prefix)+"-baseline.dds",source.get_width(),source.get_height(),semantic=="color"?99:98,levels);
    std::cout<<"{\"width\":"<<source.get_width()<<",\"height\":"<<source.get_height()<<",\"mips\":"<<source.get_levels()<<",\"semantic\":\""<<semantic<<"\"}\n";
}
int main(int argc,char** argv) {
    try {
        if(argc==5&&std::string(argv[1])=="--extract-baseline"){ExtractBaseline(argv[2],argv[3],argv[4]);return 0;}
        Check(argc==4||argc==5,"CoreTextureAudit ktx2 source-file output-prefix [BC format], or --extract-baseline ktx2 prefix normal|color|data");
        const std::string onlyFormat=argc==5?argv[4]:"";
        Check(onlyFormat.empty()||onlyFormat=="BC1"||onlyFormat=="BC3"||onlyFormat=="BC5"||onlyFormat=="BC7"||onlyFormat=="ETC2"||onlyFormat=="ASTC","Requested target format");
        auto input=Read(argv[1]);auto source=Read(argv[2]);const std::string prefix=argv[3];std::cout<<std::setprecision(9);
        auto start=Clock::now();basist::basisu_transcoder_init();const double globalInit=Micros(start);
        std::vector<std::byte> packed(ZSTD_compressBound(source.size()));const auto packedSize=ZSTD_compress(packed.data(),packed.size(),source.data(),source.size(),17);Check(!ZSTD_isError(packedSize),"Source pack Zstd");
        std::vector<std::byte> output(source.size());auto unpack=Bench([&](int){const auto size=ZSTD_decompress(output.data(),output.size(),packed.data(),packedSize);if(size!=source.size())std::abort();checksum=uint64_t(output[0]);},8);Check(output==source,"Source pack roundtrip");
        std::vector<std::byte> candidatePack(ZSTD_compressBound(input.size()));const auto candidatePackSize=ZSTD_compress(candidatePack.data(),candidatePack.size(),input.data(),input.size(),17);Check(!ZSTD_isError(candidatePackSize),"Candidate pack Zstd");
        basist::ktx2_transcoder texture;start=Clock::now();Check(texture.init(input.data(),uint32_t(input.size()))&&texture.start_transcoding(),"KTX2 parse/start");const double firstInit=Micros(start);
        std::cout<<"{\"file\":\""<<std::filesystem::path(argv[1]).filename().string()<<"\",\"source_bytes\":"<<source.size()<<",\"source_pack_zstd17_bytes\":"<<packedSize<<",\"ktx2_bytes\":"<<input.size()<<",\"ktx2_pack_zstd17_bytes\":"<<candidatePackSize<<",\"width\":"<<texture.get_width()<<",\"height\":"<<texture.get_height()<<",\"mips\":"<<texture.get_levels()<<",\"global_init_us\":"<<globalInit<<",\"first_parse_start_us\":"<<firstInit;
        PrintStats("source_pack_decode",unpack);std::cout<<"}\n";
        const std::pair<const char*,basist::transcoder_texture_format> formats[]={
            {"BC1",basist::transcoder_texture_format::cTFBC1_RGB},{"BC3",basist::transcoder_texture_format::cTFBC3_RGBA},
            {"BC5",basist::transcoder_texture_format::cTFBC5_RG},{"BC7",basist::transcoder_texture_format::cTFBC7_RGBA},
            {"ETC2",basist::transcoder_texture_format::cTFETC2_RGBA},{"ASTC",basist::transcoder_texture_format::cTFASTC_4x4_RGBA}};
        for(const auto& [name,format]:formats){
            if(!onlyFormat.empty()&&onlyFormat!=name)continue;
            std::vector<std::vector<uint8_t>> levels(texture.get_levels());std::vector<uint32_t> blocks(levels.size());size_t gpuBytes=0;
            for(uint32_t m=0;m<levels.size();++m){const auto w=std::max(1u,texture.get_width()>>m),h=std::max(1u,texture.get_height()>>m);blocks[m]=((w+3)/4)*((h+3)/4);levels[m].resize(blocks[m]*basist::basis_get_bytes_per_block_or_pixel(format));gpuBytes+=levels[m].size();}
            auto run=[&](){for(uint32_t m=0;m<levels.size();++m)if(!texture.transcode_image_level(m,0,0,levels[m].data(),blocks[m],format))return false;checksum=levels[0][0];return true;};
            start=Clock::now();const bool supported=run();const double first=Micros(start);if(!supported){std::cout<<"{\"file\":\""<<std::filesystem::path(argv[1]).filename().string()<<"\",\"format\":\""<<name<<"\",\"supported\":false}\n";continue;}
            const auto time=Bench([&](int){if(!run())std::abort();},4);
            // Include parsing, table setup, allocation and first transcode in a separate warm-process load measurement.
            const auto load=Bench([&](int){basist::ktx2_transcoder fresh;if(!fresh.init(input.data(),uint32_t(input.size()))||!fresh.start_transcoding())std::abort();for(uint32_t m=0;m<levels.size();++m)if(!fresh.transcode_image_level(m,0,0,levels[m].data(),blocks[m],format))std::abort();},2);
            std::cout<<"{\"file\":\""<<std::filesystem::path(argv[1]).filename().string()<<"\",\"format\":\""<<name<<"\",\"supported\":true,\"gpu_payload_bytes\":"<<gpuBytes<<",\"first_transcode_us\":"<<first;PrintStats("warm_transcode",time);PrintStats("memory_load_prepare",load);std::cout<<"}\n";
            const int dxgi=std::string(name)=="BC1"?71:std::string(name)=="BC3"?77:std::string(name)=="BC5"?83:std::string(name)=="BC7"?98:0;
            if(dxgi){uint32_t dds[37]{};dds[0]=0x20534444;dds[1]=124;dds[2]=0xA1007;dds[3]=texture.get_height();dds[4]=texture.get_width();dds[5]=uint32_t(levels[0].size());dds[7]=texture.get_levels();dds[19]=32;dds[20]=4;dds[21]=0x30315844;dds[27]=0x401008;dds[32]=uint32_t(dxgi);dds[33]=3;dds[35]=1;std::ofstream file(prefix+"-"+name+".dds",std::ios::binary);file.write(reinterpret_cast<char*>(dds),sizeof(dds));for(const auto& bytes:levels)file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());Check(bool(file),"DDS output");}
        }
        for(uint32_t m=0;m<texture.get_levels();++m){const auto w=std::max(1u,texture.get_width()>>m),h=std::max(1u,texture.get_height()>>m);std::vector<uint8_t> rgba(w*h*4);Check(texture.transcode_image_level(m,0,0,rgba.data(),w*h,basist::transcoder_texture_format::cTFRGBA32),"RGBA quality decode");Check(stbi_write_png((prefix+"-mip"+std::to_string(m)+".png").c_str(),int(w),int(h),4,rgba.data(),int(w*4))!=0,"PNG output");}
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
