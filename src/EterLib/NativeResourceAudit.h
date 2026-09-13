#pragma once
#include <d3d9.h>
#include <array>
#include <fstream>
#include "Renderer/ResourceData.h"

namespace Renderer
{
// ZiiNAN: Backend-neutral graphics resource ownership
enum class NativeResourceKind { Texture, CubeTexture, VolumeTexture, VertexBuffer, IndexBuffer,
    Surface, Declaration, VertexShader, PixelShader, DebugMesh, Count };
struct NativeResourceCounter { std::atomic<size_t> attempts{0}, succeeded{0}, blocked{0}; };
inline std::array<NativeResourceCounter,size_t(NativeResourceKind::Count)> nativeResourceCounters;
inline std::atomic<size_t> compatibilityDeviceCreations{0};
inline const char* NativeResourceName(NativeResourceKind kind)
{
    constexpr const char* names[]={"texture","cube","volume","vertex-buffer","index-buffer",
        "surface","declaration","vertex-shader","pixel-shader","debug-mesh-helper"};
    return names[size_t(kind)];
}
template<class Create> HRESULT CreateNativeResource(NativeResourceKind kind, Create&& create, const char* file, int line)
{
    auto& counter=nativeResourceCounters[size_t(kind)]; ++counter.attempts;
    if (UseNeutralResources()) {
        // A missed production path is an explicit failure, never a native resource fallback.
        if (++counter.blocked == 1) {
            std::ofstream log("native-resource-violations.log",std::ios::app);
            log << NativeResourceName(kind) << " blocked at " << file << ':' << line << std::endl;
        }
        return D3DERR_INVALIDCALL;
    }
    const HRESULT result=create();
    if (SUCCEEDED(result)) ++counter.succeeded;
    return result;
}
inline void WriteNativeResourceAudit(std::ostream& output)
{
    size_t attempts=0,successes=0,blocked=0;
    output << "NeutralResourcePath=" << UseNeutralResources() << '\n';
    for(size_t i=0;i<nativeResourceCounters.size();++i) {
        const auto& c=nativeResourceCounters[i];
        attempts+=c.attempts.load(); successes+=c.succeeded.load(); blocked+=c.blocked.load();
        output << NativeResourceName(NativeResourceKind(i)) << " attempts=" << c.attempts
               << " succeeded=" << c.succeeded << " blocked=" << c.blocked << '\n';
    }
    output << "NativeResourceAttempts=" << attempts << " NativeResourceCreations=" << successes
           << " NativeResourceBlocked=" << blocked << '\n'
           << "CompatibilityDeviceCreations=" << compatibilityDeviceCreations
           << "\nLegacyRendererInitializations=0 D3D9DeviceCreations=" << compatibilityDeviceCreations << "\n"
           << "SourceTextures=" << liveSourceTextures << " SourceBuffers=" << liveSourceBuffers << std::endl;
}
}
#define M2_NATIVE_RESOURCE(kind, expression) \
    Renderer::CreateNativeResource(Renderer::NativeResourceKind::kind, [&]() -> HRESULT { return (expression); }, __FILE__, __LINE__)
