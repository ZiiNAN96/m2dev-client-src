"""Install the production FXC cache hook in the already prepared pinned copy.

No Diligent vendor or FX source changes; the original compile arguments/flags
remain intact in ShaderBytecodeCache::Compile. Fail closed on upstream drift.
"""
from pathlib import Path
import sys

path = Path(sys.argv[1]) / 'ShaderD3DBase.cpp'
text = path.read_text(encoding='utf-8')
old = ('    ShaderLoadAudit::Scope p0lCompile("shader-compilation", ShaderLoadAudit::ShaderName(ShaderCI));\n'
       '    return D3DCompile(Source, SourceLength, nullptr, Macros, &IncludeImpl, ShaderCI.EntryPoint, profile, dwShaderFlags, 0, ppBlobOut, ppCompilerOutput);')
assert text.count(old) == 1, 'Pinned FXC call changed'
text = text.replace(old, '    return ShaderBytecodeCache::Compile(Source, SourceLength, ShaderCI, profile, dwShaderFlags, Macros, &IncludeImpl, ppBlobOut, ppCompilerOutput);')
text = text.replace('#include "Renderer/ShaderLoadAudit.h"', '#include "Renderer/ShaderLoadAudit.h"\n#include "Renderer/ShaderBytecodeCache.h"')
path.write_text(text, encoding='utf-8', newline='\n')
