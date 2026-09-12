#pragma once
// ZiiNAN: Additional actor stages captured from real D3D9 state and compared by pixel readback.
static Renderer::StaticObjectDraw ActorMaterialCase(int pose, const Renderer::StaticObjectDraw& base,
    IDirect3DTexture9* sphere, const Renderer::TerrainTexturePtr& modernSphere)
{
    using namespace Renderer;
    const int mode=pose-27;
    const bool specular=(mode>=1 && mode<=3) || mode==8 || mode==9;
    const bool fade=mode==0 || mode==6 || mode==7;
    STATEMANAGER.SetRenderState(D3DRS_TEXTUREFACTOR,D3DXCOLOR(0.2f,0.4f,0.6f,mode==1 ? 0.2f : (mode==3 ? 1.0f : 0.6f)));
    STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAARG2,(specular || fade) ? D3DTA_TFACTOR : D3DTA_DIFFUSE);
    STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAOP,fade ? D3DTOP_SELECTARG2 : D3DTOP_MODULATE);
    // ZiiNAN: Ensure deterministic actor material state. SELECTARG2 ignores ARG1.
    if(mode==0 || mode==6) STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAARG1,mode==0 ? D3DTA_CURRENT : D3DTA_DIFFUSE);
    STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE,fade);
    STATEMANAGER.SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    STATEMANAGER.SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    STATEMANAGER.SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
    STATEMANAGER.SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
    STATEMANAGER.SetRenderState(D3DRS_ZWRITEENABLE,mode!=7);
    STATEMANAGER.SetRenderState(D3DRS_ALPHATESTENABLE,mode==6 || mode==9);
    STATEMANAGER.SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER);
    STATEMANAGER.SetRenderState(D3DRS_ALPHAREF,0);
    STATEMANAGER.SetRenderState(D3DRS_NORMALIZENORMALS,mode==8);
    if(specular) {
        STATEMANAGER.SetTexture(1,sphere);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_MODULATEALPHA_ADDCOLOR);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLORARG1,D3DTA_CURRENT);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLORARG2,D3DTA_TEXTURE);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_CURRENT);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_TEXCOORDINDEX,D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_COUNT2);
        D3DXMATRIX matrix; D3DXMatrixTranslation(&matrix,0.17f*mode,0.11f*mode,0);
        STATEMANAGER.SetTransform(D3DTS_TEXTURE1,&matrix);
        for(auto sampler:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV}) STATEMANAGER.SetSamplerState(1,sampler,D3DTADDRESS_WRAP);
        for(auto sampler:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER}) STATEMANAGER.SetSamplerState(1,sampler,D3DTEXF_LINEAR);
        STATEMANAGER.SetSamplerState(1,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
    } else if(mode==4 || mode==5) {
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,mode==4 ? D3DTOP_ADD : D3DTOP_MODULATE);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLORARG1,D3DTA_CURRENT);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLORARG2,D3DTA_TFACTOR);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
    }
    StaticObjectDraw captured;
    Check(CaptureStaticMapObjectDraw(captured,false,false,true),"5B native actor material capture");
    if(mode==0) {
        StaticObjectDraw reused;
        reused.actorStage=ActorMaterialStage::Specular; reused.alphaTest=StaticObjectAlphaTest::Greater;
        reused.alphaReference=255; reused.fog=TerrainFog::Exp; reused.rangeFog=true;
        reused.anisotropic=true; reused.maxAnisotropy=16; reused.cameraAlpha=modernSphere;
        reused.sphereMap=modernSphere; reused.pointDiffuse={1,1,1,1}; reused.pointPositionRange={1,1,1,100};
        Check(CaptureStaticMapObjectDraw(reused,false,false,true),"reused actor snapshot captures independently");
        Check(reused.actorStage==captured.actorStage && reused.alphaTest==captured.alphaTest &&
              reused.alphaReference==captured.alphaReference && reused.fog==captured.fog && reused.rangeFog==captured.rangeFog &&
              reused.anisotropic==captured.anisotropic && reused.maxAnisotropy==captured.maxAnisotropy &&
              reused.pointDiffuse==captured.pointDiffuse && reused.pointPositionRange==captured.pointPositionRange &&
              !reused.cameraAlpha && !reused.sphereMap,"inactive actor fields never inherited from a prior snapshot");
        for(DWORD ignored:{DWORD(D3DTA_TEXTURE),DWORD(D3DTA_DIFFUSE),DWORD(D3DTA_CURRENT),DWORD(D3DTA_TFACTOR)}) {
            STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAARG1,ignored);
            Check(CaptureStaticMapObjectDraw(reused,false,false,true) && reused.factorAlphaOnly &&
                  reused.textureFactor==captured.textureFactor,"SELECTARG2 independent of ignored alpha argument 1");
        }
        STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_CURRENT);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
        Check(!CaptureStaticMapObjectDraw(reused,false,false,true),"unsupported ACTIVE alpha argument still rejected");
        STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);
        std::cout<<"Actor capture: inactive ARG1 variants / fresh snapshot / active-state rejection: PASS\n";
    }
    captured.matrices=base.matrices; captured.normalTransform=base.normalTransform;
    captured.firstIndex=base.firstIndex; captured.indexCount=base.indexCount;
    captured.baseVertex=base.baseVertex; captured.vertexCount=base.vertexCount;
    if(specular) captured.sphereMap=modernSphere;
    Check(captured.factorAlphaOnly==fade && captured.factorAlpha==specular,"5B actual factor alpha mode");
    return captured;
}
