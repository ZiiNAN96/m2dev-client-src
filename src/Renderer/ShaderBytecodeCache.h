#pragma once
// Windows D3D11/FXC integration only; no renderer or portable-core API change.
#include <cstddef>
#include <d3dcompiler.h>

namespace Diligent { struct ShaderCreateInfo; }
namespace ShaderBytecodeCache {
HRESULT Compile(const char* source, std::size_t sourceLength,
                const Diligent::ShaderCreateInfo& shader, const char* profile,
                unsigned flags, const D3D_SHADER_MACRO* macros, ID3DInclude* includes,
                ID3DBlob** bytecode, ID3DBlob** errors);
}
