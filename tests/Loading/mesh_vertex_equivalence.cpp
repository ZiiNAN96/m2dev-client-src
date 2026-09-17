// Focused CPU compiler check for the pass-independent ModernVS cache key.
// Compile every existing geometry/tangent/instance/pass combination with the
// pinned Diligent HLSL preamble, generated FX includes and production flags.
#include <Windows.h>
#include <d3dcompiler.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include "Renderer/ModernMeshShader.h"

static std::string Read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("Cannot read "+path.string());
    return {std::istreambuf_iterator<char>(file),{}};
}
struct Includes : ID3DInclude {
    std::map<std::string,std::string> sources;
    explicit Includes(const std::filesystem::path& directory) {
        for(const auto& entry:std::filesystem::recursive_directory_iterator(directory))if(entry.is_regular_file()) {
            const auto [it,added]=sources.emplace(entry.path().filename().string(),Read(entry.path()));
            if(!added)throw std::runtime_error("Ambiguous include "+entry.path().string());
        }
    }
    HRESULT __stdcall Open(D3D_INCLUDE_TYPE,LPCSTR name,LPCVOID,LPCVOID* data,UINT* size) override {
        const auto it=sources.find(name);if(it==sources.end())return E_FAIL;
        *data=it->second.data();*size=static_cast<UINT>(it->second.size());return S_OK;
    }
    HRESULT __stdcall Close(LPCVOID) override {return S_OK;}
};
int main(int argc,char** argv) try {
    if(argc!=3)throw std::runtime_error("Usage: mesh_vertex_equivalence <Core> <generated FX>");
    Includes includes(std::filesystem::path(argv[2])/"Shaders");
    const auto preamble=Read(std::filesystem::path(argv[1])/"Graphics/ShaderTools/include/HLSLDefinitions.fxh");
    unsigned compared=0;
    for(unsigned instances=0;instances<2;++instances)for(unsigned tangent=0;tangent<2;++tangent)for(unsigned geometry=0;geometry<3;++geometry) {
        std::string reference;
        for(unsigned pass=0;pass<3;++pass) {
            const auto define=[](const char* name,unsigned value){return std::string("#define ")+name+" "+std::to_string(value)+"\n";};
            auto source=preamble+"\n#define VERTEX_SHADER 1\n"+define("GDX_SKIN",geometry==1)+define("GDX_AUX",geometry==2)+
                define("GDX_SHADOW",pass==1)+define("GDX_FORWARD",pass==2)+define("GDX_TANGENT",tangent)+define("H2_INSTANCED",instances)+
                "#line 1 \"ModernVS\"\n"+Renderer::modernMeshShader;
            const D3D_SHADER_MACRO macros[]={{"D3DCOMPILER",""},{nullptr,nullptr}};
            ID3DBlob* bytecode=nullptr;ID3DBlob* error=nullptr;
            const auto hr=D3DCompile(source.data(),source.size(),nullptr,macros,&includes,"ModernVS","vs_5_0",
                D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_PACK_MATRIX_ROW_MAJOR,0,&bytecode,&error);
            if(FAILED(hr)) {
                if(error){std::cerr<<static_cast<const char*>(error->GetBufferPointer());error->Release();}
                throw std::runtime_error("Shader compilation failed");
            }
            if(error)error->Release();
            std::string bytes(static_cast<const char*>(bytecode->GetBufferPointer()),bytecode->GetBufferSize());bytecode->Release();
            if(pass==0)reference=bytes;
            else {if(bytes!=reference)throw std::runtime_error("Pass changes vertex bytecode");++compared;}
        }
        std::cout<<"geometry="<<geometry<<" tangent="<<tangent<<" instances="<<instances<<" passes=3 byte_identical=1 bytes="<<reference.size()<<'\n';
    }
    std::cout<<"PASS compilations=36 exact_comparisons="<<compared<<'\n';return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
