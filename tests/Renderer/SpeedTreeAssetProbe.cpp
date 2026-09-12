// ZiiNAN: Diligent SpeedTree rendering integration; read-only native SDK asset evidence.
#include <cstddef>
#include <SpeedTreeRT.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

int main(int argc,char** argv)
{
    if(argc<2) return 2;
    CSpeedTreeRT::SetNumWindMatrices(4);
    CSpeedTreeRT::SetTextureFlip(true);
    const float light[]={-.707f,-.300f,.707f,1,1,1,.5f,.5f,.5f,1,1,1,0,1,0,0};
    CSpeedTreeRT::SetLightAttributes(0,light); CSpeedTreeRT::SetLightState(0,true);
    const float eye[]={0,-2000,600}, direction[]={0,1,-.2f};
    CSpeedTreeRT::SetCamera(eye,direction);
    CSpeedTreeRT::SetDropToBillboard(true);
    for(int file=1;file<argc;++file) {
        CSpeedTreeRT tree;
        tree.SetWindStrength(1); tree.SetLocalMatrices(0,4);
        if(!tree.LoadTree(argv[file])) { std::fprintf(stderr,"load failed: %s\n",argv[file]); return 1; }
        tree.SetBranchLightingMethod(CSpeedTreeRT::LIGHT_STATIC);
        tree.SetLeafLightingMethod(CSpeedTreeRT::LIGHT_STATIC);
        tree.SetFrondLightingMethod(CSpeedTreeRT::LIGHT_STATIC);
        tree.SetBranchWindMethod(CSpeedTreeRT::WIND_NONE);
        tree.SetLeafWindMethod(CSpeedTreeRT::WIND_NONE);
        tree.SetFrondWindMethod(CSpeedTreeRT::WIND_NONE);
        tree.SetNumLeafRockingGroups(1);
        if(!tree.Compute(nullptr,1,false)) { std::fprintf(stderr,"compute failed\n"); return 1; }
        tree.SetLeafRockingState(true);
        float box[6]; tree.GetBoundingBox(box); tree.SetLodLimits((box[5]-box[2])*2,(box[5]-box[2])*9);
        CSpeedTreeRT::STextures textures; tree.GetTextures(textures);
        CSpeedTreeRT::SGeometry geometry;
        std::printf("asset=%s\ntextures branch=%s composite=%s self=%s\n",argv[file],
            textures.m_pBranchTextureFilename ? textures.m_pBranchTextureFilename : "none",
            textures.m_pCompositeFilename ? textures.m_pCompositeFilename : "none",
            textures.m_pSelfShadowFilename ? textures.m_pSelfShadowFilename : "none");
        std::printf("lod_counts=%u,%u,%u height=%g\n",tree.GetNumBranchLodLevels(),tree.GetNumFrondLodLevels(),tree.GetNumLeafLodLevels(),box[5]-box[2]);
        for(float lod:{1.0f,.75f,.5f,.25f,0.0f}) {
            tree.SetLodLevel(lod); tree.GetGeometry(geometry);
            const auto& b=geometry.m_sBranches; const auto& f=geometry.m_sFronds;
            std::printf("lod=%g branch=%u/%u/%d frond=%u/%u/%d leaf0=%u/%d/%d/%g leaf1=%u/%d/%d/%g boards=%d,%d alpha=%g,%g\n",
                lod,b.m_usVertexCount,b.m_usNumStrips,b.m_nDiscreteLodLevel,f.m_usVertexCount,f.m_usNumStrips,f.m_nDiscreteLodLevel,
                geometry.m_sLeaves0.m_usLeafCount,geometry.m_sLeaves0.m_nDiscreteLodLevel,geometry.m_sLeaves0.m_bIsActive,geometry.m_sLeaves0.m_fAlphaTestValue,
                geometry.m_sLeaves1.m_usLeafCount,geometry.m_sLeaves1.m_nDiscreteLodLevel,geometry.m_sLeaves1.m_bIsActive,geometry.m_sLeaves1.m_fAlphaTestValue,
                geometry.m_sBillboard0.m_bIsActive,geometry.m_sBillboard1.m_bIsActive,geometry.m_fBranchAlphaTestValue,geometry.m_fFrondAlphaTestValue);
        }
        tree.SetLodLevel(1); CSpeedTreeRT::SetTime(0); tree.GetGeometry(geometry);
        unsigned count=0; const auto* table=tree.GetLeafBillboardTable(count);
        std::vector<float> before;
        if(table && count) before.assign(table,table+count);
        CSpeedTreeRT::SetTime(1.25f); tree.GetGeometry(geometry); table=tree.GetLeafBillboardTable(count);
        float delta=0;
        if(before.size()!=count) return 1;
        for(unsigned i=0;i<count;++i) { if(!std::isfinite(table[i])) return 1; delta=std::max(delta,std::abs(table[i]-before[i])); }
        std::printf("leaf_table_floats=%u time_delta=%g wind_methods=%d,%d,%d rocking=%d\n",count,delta,
            tree.GetBranchWindMethod(),tree.GetFrondWindMethod(),tree.GetLeafWindMethod(),tree.GetLeafRockingState());
    }
    return 0;
}
