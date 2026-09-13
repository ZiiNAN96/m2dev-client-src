#pragma once
// ZiiNAN: Native D3D9 fixed-function output is the effect material oracle.
#include "Renderer/DiligentEffectRenderer.h"

static void EffectGpuChecks(Renderer::LegacyD3D9Backend& legacy,Renderer::DiligentD3D11Backend& modern)
{
    using namespace Renderer;
    DiligentEffectRenderer effects(modern); Check(effects.Initialize(),"effect shader initialization");
    auto* device=STATEMANAGER.GetDevice();
    const auto bytes=TerrainFixture::AlphaDDS();
    auto texture=LoadStaticObjectTextureMemory(bytes.data(),bytes.size(),effects);
    auto native=LegacyProbe::Texture(bytes);
    const std::array<float,16> identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    for(unsigned test=0;test<70;++test) {
        const unsigned width=test%2 ? 192 : 160,height=test%2 ? 144 : 120;
        Check(legacy.Resize(width,height) && modern.Resize(width,height),"effect resize");
        if(test%10==0) Check(modern.Resize(0,0) && !modern.BeginFrame() && modern.Resize(width,height),"effect suspend restore");
        ObjectLegacyProbe::AlphaTarget alphaTarget(width,height);
        EffectDraw d; d.matrices={identity,identity,identity}; d.textureTransform=identity; d.factor={.6f,.8f,.4f,.65f};
        d.strip=test%2==0; d.depthWrite=test%3==0; d.blend=test!=0; d.sampler.min=d.sampler.mag=test%2 ? 1 : 2;
        if(test<10) { d.src=1+test; d.dst=6; }
        else if(test<20) { d.src=5; d.dst=1+(test-10); }
        else if(test<26) { const uint32_t ops[]={2,3,4,5,6,8}; d.colorOp=ops[test-20]; }
        else if(test<34) { d.alphaTest=true; d.alphaReference=80; d.alphaFunction=1+test-26; }
        else if(test<38) { d.textured=false; d.colorOp=d.alphaOp=2; d.colorArg1=d.alphaArg1=test==36 ? 2 : 0; d.src=5; d.dst=test==35 ? 2 : 6; }
        else if(test<42) { d.fog=test==38 ? 1 : test==39 ? 2 : 3; d.rangeFog=test==41; d.fogParameters={0,1,2,0}; d.fogColor={.2f,.3f,.4f,1}; }
        else if(test<46) { d.sampler.addressU=d.sampler.addressV=test-40; }
        else if(test==46) { d.src=12; d.dst=2; }
        else if(test==47) { d.src=13; d.dst=2; }
        else if(test==48) { d.blend=false; d.colorOp=4; d.colorArg1=0; d.alphaArg1=0; }
        else if(test==49) { d.sampler.min=d.sampler.mag=0; }
        else if(test==50) { d.textureTransformFlags=2; d.textureTransform[0]=2; d.textureTransform[5]=3; d.textureTransform[12]=.1f; }
        else if(test==51) { d.depthTest=false; d.depthWrite=true; }
        else if(test==52) { d.alphaOp=2; d.alphaArg1=3; d.blend=false; }
        else if(test==53) { d.cull=2; d.blend=false; }
        else if(test<57) { d.src=test==54 ? 2 : test==55 ? 5 : 9; d.dst=13; }
        else if(test==57) { d.src=11; d.dst=6; }
        else if(test<62) { d.blendOp=2+test-58; }
        else {
            // ZiiNAN: Guild projected alpha, minimap cover and dungeon UV1/lightmap.
            d.secondaryTexture=texture; d.secondaryTransform=identity;
            d.secondarySampler.min=d.secondarySampler.mag=test%2 ? 1 : 2;
            d.secondaryColorOp=test%3==0 ? 3 : 4;
            d.secondaryAlphaOp=4; d.secondaryCoordinates=test<66 ? 0x20000 : 1;
            d.secondaryTransformFlags=test<66 ? 2 : 0;
            d.secondaryTransform[0]=.4f; d.secondaryTransform[5]=.4f;
            d.secondaryTransform[12]=.5f; d.secondaryTransform[13]=.5f;
        }
        std::vector<EffectVertex> v;
        const EffectVertex quad[]={{{-.8f,-.8f,.5f},0xa099cc66,{-.1f,.2f}},{{-.8f,.8f,.5f},0x70cc9966,{-.1f,1.3f}},
            {{.8f,-.8f,.5f},0xe06699cc,{1.2f,.2f}},{{.8f,.8f,.5f},0x80cc6699,{1.2f,1.3f}}};
        if(d.strip) v.assign(quad,quad+4); else for(unsigned i:{0u,1u,2u,2u,1u,3u}) v.push_back(quad[i]);
        struct Vertex2 { EffectVertex base; std::array<float,2> uv1; };
        std::vector<Vertex2> v2;
        for(const auto& vertex:v) { v2.push_back({vertex,{1-vertex.uv[0],1-vertex.uv[1]}}); }
        if(d.secondaryCoordinates==1) for(const auto& vertex:v2) d.secondaryUV.push_back(vertex.uv1);
        Check(legacy.BeginFrame() && modern.BeginFrame(),"effect begin frame");
        const ClearColor background={.1f,.2f,.3f,.4f}; legacy.Clear({true,background}); modern.Clear({true,background}); effects.ResetFrame();
        const auto rs=[&](D3DRENDERSTATETYPE type,DWORD value) { Check(SUCCEEDED(device->SetRenderState(type,value)),"effect native render state"); };
        const auto ts=[&](D3DTEXTURESTAGESTATETYPE type,DWORD value) { Check(SUCCEEDED(device->SetTextureStageState(0,type,value)),"effect native texture state"); };
        const auto ss=[&](D3DSAMPLERSTATETYPE type,DWORD value) { Check(SUCCEEDED(device->SetSamplerState(0,type,value)),"effect native sampler state"); };
        for(auto type:{D3DRS_LIGHTING,D3DRS_SPECULARENABLE,D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_STENCILENABLE,D3DRS_SCISSORTESTENABLE}) rs(type,FALSE);
        rs(D3DRS_COLORWRITEENABLE,15); rs(D3DRS_FILLMODE,D3DFILL_SOLID); rs(D3DRS_SHADEMODE,D3DSHADE_GOURAUD);
        rs(D3DRS_ZENABLE,d.depthTest); rs(D3DRS_ZWRITEENABLE,d.depthWrite); rs(D3DRS_ZFUNC,d.depthFunction); rs(D3DRS_CULLMODE,d.cull+1);
        rs(D3DRS_ALPHABLENDENABLE,d.blend); rs(D3DRS_SRCBLEND,d.src); rs(D3DRS_DESTBLEND,d.dst); rs(D3DRS_BLENDOP,d.blendOp);
        rs(D3DRS_ALPHATESTENABLE,d.alphaTest); rs(D3DRS_ALPHAFUNC,d.alphaFunction); rs(D3DRS_ALPHAREF,d.alphaReference);
        rs(D3DRS_TEXTUREFACTOR,D3DCOLOR_COLORVALUE(d.factor[0],d.factor[1],d.factor[2],d.factor[3]));
        // Use quantized native TFACTOR, not a different float material color in the reference shader.
        D3DXCOLOR quantized(D3DCOLOR_COLORVALUE(d.factor[0],d.factor[1],d.factor[2],d.factor[3])); d.factor={quantized.r,quantized.g,quantized.b,quantized.a};
        ts(D3DTSS_COLOROP,d.colorOp); ts(D3DTSS_COLORARG1,d.colorArg1); ts(D3DTSS_COLORARG2,d.colorArg2);
        ts(D3DTSS_ALPHAOP,d.alphaOp); ts(D3DTSS_ALPHAARG1,d.alphaArg1); ts(D3DTSS_ALPHAARG2,d.alphaArg2);
        ts(D3DTSS_TEXCOORDINDEX,d.textureCoordinates); ts(D3DTSS_TEXTURETRANSFORMFLAGS,d.textureTransformFlags);
        device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE); device->SetTexture(1,nullptr);
        device->SetTexture(0,d.textured ? native.Get() : nullptr);
        ss(D3DSAMP_ADDRESSU,d.sampler.addressU); ss(D3DSAMP_ADDRESSV,d.sampler.addressV);
        ss(D3DSAMP_MINFILTER,d.sampler.min); ss(D3DSAMP_MAGFILTER,d.sampler.mag); ss(D3DSAMP_MIPFILTER,d.sampler.mip);
        ss(D3DSAMP_MAXMIPLEVEL,0); ss(D3DSAMP_MIPMAPLODBIAS,0); ss(D3DSAMP_BORDERCOLOR,0);
        rs(D3DRS_FOGENABLE,d.fog!=0); rs(D3DRS_FOGVERTEXMODE,d.fog); rs(D3DRS_FOGTABLEMODE,D3DFOG_NONE); rs(D3DRS_RANGEFOGENABLE,d.rangeFog);
        rs(D3DRS_FOGCOLOR,D3DCOLOR_COLORVALUE(.2f,.3f,.4f,1));
        const auto bits=[](float f) { DWORD b; memcpy(&b,&f,4); return b; };
        rs(D3DRS_FOGSTART,bits(d.fogParameters[0])); rs(D3DRS_FOGEND,bits(d.fogParameters[1])); rs(D3DRS_FOGDENSITY,bits(d.fogParameters[2]));
        for(auto entry:{std::pair<D3DTRANSFORMSTATETYPE,const std::array<float,16>*>(D3DTS_WORLD,&d.matrices.world),
            {D3DTS_VIEW,&d.matrices.view},{D3DTS_PROJECTION,&d.matrices.projection},{D3DTS_TEXTURE0,&d.textureTransform}})
            Check(SUCCEEDED(device->SetTransform(entry.first,reinterpret_cast<const D3DMATRIX*>(entry.second->data()))),"effect native transforms");
        device->SetVertexShader(nullptr); device->SetPixelShader(nullptr); device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        if(d.secondaryTexture) {
            device->SetTexture(1,native.Get()); device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX2);
            for(auto entry:{std::pair{D3DTSS_COLOROP,d.secondaryColorOp},{D3DTSS_COLORARG1,d.secondaryColorArg1},
                {D3DTSS_COLORARG2,d.secondaryColorArg2},{D3DTSS_ALPHAOP,d.secondaryAlphaOp},
                {D3DTSS_ALPHAARG1,d.secondaryAlphaArg1},{D3DTSS_ALPHAARG2,d.secondaryAlphaArg2},
                {D3DTSS_TEXCOORDINDEX,d.secondaryCoordinates},{D3DTSS_TEXTURETRANSFORMFLAGS,d.secondaryTransformFlags}})
                device->SetTextureStageState(1,entry.first,entry.second);
            device->SetTextureStageState(2,D3DTSS_COLOROP,D3DTOP_DISABLE);
            const auto& s=d.secondarySampler;
            for(auto entry:{std::pair{D3DSAMP_ADDRESSU,s.addressU},{D3DSAMP_ADDRESSV,s.addressV},
                {D3DSAMP_MINFILTER,s.min},{D3DSAMP_MAGFILTER,s.mag},{D3DSAMP_MIPFILTER,s.mip}})
                device->SetSamplerState(1,entry.first,entry.second);
            device->SetTransform(D3DTS_TEXTURE1,reinterpret_cast<const D3DMATRIX*>(d.secondaryTransform.data()));
        }
        Check(SUCCEEDED(device->DrawPrimitiveUP(d.strip ? D3DPT_TRIANGLESTRIP : D3DPT_TRIANGLELIST,2,
            d.secondaryTexture ? static_cast<const void*>(v2.data()) : static_cast<const void*>(v.data()),d.secondaryTexture ? 32 : 24)),"effect native draw");
        effects.Draw(v.data(),uint32_t(v.size()),d.textured ? texture : TerrainTexturePtr{},d,EffectPart::Particle);
        Check(!effects.Failed() && effects.DrawCount(EffectPart::Particle)==1,"effect material submission");
        legacy.EndFrame(); modern.EndFrame();
        const auto a=LegacyProbe::Read(width,height),b=BackendTestAccess::Read(modern,false),depth=BackendTestAccess::Read(modern,true);
        double error=0; size_t compared=0,bad=0,depthErrors=0;
        for(unsigned y=height/5;y<height*4/5;++y) for(unsigned x=width/5;x<width*4/5;++x) {
            const size_t i=y*width+x; ++compared;
            for(unsigned c=0;c<4;++c) { const int delta=std::abs(int((a[i]>>((c==3 ? 3 : 2-c)*8))&255)-int((b[i]>>(c*8))&255)); error+=delta; if(delta>4) ++bad; }
            if(!d.alphaTest && d.cull==0 && ((depth[i]&0xffffff)!=0xffffff)!=(d.depthTest && d.depthWrite)) ++depthErrors;
        }
        std::cout<<"Effect native case="<<test<<" mean="<<error/(compared*4)<<" channel-errors="<<bad<<" depth-errors="<<depthErrors<<'\n';
        SaveSplatReadback(a,width,height,false,"effect-case"+std::to_string(test)+"-d3d9");
        SaveSplatReadback(b,width,height,true,"effect-case"+std::to_string(test)+"-d3d11");
        Check(error/(compared*4)<1.5 && bad<compared/100+1 && !depthErrors,"native effect color alpha depth parity");
        legacy.Present(); modern.Present();
    }
    // Poison PSO, sampler, alpha and constants before an opaque draw; do not globally reset state.
    EffectDraw draw; draw.matrices={identity,identity,identity}; draw.textureTransform=identity;
    draw.textured=false; draw.colorOp=draw.alphaOp=2; draw.colorArg1=draw.alphaArg1=0; draw.blend=false;
    std::vector<EffectVertex> large(6000); for(size_t i=0;i<large.size();++i) large[i]={{{-.5f+float(i%3)*.5f,-.5f+float(i%2),.2f}},0xff22bb77,{0,0}};
    draw.strip=false;
    Check(modern.BeginFrame(),"effect growth frame"); effects.ResetFrame(); modern.Clear({true,ClearColor{0,0,0,1}});
    effects.Draw(large.data(),uint32_t(large.size()),{},draw,EffectPart::Mesh);
    // ZiiNAN: Internal M11 upload adds UV1; the native CPU EffectVertex remains 24 bytes.
    Check(sizeof(EffectVertex)==24 && effects.BufferBytes()>=large.size()*32 && effects.UploadBytes()==large.size()*32,"effect dynamic buffer growth/byte accounting");
    modern.EndFrame();
    const auto clean=BackendTestAccess::Read(modern,false);
    Check(modern.BeginFrame(),"effect state isolation frame"); modern.Clear({true,ClearColor{0,0,0,1}});
    auto poison=draw; poison.blend=true; poison.src=2; poison.dst=2; poison.alphaTest=true; poison.alphaReference=255; poison.cull=2; poison.depthWrite=true;
    poison.secondaryTexture=texture; poison.secondaryTransform=identity;
    poison.secondaryColorOp=4; poison.secondaryAlphaOp=4;
    effects.Draw(large.data(),3,{},poison,EffectPart::FlyTrace);
    effects.Draw(large.data(),uint32_t(large.size()),{},draw,EffectPart::Mesh); modern.EndFrame();
    Check(clean==BackendTestAccess::Read(modern,false),"effect predecessor-independent material state");
    poison.secondaryTexture.reset();
    effects.ResetFrame(); texture.reset(); Check(effects.LiveTextureCount()==0,"effect texture released after end");
    effects.Shutdown(); Check(effects.LiveBufferCount()==0 && effects.LiveTextureCount()==0 && !effects.Failed(),"effect shutdown resources zero");
    std::cout<<"Effect native parity / growth / state isolation / texture lifetime / shutdown: PASS\n";
}
