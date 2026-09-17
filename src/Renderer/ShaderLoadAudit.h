#pragma once
// Opt-in CPU wall-clock diagnostics, including calls originating inside FX.
// Bytecode equality below is exact; diagnostic hashes are never cache keys.
#include "EterBase/MapLoadTrace.h"
#include "ShaderLoadAuditContext.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsTools/interface/XXH128Hasher.hpp"
#include <sstream>

namespace ShaderLoadAudit {
inline std::string ShaderName(const Diligent::ShaderCreateInfo& ci) {
    if(!MapLoadTrace::state.active)return {};
    std::string result=Name(ci.Desc.Name)+" | "+(ci.EntryPoint?ci.EntryPoint:"")+" | "+(ci.FilePath?ci.FilePath:"");
    result+=" | stage="+std::to_string(unsigned(ci.Desc.ShaderType));
    for(unsigned i=0;i<ci.Macros.Count;++i)result+=" "+std::string(ci.Macros[i].Name)+"="+ci.Macros[i].Definition;
    return result;
}
class Scope : public MapLoadTrace::Scope {
public:
    Scope(const char* kind,const std::string& name):MapLoadTrace::Scope("Shaders / PSOs",name,kind){}
};
template<class T> inline std::string Hash(const T& value) {
    Diligent::XXH128State hasher;hasher.Update(value);const auto h=hasher.Digest();
    std::ostringstream text;text<<std::hex<<h.HighPart<<':'<<h.LowPart;return text.str();
}
inline std::map<std::string,std::string> bytecodes;
inline std::map<std::string,std::string> shaderInputs;
inline std::map<std::string,std::string> pipelineInputs;
inline void ShaderInput(const Diligent::ShaderCreateInfo& ci,const std::string& name) {
    if(!MapLoadTrace::state.active)return;
    Scope timing("audit-shader-key",name);
    auto [it,inserted]=shaderInputs.emplace(Hash(ci),name);
    MapLoadTrace::Count(inserted?"shader-input-new":"shader-input-duplicate",name);
}
inline void Bytecode(Diligent::IShader* shader,const std::string& name) {
    if(!MapLoadTrace::state.active||!shader)return;
    Scope timing("audit-bytecode",name);
    const void* data=nullptr;Diligent::Uint64 size=0;shader->GetBytecode(&data,size);
    if(!data||!size)return;
    auto [it,inserted]=bytecodes.emplace(std::string(static_cast<const char*>(data),size),name);
    MapLoadTrace::Count(inserted?"shader-bytecode-unique":"shader-bytecode-identical",inserted?name:name+" == "+it->second,size);
}
template<class T> inline void PipelineInput(const T& ci) {
    if(!MapLoadTrace::state.active)return;
    const auto name=Name(ci.PSODesc.Name);Scope timing("audit-pso-key",name);
    auto [it,inserted]=pipelineInputs.emplace(Hash(ci),name);
    MapLoadTrace::Count(inserted?"pso-descriptor-new":"pso-descriptor-duplicate",name);
}
inline void CreateSRB(Diligent::IPipelineState* pso,Diligent::IShaderResourceBinding** output,bool initialize) {
    Scope timing("srb-creation",MapLoadTrace::state.active?Name(pso->GetDesc().Name):std::string{});
    pso->CreateShaderResourceBinding(output,initialize);
}
}
