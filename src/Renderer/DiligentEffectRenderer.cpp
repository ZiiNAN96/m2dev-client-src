#include "ShaderLoadAudit.h"
// ZiiNAN: Diligent effect rendering integration; original CPU vertices and deterministic material binds.
#include "EterBase/MapLoadTrace.h"
#include "DiligentEffectRenderer.h"
#include "DiligentD3D11BackendInternal.h"
#include "Diagnostics.h"
#include "ModernFrame.h"
#include "FirstUseAudit.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/Sampler.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"
#include "Graphics/GraphicsTools/interface/ShaderMacroHelper.hpp"
#include "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp"
#include <map>
#include <cstring>

namespace Renderer
{
using namespace Diligent;
namespace
{
struct Counts { uint32_t textures=0; };
struct Texture final : TerrainTexture
{
    RefCntAutoPtr<ITexture> image;
    std::shared_ptr<Counts> counts;
    ~Texture() override { if(counts) --counts->textures; }
};
struct Constants
{
    TerrainMatrices matrices;
    std::array<float,16> textureTransform;
    std::array<float,4> factor,fogColor,fogParameters,pixelOffset;
    std::array<uint32_t,4> color,alpha,modes,coordinates;
    std::array<float,16> secondaryTransform;
    std::array<uint32_t,4> secondaryColor,secondaryAlpha;
};
struct UploadVertex { EffectVertex base; std::array<float,2> secondaryUV; };
static_assert(sizeof(UploadVertex)==32);
static_assert(sizeof(Constants)%16==0);
constexpr char source[]=R"(
#if EFFECT_HDR
#include "SRGBUtilities.fxh"
#endif
cbuffer EffectConstants {
 row_major float4x4 World; row_major float4x4 View; row_major float4x4 Projection;
 row_major float4x4 TextureTransform;
 float4 Factor; float4 FogColor; float4 FogParameters; float4 PixelOffset;
 uint4 Color; uint4 Alpha; uint4 Modes; uint4 Coordinates;
 row_major float4x4 SecondaryTransform; uint4 SecondaryColor; uint4 SecondaryAlpha;
};
#if EFFECT_TEXTURE
Texture2D EffectTexture; SamplerState EffectSampler;
#endif
#if SECONDARY_TEXTURE
Texture2D SecondaryTexture; SamplerState SecondarySampler;
#endif
struct Output { float4 position:SV_POSITION; float4 diffuse:COLOR0; float2 uv:TEXCOORD0; float fog:TEXCOORD1; float2 secondaryUV:TEXCOORD2; };
Output VS(float3 position:ATTRIB0,float4 diffuse:ATTRIB1,float2 uv:ATTRIB2,float2 uv2:ATTRIB3) {
 Output o; float4 eye=mul(mul(float4(position,1),World),View); o.position=mul(eye,Projection);
 o.diffuse=diffuse.bgra;
 float4 tex=Coordinates.x!=0 ? eye : float4(uv,1,0);
 o.uv=Coordinates.y!=0 ? mul(tex,TextureTransform).xy : tex.xy;
 float4 second=Coordinates.z==0x20000 ? eye : float4(Coordinates.z==1 ? uv2 : uv,1,0);
 o.secondaryUV=Coordinates.w!=0 ? mul(second,SecondaryTransform).xy : second.xy;
 float d=Modes.y!=0 ? length(eye.xyz) : abs(eye.z); o.fog=1;
 if(Modes.x==1) o.fog=exp(-FogParameters.z*d);
 if(Modes.x==2) o.fog=exp(-pow(FogParameters.z*d,2));
 if(Modes.x==3) o.fog=(FogParameters.y-d)/(FogParameters.y-FogParameters.x);
 o.fog=saturate(o.fog); o.position.xy+=PixelOffset.xy*o.position.w; return o;
}
float4 Argument(uint arg,float4 diffuse,float4 tex) {
 uint sel=arg&15; float4 v=sel<=1 ? diffuse : (sel==2 ? tex : Factor);
 if((arg&32)!=0) v=v.aaaa; if((arg&16)!=0) v=1-v; return v;
}
float4 Operation(uint op,float4 a,float4 b,float4 current) {
 if(op==1) return current; if(op==2) return a; if(op==3) return b;
 if(op==4) return saturate(a*b); if(op==5) return saturate(2*a*b); if(op==6) return saturate(4*a*b);
 if(op==8) return saturate(a+b-0.5);
 // ZiiNAN: Original SkyBox cloud combiner, Renderer::TextureOpModulateInvAlphaAddColor.
 if(op==20) return saturate(a+(1-a.a)*b); return current;
}
bool Compare(uint f,float a,float b) {
 if(f==1) return false; if(f==2) return a<b; if(f==3) return a==b; if(f==4) return a<=b;
 if(f==5) return a>b; if(f==6) return a!=b; if(f==7) return a>=b; return true;
}
float4 SecondArgument(uint arg,float4 diffuse,float4 current,float4 tex) {
 if((arg&15)!=1) return Argument(arg,diffuse,tex);
 float4 v=current; if((arg&32)!=0) v=v.aaaa; if((arg&16)!=0) v=1-v; return v;
}
float4 PS(Output i):SV_TARGET {
 float4 tex=1;
#if EFFECT_TEXTURE
 tex=EffectTexture.Sample(EffectSampler,i.uv);
#endif
 float4 c=i.diffuse;
 if(Color.x!=1 && !(Modes.z==0 && (Color.y&15)==2)) {
  c.rgb=Operation(Color.x,Argument(Color.y,i.diffuse,tex),Argument(Color.z,i.diffuse,tex),i.diffuse).rgb;
  c.a=Operation(Alpha.x,Argument(Alpha.y,i.diffuse,tex),Argument(Alpha.z,i.diffuse,tex),i.diffuse).a;
  // ZiiNAN: Native textured sky with disabled alpha stage writes opaque alpha (GPU oracle).
  if(Alpha.x==1 && Modes.z!=0) c.a=1;
 }
#if SECONDARY_TEXTURE
 {
  float4 t=SecondaryTexture.Sample(SecondarySampler,i.secondaryUV); float4 previous=c;
  c.rgb=Operation(SecondaryColor.x,SecondArgument(SecondaryColor.y,i.diffuse,previous,t),SecondArgument(SecondaryColor.z,i.diffuse,previous,t),previous).rgb;
  c.a=Operation(SecondaryAlpha.x,SecondArgument(SecondaryAlpha.y,i.diffuse,previous,t),SecondArgument(SecondaryAlpha.z,i.diffuse,previous,t),previous).a;
 }
#endif
 if(Modes.w!=0 && !Compare(Alpha.w,floor(saturate(c.a)*255+0.5),float(Color.w))) discard;
#if EFFECT_HDR
 // Additive legacy effects author emission strength, including faint coloured
 // aura tails. Decoding that strength as surface albedo crushes their glow.
 // Keep it in the HDR emission domain; retain the authored blend and alpha.
 // Ordinary alpha surfaces still use the sRGB colour conversion. No world fog.
 if(SecondaryAlpha.w==0) c.rgb=FastSRGBToLinear(c.rgb);
#else
 c.rgb=lerp(FogColor.rgb,c.rgb,i.fog);
#endif
 return c;
}
)";
BLEND_FACTOR Factor(uint32_t value,bool alpha,bool opaque)
{
    if(opaque && value==7) return BLEND_FACTOR_ONE;
    if(opaque && value==8) return BLEND_FACTOR_ZERO;
    switch(value) {
    case 1:return BLEND_FACTOR_ZERO; case 2:return BLEND_FACTOR_ONE;
    case 3:return alpha ? BLEND_FACTOR_SRC_ALPHA : BLEND_FACTOR_SRC_COLOR;
    case 4:return alpha ? BLEND_FACTOR_INV_SRC_ALPHA : BLEND_FACTOR_INV_SRC_COLOR;
    case 5:return BLEND_FACTOR_SRC_ALPHA; case 6:return BLEND_FACTOR_INV_SRC_ALPHA;
    case 7:return BLEND_FACTOR_DEST_ALPHA; case 8:return BLEND_FACTOR_INV_DEST_ALPHA;
    case 9:return alpha ? BLEND_FACTOR_DEST_ALPHA : BLEND_FACTOR_DEST_COLOR;
    case 10:return alpha ? BLEND_FACTOR_INV_DEST_ALPHA : BLEND_FACTOR_INV_DEST_COLOR;
    case 11:return alpha ? BLEND_FACTOR_ONE : (opaque ? BLEND_FACTOR_ZERO : BLEND_FACTOR_SRC_ALPHA_SAT);
    default:return BLEND_FACTOR_ZERO;
    }
}
}
struct DiligentEffectRenderer::Impl
{
    DiligentD3D11Backend& backend;
    RefCntAutoPtr<IBuffer> constants,vertices;
    RefCntAutoPtr<IShader> vs;
    std::array<RefCntAutoPtr<IShader>,8> ps;
    struct Pipeline { RefCntAutoPtr<IPipelineState> state; RefCntAutoPtr<IShaderResourceBinding> bindings; };
    std::map<uint64_t,Pipeline> pipelines;
    std::map<EffectSampler,RefCntAutoPtr<ISampler>> samplers;
    std::shared_ptr<Counts> counts=std::make_shared<Counts>();
    std::array<uint32_t,5> draws{};
    uint64_t vertexCount=0,bytes=0,capacity=0;
    bool failed=false;
    // Failure-only diagnostic: retain the original sticky failure state and draw behavior.
    void Fail(const char* reason, int line) noexcept
    {
        const bool first = !failed;
        failed = true;
        if (first) LogRendererFailure("DiligentEffectRenderer.cpp", this, reason, line);
    }
    explicit Impl(DiligentD3D11Backend& b):backend(b) {}
};
DiligentEffectRenderer::DiligentEffectRenderer(DiligentD3D11Backend& b):m_impl(std::make_unique<Impl>(b)) {}
DiligentEffectRenderer::~DiligentEffectRenderer() { Shutdown(); }
bool DiligentEffectRenderer::Failed() const { return m_impl->failed; }
void DiligentEffectRenderer::ReportFailure() { m_impl->Fail("bridge or wrapper rejected source or legacy state", __LINE__); }
uint32_t DiligentEffectRenderer::DrawCount(EffectPart p) const { return m_impl->draws.at(uint32_t(p)); }
uint64_t DiligentEffectRenderer::Vertices() const { return m_impl->vertexCount; }
uint64_t DiligentEffectRenderer::UploadBytes() const { return m_impl->bytes; }
uint64_t DiligentEffectRenderer::BufferBytes() const { return m_impl->capacity; }
uint32_t DiligentEffectRenderer::LiveTextureCount() const { return m_impl->counts->textures; }
uint32_t DiligentEffectRenderer::LiveBufferCount() const { return unsigned(bool(m_impl->vertices))+unsigned(bool(m_impl->constants)); }
void DiligentEffectRenderer::ResetFrame()
{
    auto& s=*m_impl; s.draws.fill(0); s.vertexCount=s.bytes=0;
    ReleaseBindings();
}
void DiligentEffectRenderer::ReleaseBindings()
{
    auto& s=*m_impl;
    for(auto& entry:s.pipelines) if(entry.second.bindings) {
        for(const char* name:{"EffectTexture","SecondaryTexture"})
            if(auto* variable=entry.second.bindings->GetVariableByName(SHADER_TYPE_PIXEL,name))variable->Set(nullptr);
    }
}
void DiligentEffectRenderer::Shutdown()
{
    auto& s=*m_impl;
    s.pipelines.clear(); s.samplers.clear(); s.vertices.Release(); s.constants.Release(); s.vs.Release(); for(auto& ps:s.ps)ps.Release(); s.capacity=0;
}
bool DiligentEffectRenderer::Initialize()
{
    auto& s=*m_impl; if(!s.backend.m_impl || s.constants) return false;
    try {
        BufferDesc desc; desc.Name="Native effect material and transforms"; desc.Size=sizeof(Constants);
        desc.Usage=USAGE_DYNAMIC; desc.BindFlags=BIND_UNIFORM_BUFFER; desc.CPUAccessFlags=CPU_ACCESS_WRITE;
        { MapLoadTrace::Scope p0lCreate("GPU resources","CreateBuffer","gpu-api"); MapLoadTrace::Count("CreateBuffer","",0,true); s.backend.m_impl->device->CreateBuffer(desc,nullptr,&s.constants); }
        ShaderCreateInfo shader; shader.SourceLanguage=SHADER_SOURCE_LANGUAGE_HLSL; shader.Source=source;
        shader.Desc.Name="Native CPU effect vertices"; shader.Desc.ShaderType=SHADER_TYPE_VERTEX; shader.EntryPoint="VS";
        {FirstUseAudit timing("shader","effect");{ MapLoadTrace::Scope p0lCreate("Shaders / PSOs","CreateShader","cpu"); MapLoadTrace::Count("CreateShader","",0,true); s.backend.m_impl->device->CreateShader(shader,&s.vs); }}
        shader.Desc.Name="Native effect texture factor alpha fog"; shader.Desc.ShaderType=SHADER_TYPE_PIXEL; shader.EntryPoint="PS";
        for(unsigned variant=0;variant<8;++variant) {
            ShaderMacroHelper macros;macros.Add("EFFECT_TEXTURE",bool(variant&1));macros.Add("SECONDARY_TEXTURE",bool(variant&2));
            macros.Add("EFFECT_HDR",bool(variant&4));
            shader.pShaderSourceStreamFactory=&DiligentFXShaderSourceStreamFactory::GetInstance();
            shader.Macros=macros;{FirstUseAudit timing("shader","effect");{ MapLoadTrace::Scope p0lCreate("Shaders / PSOs","CreateShader","cpu"); MapLoadTrace::Count("CreateShader","",0,true); s.backend.m_impl->device->CreateShader(shader,&s.ps[variant]); }}
            if(!s.ps[variant])return false;
        }
        return s.constants && s.vs;
    } catch(...) { s.Fail("initialization exception", __LINE__); return false; }
}
TerrainTexturePtr DiligentEffectRenderer::UploadTexture(const TerrainTextureData& data)
{
    // Reuse the existing decoded-image/mip contract, no effect-specific file decoder.
    auto& s=*m_impl; const auto fail=[&](int line) -> TerrainTexturePtr { s.Fail("texture upload rejected image, mip layout or GPU resource", line); return {}; };
    if(!s.backend.m_impl || !data.width || !data.height || data.width>8192 || data.height>8192 || data.mips.empty()) return fail(__LINE__);
    uint32_t maximum=1; for(auto d=std::max(data.width,data.height);d>1;d>>=1) ++maximum;
    if(data.mips.size()>maximum) return fail(__LINE__);
    TEXTURE_FORMAT format=TEX_FORMAT_UNKNOWN; uint32_t block=0,bytes=4;
    switch(data.format) {
    case TerrainTextureFormat::RGBA8: format=TEX_FORMAT_RGBA8_UNORM; break;
    case TerrainTextureFormat::BGRA8: format=TEX_FORMAT_BGRA8_UNORM; break;
    case TerrainTextureFormat::BGRX8: format=TEX_FORMAT_BGRX8_UNORM; break;
    case TerrainTextureFormat::BC1: format=TEX_FORMAT_BC1_UNORM; block=8; break;
    case TerrainTextureFormat::BC2: format=TEX_FORMAT_BC2_UNORM; block=16; break;
    case TerrainTextureFormat::BC3: format=TEX_FORMAT_BC3_UNORM; block=16; break;
    case TerrainTextureFormat::B5G5R5A1: format=TEX_FORMAT_B5G5R5A1_UNORM; bytes=2; break;
    default:return fail(__LINE__);
    }
    try {
        std::vector<TextureSubResData> mips; auto w=data.width,h=data.height;
        for(const auto& mip:data.mips) {
            const size_t row=block ? size_t((w+3)/4)*block : size_t(w)*bytes,rows=block ? (h+3)/4 : h;
            if(!mip.data || mip.rowStride<row || mip.size<row || (rows-1)>(mip.size-row)/mip.rowStride) return fail(__LINE__);
            TextureSubResData sub; sub.pData=mip.data; sub.Stride=mip.rowStride; mips.push_back(sub);
            w=std::max(1u,w>>1); h=std::max(1u,h>>1);
        }
        auto texture=std::make_shared<Texture>();
        TextureDesc desc; desc.Name="Original effect image"; desc.Type=RESOURCE_DIM_TEX_2D;
        desc.Width=data.width; desc.Height=data.height; desc.MipLevels=uint32_t(mips.size());
        desc.Format=format; desc.Usage=USAGE_IMMUTABLE; desc.BindFlags=BIND_SHADER_RESOURCE;
        TextureData initial{mips.data(),desc.MipLevels}; { MapLoadTrace::Scope p0lCreate("GPU resources","CreateTexture","gpu-api"); MapLoadTrace::Count("CreateTexture","",0,true); s.backend.m_impl->device->CreateTexture(desc,&initial,&texture->image); }
        if(!texture->image) return fail(__LINE__); texture->counts=s.counts; ++s.counts->textures; return texture;
    } catch(...) { return fail(__LINE__); }
}
void DiligentEffectRenderer::Draw(const EffectVertex* vertices,uint32_t count,const TerrainTexturePtr& image,const EffectDraw& d,EffectPart part)
{
    if(shadowCasterCollection)return;
    auto& s=*m_impl; auto texture=std::dynamic_pointer_cast<Texture>(image);
    auto secondary=std::dynamic_pointer_cast<Texture>(d.secondaryTexture);
    if(!s.backend.m_impl || !s.backend.m_impl->inFrame || !s.constants || !vertices || count>UINT32_MAX/sizeof(UploadVertex) ||
       !EffectDrawValid(d,count) || uint32_t(part)>=s.draws.size() || (d.textured && !texture) ||
       (texture && texture->counts!=s.counts) || (d.secondaryTexture && (!secondary || secondary->counts!=s.counts))) {
        s.Fail(!s.backend.m_impl ? "missing effect backend" : !s.backend.m_impl->inFrame ? "effect submitted outside backend frame" :
            !s.constants ? "missing effect constants buffer" : !vertices ? "missing effect vertices" :
            count>UINT32_MAX/sizeof(UploadVertex) ? "effect vertex upload size overflow" : !EffectDrawValid(d,count) ? "invalid effect material or primitive state" :
            uint32_t(part)>=s.draws.size() ? "invalid effect part" : (d.textured && !texture) ? "missing effect texture" :
            (texture && texture->counts!=s.counts) ? "effect texture owner mismatch" : "effect secondary texture type or owner mismatch", __LINE__);
        return;
    }
    for(uint32_t i=0;i<count;++i) {
        for(float f:vertices[i].position) if(!std::isfinite(f)) { s.Fail("non-finite effect vertex position", __LINE__); return; }
        for(float f:vertices[i].uv) if(!std::isfinite(f)) { s.Fail("non-finite effect vertex UV", __LINE__); return; }
    }
    try {
        auto& b=*s.backend.m_impl;
        const bool hdr=!d.ui&&b.modern&&b.modern->HDRWorldActive();
        if(hdr)b.modern->BindWorldTarget();
        uint32_t src=d.src,dst=d.dst;
        if(src==12) { src=5; dst=6; } else if(src==13) { src=6; dst=5; }
        // Original magmabublea.mse uses destination 13; original renderer readback matches INVSRCALPHA.
        // Unlike a source BOTH value, it does not override the other factor. Covered by GPU parity.
        else if(dst==13) dst=6;
        if(dst>11) { s.Fail("unsupported destination blend factor", __LINE__); return; }
        const uint64_t key=uint64_t(d.strip)|(uint64_t(d.blend)<<1)|(uint64_t(d.depthTest)<<2)|(uint64_t(d.depthWrite)<<3)|
            (uint64_t(d.cull)<<4)|(uint64_t(d.depthFunction)<<6)|(uint64_t(src)<<10)|(uint64_t(dst)<<14)|
            (uint64_t(d.blendOp)<<18)|(uint64_t(d.opaqueTargetAlpha)<<21)|(uint64_t(d.lines)<<22)|(uint64_t(d.scissor)<<23)|
            (uint64_t(d.colorWriteMask)<<24)|(uint64_t(d.textured)<<28)|(uint64_t(bool(secondary))<<29)|(uint64_t(hdr)<<30);
        auto& p=s.pipelines[key];
        if(!p.state) {
            GraphicsPipelineStateCreateInfo info;
            info.PSODesc.Name="Deterministic native effect draw"; info.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
            ShaderResourceVariableDesc variables[]={{SHADER_TYPE_PIXEL,"EffectTexture",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {SHADER_TYPE_PIXEL,"EffectSampler",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {SHADER_TYPE_PIXEL,"SecondaryTexture",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {SHADER_TYPE_PIXEL,"SecondarySampler",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
            std::vector<ShaderResourceVariableDesc> activeVariables;
            if(d.textured){activeVariables.push_back(variables[0]);activeVariables.push_back(variables[1]);}
            if(secondary){activeVariables.push_back(variables[2]);activeVariables.push_back(variables[3]);}
            info.PSODesc.ResourceLayout.Variables=activeVariables.data(); info.PSODesc.ResourceLayout.NumVariables=Uint32(activeVariables.size());
            auto& g=info.GraphicsPipeline; const auto& swap=b.swapChain->GetDesc();
            g.NumRenderTargets=1; g.RTVFormats[0]=hdr?TEX_FORMAT_RGBA16_FLOAT:swap.ColorBufferFormat; g.DSVFormat=swap.DepthBufferFormat;
            g.PrimitiveTopology=d.lines ? PRIMITIVE_TOPOLOGY_LINE_LIST : d.strip ? PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            g.RasterizerDesc.CullMode=d.cull==0 ? CULL_MODE_NONE : CULL_MODE_BACK;
            g.RasterizerDesc.FrontCounterClockwise=d.cull==1; g.RasterizerDesc.DepthClipEnable=True;
            g.RasterizerDesc.ScissorEnable=d.scissor;
            g.DepthStencilDesc.DepthEnable=d.depthTest; g.DepthStencilDesc.DepthWriteEnable=d.depthWrite; g.DepthStencilDesc.DepthFunc=COMPARISON_FUNCTION(d.depthFunction);
            auto& blend=g.BlendDesc.RenderTargets[0]; blend.BlendEnable=d.blend;
            blend.RenderTargetWriteMask=COLOR_MASK(d.colorWriteMask);
            blend.SrcBlend=Factor(src,false,d.opaqueTargetAlpha); blend.DestBlend=Factor(dst,false,d.opaqueTargetAlpha);
            blend.SrcBlendAlpha=Factor(src,true,d.opaqueTargetAlpha); blend.DestBlendAlpha=Factor(dst,true,d.opaqueTargetAlpha);
            blend.BlendOp=blend.BlendOpAlpha=BLEND_OPERATION(d.blendOp);
            LayoutElement layout[]={{0,0,3,VT_FLOAT32,False,0,32},{1,0,4,VT_UINT8,True,12,32},{2,0,2,VT_FLOAT32,False,16,32},{3,0,2,VT_FLOAT32,False,24,32}};
            g.InputLayout.LayoutElements=layout; g.InputLayout.NumElements=4; info.pVS=s.vs; info.pPS=s.ps[unsigned(d.textured)|unsigned(bool(secondary))<<1|unsigned(hdr)<<2];
            {FirstUseAudit timing("pso","effect");{ MapLoadTrace::Scope p0lCreate("Shaders / PSOs","CreateGraphicsPipelineState","gpu-api"); MapLoadTrace::Count("CreateGraphicsPipelineState","",0,true); b.device->CreateGraphicsPipelineState(info,&p.state); }} if(!p.state) { s.Fail("effect pipeline creation failed", __LINE__); return; }
            for(auto stage:{SHADER_TYPE_VERTEX,SHADER_TYPE_PIXEL}) if(auto* v=p.state->GetStaticVariableByName(stage,"EffectConstants")) v->Set(s.constants);
            ShaderLoadAudit::CreateSRB(p.state,&p.bindings,true); if(!p.bindings) { s.Fail("effect shader resource binding creation failed", __LINE__); return; }
        }
        const auto getSampler=[&](const EffectSampler& sampling) -> ISampler* {
        auto& sampler=s.samplers[sampling];
        if(!sampler) {
            SamplerDesc desc;
            desc.MinFilter=sampling.min>=2 ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            desc.MagFilter=sampling.mag>=2 ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            desc.MipFilter=sampling.mip==2 ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            if(sampling.min==3 || sampling.mag==3) { desc.MinFilter=desc.MagFilter=desc.MipFilter=FILTER_TYPE_ANISOTROPIC; desc.MaxAnisotropy=sampling.anisotropy; }
            desc.AddressU=TEXTURE_ADDRESS_MODE(sampling.addressU); desc.AddressV=TEXTURE_ADDRESS_MODE(sampling.addressV);
            desc.MinLOD=float(sampling.maxMip); desc.MipLODBias=sampling.lodBias;
            if(!sampling.mip) desc.MaxLOD=desc.MinLOD;
            for(unsigned i=0;i<4;++i) desc.BorderColor[i]=float((sampling.border>>((i==0 ? 2 : i==2 ? 0 : i)*8))&255)/255;
            b.device->CreateSampler(desc,&sampler);
        }
        return sampler;
        };
        auto* sampler=getSampler(d.sampler); auto* secondarySampler=getSampler(d.secondarySampler);
        if(!sampler || !secondarySampler) { s.Fail("effect sampler creation failed", __LINE__); return; }
        const uint64_t bytes=uint64_t(count)*sizeof(UploadVertex);
        if(bytes>s.capacity) {
            s.vertices.Release(); s.capacity=std::max<uint64_t>(4096,bytes);
            BufferDesc desc; desc.Name="CPU effect DISCARD upload"; desc.Size=s.capacity; desc.Usage=USAGE_DYNAMIC;
            desc.BindFlags=BIND_VERTEX_BUFFER; desc.CPUAccessFlags=CPU_ACCESS_WRITE;
            { MapLoadTrace::Scope p0lCreate("GPU resources","CreateBuffer","gpu-api"); MapLoadTrace::Count("CreateBuffer","",0,true); b.device->CreateBuffer(desc,nullptr,&s.vertices); } if(!s.vertices) { s.capacity=0; s.Fail("effect vertex buffer creation failed", __LINE__); return; }
        }
        { MapHelper<UploadVertex> mapped(b.context,s.vertices,MAP_WRITE,MAP_FLAG_DISCARD);
          if(!mapped) { s.Fail("effect vertex buffer map failed", __LINE__); return; }
          for(uint32_t i=0;i<count;++i) { mapped[i].base=vertices[i]; mapped[i].secondaryUV=d.secondaryUV.empty() ? vertices[i].uv : d.secondaryUV[i]; }
        }
        {
            MapHelper<Constants> mapped(b.context,s.constants,MAP_WRITE,MAP_FLAG_DISCARD);
            if(!mapped) { s.Fail("effect constants buffer map failed", __LINE__); return; }
            mapped->matrices=d.matrices; mapped->textureTransform=d.textureTransform; mapped->factor=d.factor;
            mapped->fogColor=d.fogColor; mapped->fogParameters=d.fogParameters;
            const auto& swap=b.swapChain->GetDesc();
            mapped->pixelOffset={1.0f/(d.ui ? d.viewport[2] : swap.Width),-1.0f/(d.ui ? d.viewport[3] : swap.Height),0,0};
            mapped->color={d.colorOp,d.colorArg1,d.colorArg2,d.alphaReference}; mapped->alpha={d.alphaOp,d.alphaArg1,d.alphaArg2,d.alphaFunction};
            mapped->modes={d.fog,uint32_t(d.rangeFog),uint32_t(d.textured),uint32_t(d.alphaTest)};
            mapped->coordinates={d.textureCoordinates,d.textureTransformFlags,d.secondaryCoordinates,d.secondaryTransformFlags};
            mapped->secondaryTransform=d.secondaryTransform;
            mapped->secondaryColor={d.secondaryColorOp,d.secondaryColorArg1,d.secondaryColorArg2,uint32_t(bool(secondary))};
            mapped->secondaryAlpha={d.secondaryAlphaOp,d.secondaryAlphaArg1,d.secondaryAlphaArg2,
                uint32_t(hdr&&d.blend&&dst==2&&d.blendOp==1)};
        }
        if(d.textured) {
            p.bindings->GetVariableByName(SHADER_TYPE_PIXEL,"EffectTexture")->Set(texture->image->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            p.bindings->GetVariableByName(SHADER_TYPE_PIXEL,"EffectSampler")->Set(sampler);
        }
        if(secondary) {
            p.bindings->GetVariableByName(SHADER_TYPE_PIXEL,"SecondaryTexture")->Set(secondary->image->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            p.bindings->GetVariableByName(SHADER_TYPE_PIXEL,"SecondarySampler")->Set(secondarySampler);
        }
        b.context->SetPipelineState(p.state);
        // ZiiNAN: UI never inherits a world viewport or a previous widget's scissor state.
        if(d.ui) {
            Viewport viewport(float(d.viewport[0]),float(d.viewport[1]),float(d.viewport[2]),float(d.viewport[3]),0,1);
            const auto& swap=b.swapChain->GetDesc();
            b.context->SetViewports(1,&viewport,swap.Width,swap.Height);
            if(d.scissor) {
                Rect rect(d.clip[0],d.clip[1],d.clip[2],d.clip[3]);
                b.context->SetScissorRects(1,&rect,swap.Width,swap.Height);
            }
        } else {
            const auto& swap=b.swapChain->GetDesc(); Viewport viewport(0,0,float(swap.Width),float(swap.Height),0,1);
            b.context->SetViewports(1,&viewport,swap.Width,swap.Height);
        }
        IBuffer* vb=s.vertices; Uint64 offset=0;
        b.context->SetVertexBuffers(0,1,&vb,&offset,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,SET_VERTEX_BUFFERS_FLAG_RESET);
        b.context->SetIndexBuffer(nullptr,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        b.context->CommitShaderResources(p.bindings,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        b.context->Draw(DrawAttribs{count,DRAW_FLAG_VERIFY_ALL});
        ++s.draws[uint32_t(part)]; s.vertexCount+=count; s.bytes+=bytes;
    } catch(...) { s.Fail("effect draw exception", __LINE__); }
}
}
