#pragma once
#include "Renderer/ResourceData.h"
#include <ostream>

namespace Renderer
{
// ZiiNAN: Removed final D3D9 compile-time dependency. Report actual CPU owners only.
inline void WriteSourceResourceAudit(std::ostream& output)
{
    output << "SourceTextures=" << liveSourceTextures
           << " SourceBuffers=" << liveSourceBuffers << std::endl;
}
}
