#pragma once
// ZiiNAN: Diligent SpeedTree rendering integration; native fixed function and original VS1.1 as oracle.
#include "Renderer/DiligentTreeRenderer.h"
#include "SpeedTreeLib/SpeedTreeWrapper.h"
#include "SpeedTreeLib/VertexShaders.h"

static void TreeGpuChecks(LegacyProbe& screen,Renderer::LegacyD3D9Backend& legacy,Renderer::DiligentD3D11Backend& modern)
{
    using namespace Renderer;
    DiligentTreeRenderer trees(modern); Check(trees.Initialize(),"tree shader initialization");
    auto* device=STATEMANAGER.GetDevice();
    Microsoft::WRL::ComPtr<IDirect3DVertexDeclaration9> leafDeclaration;
    Microsoft::WRL::ComPtr<IDirect3DVertexShader9> leafShader;
    LPDIRECT3DVERTEXDECLARATION9 decl=nullptr; LPDIRECT3DVERTEXSHADER9 shader=nullptr;
    LoadLeafShader(device,decl,shader); leafDeclaration.Attach(decl); leafShader.Attach(shader);
    Check(leafDeclaration && leafShader,"original SpeedTree leaf shader");
    auto bytes=TerrainFixture::AlphaDDS(),shadowBytes=TerrainFixture::GradientDDS();
    auto texture=LoadStaticObjectTextureMemory(bytes.data(),bytes.size(),trees);
    auto second=LoadStaticObjectTextureMemory(shadowBytes.data(),shadowBytes.size(),trees);
    auto nativeTexture=LegacyProbe::Texture(bytes),nativeSecond=LegacyProbe::Texture(shadowBytes);
    for(unsigned test=0;test<24;++test) {
        const uint32_t width=test%2 ? 800 : 640,height=test%2 ? 600 : 480;
        Check(legacy.Resize(width,height) && modern.Resize(width,height),"tree repeated resize");
        screen.SetPositionCamera(1600,-1600,0,7000,45,float((test%4)*35));
        screen.SetPerspective(30,float(width)/height,100,25600);
        TreeDraw draw; draw.matrices=screen.Matrices(); draw.part=test>=10 && test<18 ? TreePart::Leaf : test>=18 ? TreePart::Billboard : test>=4 ? TreePart::Frond : TreePart::Branch;
        draw.strip=uint32_t(draw.part)<2; draw.cull=draw.part==TreePart::Branch ? 1 : 0;
        draw.alphaReference=test==2 ? 127 : test==3 ? 255 : 84; draw.alphaTest=test!=0;
        draw.blend=test==7 || test==8 || test==9 || test==13; draw.depthWrite=test!=8;
        draw.stage1=(test==4 || test==5) ? 1 : (test==6 || test==7 || test==8 || test==9) ? 2 : 0;
        draw.cameraCoordinates=test==9;
        if(test>=20) { draw.part=TreePart::Leaf; draw.strip=false; draw.cull=0; draw.stage1=2;
            draw.cameraCoordinates=true; draw.blend=true; draw.depthWrite=test!=21; }
        draw.samplers[0].sampling={true,true,test%2!=0,test%2!=0,false,false};
        draw.samplers[1].sampling={test!=9,test!=9,false,false,false,false};
        TerrainTexturePtr cameraMask;
        Microsoft::WRL::ComPtr<IDirect3DTexture9> nativeMask;
        if(test>=20) {
            auto maskBytes=TerrainFixture::AlphaDDS();
            const uint8_t cornerAlpha[]={0,128,255,192};
            for(unsigned y=0;y<32;++y) for(unsigned x=0;x<4;++x) maskBytes[128+(y*32+x)*4+3]=cornerAlpha[test-20];
            cameraMask=LoadStaticObjectTextureMemory(maskBytes.data(),maskBytes.size(),trees);
            nativeMask=LegacyProbe::Texture(maskBytes);
            draw.samplers[1].sampling={false,false,test==23,test==23,false,false};
        }
        if(test==5) draw.samplers[0].sampling={true,true,true,true,true,true};
        draw.fog=test==1 ? 3 : test==12 || test==16 ? 4 : 0;
        draw.fogParameters={3000,12000,.0002f,0}; draw.fogColor={.2f,.3f,.4f,1};
        TreeSource data;
        const std::array<std::array<float,3>,4> points={{{0,0,0},{0,-3200,0},{3200,-3200,0},{3200,0,0}}};
        const unsigned corners[]={0,1,2,0,2,3};
        if(draw.strip) {
            for(unsigned k:{0u,1u,3u,2u}) { TreeVertex v; v.position=points[k]; v.color=0xd09fcd67; v.uv={v.position[0]/2500,-v.position[1]/2500}; v.shadowUv={v.uv[1],v.uv[0]}; data.vertices.push_back(v); }
            data.indices={0,1,2,3}; draw.count=4;
        } else {
            for(auto k:corners) { TreeVertex v; v.position=draw.part==TreePart::Leaf ? std::array<float,3>{0,0,0} : points[k];
                v.color=draw.part==TreePart::Billboard ? 0xffffffff : 0xd09fcd67;
                v.uv={points[k][0]/2500,-points[k][1]/2500}; v.leaf={float(4+k),test==15 ? .8f : 1.0f}; data.vertices.push_back(v); }
            draw.count=6;
        }
        D3DXMATRIX world,view,projection,combined,transposed,mask;
        memcpy(&world,draw.matrices.world.data(),64); memcpy(&view,draw.matrices.view.data(),64); memcpy(&projection,draw.matrices.projection.data(),64);
        world._41=test==17 ? 220 : 0; world._43=test==17 ? 100 : 0; memcpy(draw.matrices.world.data(),&world,64);
        D3DXMatrixMultiply(&combined,&view,&projection); D3DXMatrixTranspose(&transposed,&combined);
        memcpy(draw.legacyConstants.data(),&transposed,64);
        for(unsigned k=0;k<4;++k) draw.legacyConstants[4+k]={points[k][0],points[k][1],test==14 ? float(k*65) : 0,0};
        draw.legacyConstants[52]={world._41,world._42,world._43,0}; draw.legacyConstants[85]={3000,12000,1.0f/9000,0};
        D3DXMatrixIdentity(&mask); mask._11=mask._22=.0003f; mask._41=mask._42=.5f;
        memcpy(draw.textureTransform.data(),&mask,64);
        auto geometry=trees.UploadGeometry(data,draw.part==TreePart::Billboard); Check(geometry!=nullptr,"tree geometry");
        ObjectLegacyProbe::AlphaTarget alphaTarget(width,height);
        Check(legacy.BeginFrame() && modern.BeginFrame(),"tree frame");
        legacy.Clear({true,ClearColor{0,0,0,1}}); modern.Clear({true,ClearColor{0,0,0,1}}); trees.ResetFrame();
        if(draw.part==TreePart::Billboard) Check(trees.UpdateVertices(geometry,data.vertices),"native billboard update");
        STATEMANAGER.SetTransform(D3DTS_WORLD,&world); STATEMANAGER.SetTransform(D3DTS_VIEW,&view); STATEMANAGER.SetTransform(D3DTS_PROJECTION,&projection);
        STATEMANAGER.SetTransform(D3DTS_TEXTURE1,&mask);
        STATEMANAGER.SetVertexShader(nullptr); STATEMANAGER.SetPixelShader(nullptr);
        // Native ResetEx preserves driver states while the legacy cache is zeroed.
        for(auto state:{D3DRS_ALPHATESTENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_FOGENABLE,D3DRS_LIGHTING,
                        D3DRS_RANGEFOGENABLE,D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_SPECULARENABLE}) {
            STATEMANAGER.SetRenderState(state,TRUE); STATEMANAGER.SetRenderState(state,FALSE);
        }
        STATEMANAGER.SetRenderState(D3DRS_LIGHTING,FALSE); STATEMANAGER.SetRenderState(D3DRS_COLORVERTEX,TRUE);
        STATEMANAGER.SetRenderState(D3DRS_ZENABLE,TRUE); STATEMANAGER.SetRenderState(D3DRS_ZWRITEENABLE,draw.depthWrite);
        STATEMANAGER.SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL); STATEMANAGER.SetRenderState(D3DRS_CULLMODE,draw.cull+1);
        STATEMANAGER.SetRenderState(D3DRS_ALPHATESTENABLE,draw.alphaTest); STATEMANAGER.SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER);
        STATEMANAGER.SetRenderState(D3DRS_ALPHAREF,draw.alphaReference); STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE,draw.blend);
        STATEMANAGER.SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA); STATEMANAGER.SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
        STATEMANAGER.SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD); STATEMANAGER.SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
        STATEMANAGER.SetRenderState(D3DRS_FOGENABLE,draw.fog!=0); STATEMANAGER.SetRenderState(D3DRS_FOGTABLEMODE,D3DFOG_NONE);
        STATEMANAGER.SetRenderState(D3DRS_FOGVERTEXMODE,draw.fog==4 ? D3DFOG_NONE : draw.fog);
        STATEMANAGER.SetRenderState(D3DRS_RANGEFOGENABLE,FALSE);
        float nearFog=3000,farFog=12000; DWORD nearBits,farBits; memcpy(&nearBits,&nearFog,4); memcpy(&farBits,&farFog,4);
        STATEMANAGER.SetRenderState(D3DRS_FOGSTART,nearBits); STATEMANAGER.SetRenderState(D3DRS_FOGEND,farBits);
        STATEMANAGER.SetRenderState(D3DRS_FOGCOLOR,D3DCOLOR_COLORVALUE(.2f,.3f,.4f,1));
        STATEMANAGER.SetTexture(0,nativeTexture.Get()); STATEMANAGER.SetTexture(1,draw.stage1 ? (test>=20 ? nativeMask.Get() : nativeSecond.Get()) : nullptr);
        for(unsigned stage=0;stage<2;++stage) {
            const auto& sample=draw.samplers[stage].sampling;
            STATEMANAGER.SetSamplerState(stage,D3DSAMP_ADDRESSU,sample.wrapU ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
            STATEMANAGER.SetSamplerState(stage,D3DSAMP_ADDRESSV,sample.wrapV ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
            STATEMANAGER.SetSamplerState(stage,D3DSAMP_MINFILTER,sample.linearMin ? D3DTEXF_LINEAR : D3DTEXF_POINT);
            STATEMANAGER.SetSamplerState(stage,D3DSAMP_MAGFILTER,sample.linearMag ? D3DTEXF_LINEAR : D3DTEXF_POINT);
            STATEMANAGER.SetSamplerState(stage,D3DSAMP_MIPFILTER,sample.useMips ? (sample.linearMip ? D3DTEXF_LINEAR : D3DTEXF_POINT) : D3DTEXF_NONE);
            STATEMANAGER.SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,stage==1 && draw.cameraCoordinates ? D3DTSS_TCI_CAMERASPACEPOSITION : stage);
            STATEMANAGER.SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,stage==1 && draw.cameraCoordinates ? D3DTTFF_COUNT2 : D3DTTFF_DISABLE);
        }
        STATEMANAGER.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE); STATEMANAGER.SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE); STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE); STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,draw.stage1==1 ? D3DTOP_MODULATE : draw.stage1==2 ? D3DTOP_SELECTARG1 : D3DTOP_DISABLE);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLORARG1,draw.stage1==1 ? D3DTA_TEXTURE : D3DTA_CURRENT);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLORARG2,D3DTA_CURRENT); STATEMANAGER.SetTextureStageState(1,D3DTSS_ALPHAOP,draw.stage1==2 ? D3DTOP_MODULATE : D3DTOP_DISABLE);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_TEXTURE); STATEMANAGER.SetTextureStageState(1,D3DTSS_ALPHAARG2,D3DTA_CURRENT);
        STATEMANAGER.SetTextureStageState(2,D3DTSS_COLOROP,D3DTOP_DISABLE);
        if(draw.part==TreePart::Leaf) {
            std::vector<SFVFLeafVertex> native(data.vertices.size());
            for(size_t i=0;i<native.size();++i) { const auto& v=data.vertices[i]; auto& n=native[i];
                memcpy(&n.m_vPosition,v.position.data(),12); n.m_dwDiffuseColor=v.color; memcpy(n.m_fTexCoords,v.uv.data(),8);
                n.m_fLeafPlacementIndex=v.leaf[0]; n.m_fLeafScalarValue=v.leaf[1]; }
            STATEMANAGER.SetVertexDeclaration(leafDeclaration.Get()); STATEMANAGER.SetVertexShader(leafShader.Get());
            STATEMANAGER.SetVertexShaderConstant(0,draw.legacyConstants.data(),96);
            STATEMANAGER.DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,native.data(),sizeof(SFVFLeafVertex));
        } else {
            std::vector<SFVFBranchVertex> native(data.vertices.size());
            for(size_t i=0;i<native.size();++i) { const auto& v=data.vertices[i]; auto& n=native[i];
                memcpy(&n.m_vPosition,v.position.data(),12); n.m_dwDiffuseColor=v.color;
                memcpy(n.m_fTexCoords,v.uv.data(),8); memcpy(n.m_fShadowCoords,v.shadowUv.data(),8); }
            STATEMANAGER.SetFVF(D3DFVF_SPEEDTREE_BRANCH_VERTEX);
            if(draw.strip) STATEMANAGER.DrawIndexedPrimitiveUP(D3DPT_TRIANGLESTRIP,0,4,2,data.indices.data(),D3DFMT_INDEX16,native.data(),sizeof(SFVFBranchVertex));
            else STATEMANAGER.DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,native.data(),sizeof(SFVFBranchVertex));
        }
        trees.Draw(&screen,geometry,texture,draw.stage1 ? (test>=20 ? cameraMask : second) : TerrainTexturePtr{},draw);
        Check(!trees.Failed() && trees.VisibleInstances()==1 && trees.DrawCount(draw.part)==1,"tree deterministic draw");
        STATEMANAGER.SetVertexShader(nullptr);
        legacy.EndFrame(); modern.EndFrame();
        const auto d9=LegacyProbe::Read(width,height),d11=BackendTestAccess::Read(modern,false);
        const auto depth=BackendTestAccess::Read(modern,true);
        size_t pixels=0,edges=0,interior=0,alphaErrors=0,depthErrors=0; double error=0;
        for(size_t i=0;i<d9.size();++i) {
            const bool a=(d9[i]&0xffffff)!=0,b=(d11[i]&0xffffff)!=0;
            if(a!=b) {
                ++edges; bool edgeA=false,edgeB=false;
                for(int offset:{-1,1,-int(width),int(width)}) { const auto j=int64_t(i)+offset; if(j<0 || j>=int64_t(d9.size())) continue;
                    edgeA|=a!=((d9[j]&0xffffff)!=0); edgeB|=b!=((d11[j]&0xffffff)!=0); }
                if(!edgeA || !edgeB) ++interior;
            }
            if(a && b) { ++pixels; for(int c=0;c<3;++c) error+=std::abs(int((d9[i]>>(16-c*8))&255)-int((d11[i]>>(c*8))&255));
                if(std::abs(int(d9[i]>>24)-int(d11[i]>>24))>2) ++alphaErrors; }
            if(b && ((depth[i]&0xffffff)!=0xffffff)!=draw.depthWrite) ++depthErrors;
        }
        const double mean=pixels ? error/(pixels*3) : 0;
        std::cout<<"Tree native case="<<test<<" pixels="<<pixels<<" RGB="<<mean<<" edges="<<edges<<" interior="<<interior
                 <<" alpha-errors="<<alphaErrors<<" depth-errors="<<depthErrors<<'\n';
        SaveSplatReadback(d9,width,height,false,"tree-case"+std::to_string(test)+"-d3d9");
        SaveSplatReadback(d11,width,height,true,"tree-case"+std::to_string(test)+"-d3d11");
        Check((test==3 || test==20 ? pixels==0 : pixels>1000) && mean<1.5 && !interior && edges<width+height && !alphaErrors && !depthErrors,"native tree colors/alpha/leaf/fog/depth parity");
        geometry.reset(); Check(trees.LiveGeometryCount()==0,"tree geometry cycle released");
        legacy.Present(); modern.Present();
    }
    Check(modern.Resize(0,0) && modern.Resize(640,480),"tree suspend restore");
    trees.ResetFrame(); texture.reset(); second.reset(); STATEMANAGER.SetTexture(0,nullptr); STATEMANAGER.SetTexture(1,nullptr);
    Check(trees.LiveTextureCount()==0 && trees.LiveGeometryCount()==0,"tree geometry/texture/SRB lifetime");
    std::cout<<"Native SpeedTree GPU parity / 24 state cases / repeated lifetime: PASS\n";

    // Shared terrain/tree depth, both orders, plus alternating PSOs without a global state reset.
    DiligentTerrainRenderer ground(modern); Check(ground.Initialize(),"tree depth terrain pipeline");
    const std::array<float,16> identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    TreeDraw draw; draw.matrices={identity,identity,identity}; draw.cull=0; draw.count=4; draw.alphaTest=false;
    TreeSource data; data.vertices.resize(4); data.indices={0,1,2,3};
    const float xy[4][2]={{-.8f,-.8f},{-.8f,.8f},{.8f,-.8f},{.8f,.8f}};
    for(unsigned i=0;i<4;++i) { data.vertices[i].position={xy[i][0],xy[i][1],.2f}; data.vertices[i].color=0xffff0000; }
    auto mesh=trees.UploadGeometry(data);
    auto whiteBytes=TerrainFixture::GradientDDS(); std::fill(whiteBytes.begin()+128,whiteBytes.end(),255);
    auto white=LoadStaticObjectTextureMemory(whiteBytes.data(),whiteBytes.size(),trees);
    float groundVertices[289][6]{};
    for(unsigned i=0;i<4;++i) { groundVertices[i][0]=xy[i][0]; groundVertices[i][1]=xy[i][1]; groundVertices[i][2]=.7f; groundVertices[i][5]=1; }
    auto groundMesh=ground.UploadVertices(groundVertices,289,sizeof(groundVertices[0]));
    const uint16_t groundIndices[]={0,2,1,2,3,1}; auto groundIndex=ground.UploadIndices(groundIndices,6);
    Check(mesh && white && groundMesh && groundIndex,"shared tree/terrain test data");
    Check(modern.Resize(160,120),"tree shared depth target");
    for(unsigned mode=0;mode<12;++mode) {
        trees.ResetFrame(); Check(modern.BeginFrame(),"tree shared depth begin"); modern.Clear({true,ClearColor{0,0,0,1}});
        draw.depthWrite=mode%3!=1; draw.depthTest=mode%3!=2;
        // In depth-test-off cases place the tree behind the opaque terrain; it must still draw last.
        draw.matrices.world[14]=mode%3==2 ? .7f : 0;
        draw.blend=mode>=6; draw.part=mode%2 ? TreePart::Branch : TreePart::Frond;
        const bool treeFirst=mode%3==1 || (mode%3==0 && mode%2);
        const auto drawGround=[&] { ground.BeginTerrain({identity,identity,identity},true);
            ground.DrawTerrainSolid(groundMesh,groundIndex,6,false,{0,1,0,1}); };
        const auto drawTree=[&] { trees.Draw(&screen,mesh,white,{},draw); };
        if(treeFirst) { drawTree(); drawGround(); } else { drawGround(); drawTree(); }
        modern.EndFrame(); const auto pixels=BackendTestAccess::Read(modern,false);
        const auto center=pixels[60*160+80]&0xffffff;
        std::cout<<"Tree shared depth mode="<<mode<<" center="<<std::hex<<center<<std::dec<<'\n';
        Check(center==(mode%3==1 ? 0x00ff00u : 0x0000ffu),"tree/terrain shared depth and order");
        Check(!trees.Failed() && !ground.Failed(),"tree/terrain alternating states"); modern.Present();
    }
    trees.ResetFrame(); mesh.reset(); white.reset(); groundMesh.reset(); groundIndex.reset();
    Check(trees.LiveGeometryCount()==0 && trees.LiveTextureCount()==0,"tree depth resources released");
    std::cout<<"Tree/terrain shared depth / 12 draw-order and depth-write/test cases / resources zero: PASS\n";
}
