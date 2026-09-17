"""Instrument generated copies of three pinned Diligent translation units.

No vendor checkout edits and no changes to the compiler, flags or device calls.
Every replacement is checked so pin changes fail at configure time.
"""
from pathlib import Path
import sys

source, destination = map(Path, sys.argv[1:3])
destination.mkdir(parents=True, exist_ok=True)

def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)

for relative in ('Graphics/GraphicsEngineD3DBase/src/ShaderD3DBase.cpp',
                 'Graphics/GraphicsEngineD3D11/src/ShaderD3D11Impl.cpp',
                 'Graphics/GraphicsEngineD3D11/src/RenderDeviceD3D11Impl.cpp'):
    text=(source/relative).read_text(encoding='utf-8-sig')
    text=replace(text,'namespace Diligent\n{','#include "Renderer/ShaderLoadAudit.h"\n\nnamespace Diligent\n{')
    if relative.endswith('ShaderD3DBase.cpp'):
        text=replace(text,'        RefCntAutoPtr<IFileStream> pSourceStream;',
            '        ShaderLoadAudit::Scope p0lSource("shader-source-include", ShaderLoadAudit::Name(pFileName));\n        RefCntAutoPtr<IFileStream> pSourceStream;')
        text=replace(text,'    return D3DCompile(',
            '    ShaderLoadAudit::Scope p0lCompile("shader-compilation", ShaderLoadAudit::ShaderName(ShaderCI));\n    return D3DCompile(')
        text=replace(text,'            const String HLSLSource = BuildHLSLSourceString(ShaderCI);',
            '            String HLSLSource;\n            { ShaderLoadAudit::Scope p0lSource("shader-source-build", ShaderLoadAudit::ShaderName(ShaderCI)); HLSLSource = BuildHLSLSourceString(ShaderCI); }')
    elif relative.endswith('ShaderD3D11Impl.cpp'):
        text=replace(text,'    std::lock_guard<std::mutex> Lock{m_d3dShaderCacheMtx};',
            '    ShaderLoadAudit::Scope p0lLookup("native-shader-cache-lookup",ShaderLoadAudit::Name(m_Desc.Name));\n    std::lock_guard<std::mutex> Lock{m_d3dShaderCacheMtx};')
        text=replace(text,'    auto it = m_d3dShaderCache.find(BlobKey);',
            '    auto it = m_d3dShaderCache.find(BlobKey);\n    p0lLookup.Stop();\n    MapLoadTrace::Count(it!=m_d3dShaderCache.end()?"native-shader-cache-hit":"native-shader-cache-miss",ShaderLoadAudit::Name(m_Desc.Name));')
    else:
        text=replace(text,'    const ShaderD3D11Impl::CreateInfo D3D11ShaderCI{',
            '    const auto p0lName=ShaderLoadAudit::ShaderName(ShaderCI);\n'
            '    ShaderLoadAudit::ShaderInput(ShaderCI,p0lName);\n'
            '    ShaderLoadAudit::Scope p0lCreate("shader-create",p0lName);\n'
            '    const ShaderD3D11Impl::CreateInfo D3D11ShaderCI{')
        text=replace(text,'    CreateShaderImpl(ppShader, ShaderCI, D3D11ShaderCI);',
            '    CreateShaderImpl(ppShader, ShaderCI, D3D11ShaderCI);\n    p0lCreate.Stop();\n    ShaderLoadAudit::Bytecode(ppShader?*ppShader:nullptr,p0lName);')
        for kind in ('Graphics','Compute'):
            marker=f'void RenderDeviceD3D11Impl::Create{kind}PipelineState(const {kind}PipelineStateCreateInfo& PSOCreateInfo, IPipelineState** ppPipelineState)\n{{'
            text=replace(text,marker,marker+'\n    ShaderLoadAudit::PipelineInput(PSOCreateInfo);\n    ShaderLoadAudit::Scope p0lCreate("pso-creation",ShaderLoadAudit::Name(PSOCreateInfo.PSODesc.Name));')
    output=destination/Path(relative).name
    if not output.exists() or output.read_text(encoding='utf-8')!=text:
        output.write_text(text,encoding='utf-8',newline='\n')
