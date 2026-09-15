// ZiiNAN: Offline reference boundary; this executable is never linked to a runtime.
#include <cstddef>
#include <SpeedTreeRT.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>

namespace {
using Writer=rapidjson::Writer<rapidjson::StringBuffer>;
void Require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
void String(Writer& w,const char* key,const char* value) { w.Key(key);w.String(value?value:""); }
template<class T> void Array(Writer& w,const char* key,const T* p,unsigned count) {
    w.Key(key);w.StartArray();if(p)for(unsigned i=0;i<count;++i) { Require(std::isfinite(double(p[i])),"nonfinite reference value");w.Double(double(p[i])); }w.EndArray();
}
void Indexed(Writer& w,const char* key,const CSpeedTreeRT::SGeometry::SIndexed& g) {
    w.Key(key);w.StartObject();w.Key("lod");w.Int(g.m_nDiscreteLodLevel);
    w.Key("vertices");w.Uint(g.m_usVertexCount);w.Key("strips");w.Uint(g.m_usNumStrips);
    unsigned indices=0,triangles=0;
    for(unsigned s=0;s<g.m_usNumStrips;++s) {
        indices+=g.m_pStripLengths[s];
        for(unsigned i=0;i<g.m_pStripLengths[s];++i) {
            Require(g.m_pStrips[s][i]<g.m_usVertexCount,"invalid strip index");
            if(i>=2) { const auto* p=g.m_pStrips[s]+i;triangles+=p[-2]!=p[-1]&&p[-1]!=p[0]&&p[-2]!=p[0]; }
        }
    }
    w.Key("stripIndices");w.Uint(indices);w.Key("triangles");w.Uint(triangles);
    w.Key("normals");w.Bool(g.m_pNormals!=nullptr);w.Key("colors");w.Bool(g.m_pColors!=nullptr);
    w.Key("uv1");w.Bool(g.m_pTexCoords1!=nullptr);
    w.Key("selectedVertices");w.StartArray();if(g.m_usVertexCount)for(unsigned i:{0u,unsigned(g.m_usVertexCount/2),unsigned(g.m_usVertexCount-1)}){w.StartObject();w.Key("index");w.Uint(i);Array(w,"position",g.m_pCoords+i*3,3);Array(w,"uv",g.m_pTexCoords0+i*2,2);w.Key("color");w.Uint(static_cast<unsigned>(g.m_pColors[i]));w.EndObject();}w.EndArray();w.EndObject();
}
void Probe(const char* path,Writer& w) {
    const float light[]={-.707f,-.300f,.707f,1,1,1,.5f,.5f,.5f,1,1,1,0,1,0,0};
    CSpeedTreeRT::SetNumWindMatrices(4);CSpeedTreeRT::SetTextureFlip(true);
    CSpeedTreeRT::SetLightAttributes(0,light);CSpeedTreeRT::SetLightState(0,true);
    CSpeedTreeRT::SetDropToBillboard(true);CSpeedTreeRT::SetTime(0);
    const float eye[]={0,-2000,600},direction[]={0,1,0};CSpeedTreeRT::SetCamera(eye,direction);
    CSpeedTreeRT tree;tree.SetWindStrength(1);tree.SetLocalMatrices(0,4);
    Require(tree.LoadTree(path),"legacy LoadTree failed");
    tree.SetBranchLightingMethod(CSpeedTreeRT::LIGHT_STATIC);tree.SetFrondLightingMethod(CSpeedTreeRT::LIGHT_STATIC);tree.SetLeafLightingMethod(CSpeedTreeRT::LIGHT_STATIC);
    tree.SetBranchWindMethod(CSpeedTreeRT::WIND_NONE);tree.SetFrondWindMethod(CSpeedTreeRT::WIND_NONE);tree.SetLeafWindMethod(CSpeedTreeRT::WIND_NONE);
    tree.SetNumLeafRockingGroups(1);Require(tree.Compute(nullptr,1,false),"legacy Compute failed");tree.SetLeafRockingState(true);
    float box[6];tree.GetBoundingBox(box);tree.SetLodLimits((box[5]-box[2])*2,(box[5]-box[2])*9);
    CSpeedTreeRT::STextures textures;tree.GetTextures(textures);CSpeedTreeRT::SGeometry g;
    Array(w,"bounds",box,6);w.Key("sourceBytes");w.Uint64(std::filesystem::file_size(path));
    w.Key("textures");w.StartObject();String(w,"branch",textures.m_pBranchTextureFilename);String(w,"composite",textures.m_pCompositeFilename);String(w,"selfShadow",textures.m_pSelfShadowFilename);w.EndObject();
    w.Key("branchLODs");w.StartArray();for(unsigned lod=0;lod<tree.GetNumBranchLodLevels();++lod){tree.GetGeometry(g,SpeedTree_BranchGeometry,short(lod));w.StartObject();Indexed(w,"geometry",g.m_sBranches);w.EndObject();}w.EndArray();
    w.Key("frondLODs");w.StartArray();for(unsigned lod=0;lod<tree.GetNumFrondLodLevels();++lod){tree.GetGeometry(g,SpeedTree_FrondGeometry,-1,short(lod));w.StartObject();Indexed(w,"geometry",g.m_sFronds);w.EndObject();}w.EndArray();
    w.Key("leafLODs");w.StartArray();tree.SetLodLevel(1);
    for(unsigned lod=0;lod<tree.GetNumLeafLodLevels();++lod){tree.GetGeometry(g,SpeedTree_LeafGeometry,-1,-1,short(lod));w.StartObject();w.Key("count");w.Uint(g.m_sLeaves0.m_usLeafCount);w.Key("size");w.Double(tree.GetLeafLodSizeAdjustments()[lod]);w.Key("selectedLeaves");w.StartArray();const auto&leaf=g.m_sLeaves0;if(leaf.m_usLeafCount)for(unsigned i:{0u,unsigned(leaf.m_usLeafCount-1)}){w.StartObject();w.Key("index");w.Uint(i);Array(w,"center",leaf.m_pCenterCoords+i*3,3);Array(w,"offsets",leaf.m_pLeafMapCoords[i],16);Array(w,"uv",leaf.m_pLeafMapTexCoords[i],8);w.Key("color");w.Uint(static_cast<unsigned>(leaf.m_pColors[i]));w.EndObject();}w.EndArray();w.EndObject();}w.EndArray();
    w.Key("samples");w.StartArray();for(float lod:{1.f,.75f,.5f,.25f,.1f,0.f}) {
        tree.SetLodLevel(lod);tree.GetGeometry(g);w.StartObject();w.Key("lod");w.Double(lod);
        w.Key("branch");w.Int(g.m_sBranches.m_nDiscreteLodLevel);w.Key("frond");w.Int(g.m_sFronds.m_nDiscreteLodLevel);
        w.Key("leaf");w.Int(g.m_sLeaves0.m_nDiscreteLodLevel);w.Key("leafActive");w.Bool(g.m_sLeaves0.m_bIsActive);
        w.Key("billboard0");w.Bool(g.m_sBillboard0.m_bIsActive);w.Key("billboard1");w.Bool(g.m_sBillboard1.m_bIsActive);
        w.Key("horizontal");w.Bool(g.m_sHorizontalBillboard.m_bIsActive);
        Array(w,"billboardCoords",g.m_sBillboard0.m_pCoords,12);Array(w,"billboardUV",g.m_sBillboard0.m_pTexCoords,8);w.EndObject();
    }w.EndArray();
    w.Key("collisionCount");w.Uint(tree.GetCollisionObjectCount());
    tree.SetLodLevel(1);tree.GetGeometry(g);unsigned count=0;const float* table=tree.GetLeafBillboardTable(count);
    Array(w,"leafTable",table,count);CSpeedTreeRT::SetTime(1.25f);tree.GetGeometry(g);table=tree.GetLeafBillboardTable(count);Array(w,"leafTableLater",table,count);
    CSpeedTreeRT::SetTime(0);w.Key("cameraLeafSamples");w.StartArray();for(float angle:{-1.570796327f,-.785398163f,0.f,.463647609f,.785398163f,1.570796327f}){const float dir[]={0,std::cos(angle),-std::sin(angle)};CSpeedTreeRT::SetCamera(eye,dir);tree.GetGeometry(g);table=tree.GetLeafBillboardTable(count);w.StartObject();w.Key("angle");w.Double(angle);Array(w,"table",table,count);w.EndObject();}w.EndArray();
}
}
int main(int argc,char**argv) {
    if(argc!=2){std::cerr<<"usage: ZiiNANSPTProbe tree.spt\n";return 2;}
    rapidjson::StringBuffer out;Writer w(out);w.StartObject();String(w,"path",argv[1]);
    try { Probe(argv[1],w);String(w,"status","ok");w.EndObject();std::cout<<out.GetString()<<'\n';return 0; }
    catch(const std::exception& e){ std::cerr<<argv[1]<<": "<<e.what()<<'\n';return 1; }
}
