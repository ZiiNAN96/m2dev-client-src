#include "VegetationSource.h"
#include <cstddef>
#include <SpeedTreeRT.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>

namespace ZiiNAN::VegetationTool {
namespace {
using namespace AssetTool;using Vegetation::PartKind;
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
Vec3 Position(const float* p){return {p[0]*.01f,p[2]*.01f,-p[1]*.01f};}
Vec3 Normal(const float* p){Vec3 n{p[0],p[2],-p[1]};double length=0;for(float f:n)length+=double(f)*f;if(!(std::isfinite(length)&&length>1e-15))return {};for(float&f:n)f=float(f/std::sqrt(length));return n;}
Vec4 Color(unsigned long c){return {float((c>>16)&255)/255,float((c>>8)&255)/255,float(c&255)/255,float((c>>24)&255)/255};}
void Expand(Vegetation::Bounds& b,const Vegetation::Vec3&p){for(unsigned k=0;k<3;++k){b.min[k]=std::min(b.min[k],p[k]);b.max[k]=std::max(b.max[k],p[k]);}}
void Setup(CSpeedTreeRT&tree,const std::filesystem::path&path,bool dynamic){
    tree.SetWindStrength(1);tree.SetLocalMatrices(0,4);Require(tree.LoadTree(path.string().c_str()),"SPT load failed");
    auto lighting=dynamic?CSpeedTreeRT::LIGHT_DYNAMIC:CSpeedTreeRT::LIGHT_STATIC;
    tree.SetBranchLightingMethod(lighting);tree.SetFrondLightingMethod(lighting);tree.SetLeafLightingMethod(lighting);
    tree.SetBranchWindMethod(CSpeedTreeRT::WIND_NONE);tree.SetFrondWindMethod(CSpeedTreeRT::WIND_NONE);tree.SetLeafWindMethod(CSpeedTreeRT::WIND_NONE);
    tree.SetNumLeafRockingGroups(1);Require(tree.Compute(nullptr,1,false),"SPT compute failed");tree.SetLeafRockingState(true);tree.SetLodLevel(1);
}
Mesh Indexed(const CSpeedTreeRT::SGeometry::SIndexed&g,unsigned material){
    Mesh mesh;mesh.material=material;mesh.hasNormals=mesh.hasUV=mesh.hasVertexExtras=true;
    if(!g.m_usVertexCount)return mesh;
    Require(g.m_pCoords&&g.m_pColors&&g.m_pTexCoords0,"missing static branch/frond channels");
    mesh.vertices.resize(g.m_usVertexCount);
    for(unsigned i=0;i<g.m_usVertexCount;++i){auto&v=mesh.vertices[i];v.position=Position(g.m_pCoords+i*3);v.normal={0,1,0};v.color=Color(g.m_pColors[i]);std::copy_n(g.m_pTexCoords0+i*2,2,v.uv.begin());if(g.m_pTexCoords1)std::copy_n(g.m_pTexCoords1+i*2,2,v.uv1.begin());v.flexibility=g.m_pWindWeights?g.m_pWindWeights[i]:0;}
    for(unsigned strip=0;strip<g.m_usNumStrips;++strip){const auto*p=g.m_pStrips[strip];for(unsigned i=2;i<g.m_pStripLengths[strip];++i){unsigned a=p[i-2],b=p[i-1],c=p[i];Require(a<mesh.vertices.size()&&b<mesh.vertices.size()&&c<mesh.vertices.size(),"invalid source index");if(a==b||a==c||b==c)continue;if(i&1)std::swap(a,b);mesh.indices.insert(mesh.indices.end(),{a,b,c});}}
    return mesh;
}
}
bool Extract(const std::filesystem::path&path,std::string_view legacy,VegetationSource&result,std::string&error){
    try{
        Require(std::filesystem::file_size(path)>0&&std::filesystem::file_size(path)<=16*1024*1024,"invalid SPT source size");
        const float light[]={-.707f,-.300f,.707f,1,1,1,.5f,.5f,.5f,1,1,1,0,1,0,0};
        CSpeedTreeRT::SetNumWindMatrices(4);CSpeedTreeRT::SetTextureFlip(true);CSpeedTreeRT::SetLightAttributes(0,light);CSpeedTreeRT::SetLightState(0,true);CSpeedTreeRT::SetDropToBillboard(true);CSpeedTreeRT::SetTime(0);
        const float eye[]={0,-2000,600},direction[]={0,1,0};CSpeedTreeRT::SetCamera(eye,direction);
        CSpeedTreeRT tree;Setup(tree,path,false);
        VegetationSource source;source.legacyKey=Vegetation::NormalizeKey(legacy);auto&scene=source.scene;auto&m=source.metadata;
        scene.name=source.legacyKey;scene.sourceFormat="legacy-spt";scene.unitPolicy="Native Z-up cm -> glTF Y-up meters; runtime reverses once";scene.sourceMetersPerUnit=.01;scene.nodes.push_back({"vegetation"});
        const auto pos=source.legacyKey.find("ymir work/");Require(pos!=std::string::npos,"expected legacy ymir work asset key");
        auto compiled="vegetation/"+source.legacyKey.substr(pos);compiled.resize(compiled.size()-4);m.geometry=compiled+".glb";
        float box[6];tree.GetBoundingBox(box);std::copy_n(box,3,m.bounds.min.begin());std::copy_n(box+3,3,m.bounds.max.begin());m.bounds.valid=true;Require(Vegetation::ValidBounds(m.bounds),"invalid reference bounds");m.renderBounds=m.bounds;
        const float height=box[5]-box[2];Require(height>0,"zero tree height");m.nearDistance=height*2;m.farDistance=height*9;m.cullDistance=m.farDistance*2;tree.SetLodLimits(m.nearDistance,m.farDistance);
        CSpeedTreeRT::STextures textures;tree.GetTextures(textures);
        auto texture=[&](const char*name){if(!name||!*name)return std::string{};return Vegetation::NormalizeKey((std::filesystem::path(source.legacyKey).parent_path()/std::filesystem::path(name).replace_extension(".dds")).generic_string());};
        const auto branchTexture=texture(textures.m_pBranchTextureFilename),compositeTexture=texture(textures.m_pCompositeFilename);m.shadowTexture=texture(textures.m_pSelfShadowFilename);
        for(const auto&key:{branchTexture,compositeTexture}){Image image;image.name=key;image.packPath=key;scene.images.push_back(std::move(image));}
        for(unsigned kind=0;kind<4;++kind){Material material;material.name=std::array<const char*,4>{"branch","frond","leaf","billboard"}[kind];material.baseTexture=kind?1:0;material.alpha=AlphaMode::Mask;material.alphaCutoff=84.f/255;material.doubleSided=kind!=0;scene.materials.push_back(material);}
        source.lodCounts={tree.GetNumBranchLodLevels(),tree.GetNumFrondLodLevels(),tree.GetNumLeafLodLevels()};
        CSpeedTreeRT::SGeometry g;std::map<std::pair<unsigned,unsigned>,int> lookup;
        auto add=[&](Mesh mesh,PartKind kind,unsigned lod){if(mesh.indices.empty()){lookup[{unsigned(kind),lod}]=-1;return;}mesh.name=scene.materials[unsigned(kind)].name+"/lod"+std::to_string(lod);const auto index=unsigned(scene.meshes.size());m.parts.push_back({kind,lod,index});scene.nodes[0].meshes.push_back(index);lookup[{unsigned(kind),lod}]=int(index);
            for(const auto&v:mesh.vertices){Vegetation::Vec3 p{v.position[0]*100,-v.position[2]*100,v.position[1]*100};Expand(m.renderBounds,p);}scene.meshes.push_back(std::move(mesh));};
        for(unsigned kind=0;kind<2;++kind)for(unsigned lod=0;lod<source.lodCounts[kind];++lod){const auto mask=kind?SpeedTree_FrondGeometry:SpeedTree_BranchGeometry;tree.GetGeometry(g,mask,kind?-1:short(lod),kind?short(lod):-1);add(Indexed(kind?g.m_sFronds:g.m_sBranches,kind),PartKind(kind),lod);}
        CSpeedTreeRT::SetTime(0);tree.SetLodLevel(1);
        for(unsigned lod=0;lod<source.lodCounts[2];++lod){
            tree.GetGeometry(g,SpeedTree_LeafGeometry,-1,-1,short(lod));const auto&leaves=g.m_sLeaves0;
            Mesh mesh;mesh.material=2;mesh.hasNormals=mesh.hasUV=mesh.hasVertexExtras=true;
            for(unsigned i=0;i<leaves.m_usLeafCount;++i){Require(leaves.m_pLeafMapCoords&&leaves.m_pColors,"missing leaf channels");const auto center=Position(leaves.m_pCenterCoords+i*3);
                // SDK leaf-map coordinates already include the discrete LOD size adjustment.
                for(unsigned c=0;c<4;++c){Vertex v;v.pivot=center;const auto offset=Position(leaves.m_pLeafMapCoords[i]+c*4);for(unsigned k=0;k<3;++k)v.position[k]=center[k]+offset[k];v.normal={0,1,0};v.color=Color(leaves.m_pColors[i]);std::copy_n(leaves.m_pLeafMapTexCoords[i]+c*2,2,v.uv.begin());v.flexibility=1;mesh.vertices.push_back(v);}
                const auto base=i*4;mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2,base,base+2,base+3});
            }add(std::move(mesh),PartKind::Leaf,lod);
        }
        // Own the measured camera response. Reject corpus features that cannot fit this neutral model.
        for(const auto& part:m.parts)if(part.kind==PartKind::Leaf){auto& mesh=scene.meshes[part.mesh];std::array<std::vector<Vec3>,2> samples;
            for(unsigned sample=0;sample<2;++sample){const float angle=sample?.785398163f:-.785398163f;const float dir[]={0,std::cos(angle),-std::sin(angle)};CSpeedTreeRT::SetCamera(eye,dir);tree.GetGeometry(g);tree.GetGeometry(g,SpeedTree_LeafGeometry,-1,-1,short(part.lod));const auto& leaf=g.m_sLeaves0;
                Require(mesh.vertices.size()==std::size_t(leaf.m_usLeafCount)*4,"camera leaf count changed");unsigned tableCount=0;const float*table=tree.GetLeafBillboardTable(tableCount);const float scale=tree.GetLeafLodSizeAdjustments()[part.lod];for(std::size_t i=0;i<mesh.vertices.size();++i){const unsigned offset=leaf.m_pLeafClusterIndices[i/4]*16+unsigned(i%4)*4;Require(offset+4<=tableCount,"invalid leaf cluster");const float*p=table+offset;samples[sample].push_back({p[0]*scale,p[1]*scale,p[2]*scale});}}
            for(std::size_t leaf=0;leaf<mesh.vertices.size();leaf+=4){std::size_t selected=leaf;for(unsigned c=1;c<4;++c)if(std::abs(mesh.vertices[leaf+c].position[1]-mesh.vertices[leaf+c].pivot[1])>std::abs(mesh.vertices[selected].position[1]-mesh.vertices[selected].pivot[1]))selected=leaf+c;
                const auto&v=mesh.vertices[selected];const Vec3 base{(v.position[0]-v.pivot[0])*100,-(v.position[2]-v.pivot[2])*100,(v.position[1]-v.pivot[1])*100};Require(std::abs(base[2])>.001f,"degenerate leaf camera basis");
                Vec3 cosine{},sine{};for(unsigned k=0;k<3;++k){cosine[k]=(samples[0][selected][k]+samples[1][selected][k]-2*base[k])/(2*(std::cos(.785398163f)-1)*base[2]);sine[k]=(samples[1][selected][k]-samples[0][selected][k])/(2*std::sin(.785398163f)*base[2]);}
                for(unsigned c=0;c<4;++c){mesh.vertices[leaf+c].cardPitchCos=cosine;mesh.vertices[leaf+c].cardPitchSin=sine;}
            }
            for(float angle:{-.9f,-.463647609f,.463647609f,.9f}){const float dir[]={0,std::cos(angle),-std::sin(angle)};CSpeedTreeRT::SetCamera(eye,dir);tree.GetGeometry(g);tree.GetGeometry(g,SpeedTree_LeafGeometry,-1,-1,short(part.lod));
                for(std::size_t i=0;i<mesh.vertices.size();++i){const auto&v=mesh.vertices[i];const Vec3 base{(v.position[0]-v.pivot[0])*100,-(v.position[2]-v.pivot[2])*100,(v.position[1]-v.pivot[1])*100};unsigned tableCount=0;const float*table=tree.GetLeafBillboardTable(tableCount);const float*p=table+g.m_sLeaves0.m_pLeafClusterIndices[i/4]*16+(i%4)*4;const float scale=tree.GetLeafLodSizeAdjustments()[part.lod];
                    for(unsigned k=0;k<3;++k){const float predicted=base[k]+base[2]*((std::cos(angle)-1)*v.cardPitchCos[k]+std::sin(angle)*v.cardPitchSin[k]);if(!(std::abs(predicted-p[k]*scale)<.02f+.0002f*std::abs(p[k]*scale)))throw std::runtime_error("unrepresentable leaf camera response lod="+std::to_string(part.lod)+" vertex="+std::to_string(i)+" k="+std::to_string(k)+" angle="+std::to_string(angle)+" base="+std::to_string(base[k])+" z="+std::to_string(base[2])+" predicted="+std::to_string(predicted)+" expected="+std::to_string(p[k]*scale)+" cos="+std::to_string(v.cardPitchCos[k])+" sin="+std::to_string(v.cardPitchSin[k]));}}
            }
        }
        CSpeedTreeRT::SetCamera(eye,direction);
        tree.SetLodLevel(0);tree.GetGeometry(g,SpeedTree_BillboardGeometry);Require(g.m_sBillboard0.m_bIsActive&&g.m_sBillboard0.m_pCoords&&g.m_sBillboard0.m_pTexCoords,"missing required far billboard");
        Require(!g.m_sBillboard1.m_bIsActive&&!g.m_sHorizontalBillboard.m_bIsActive,"multi-plane billboard needs an explicit extractor");
        std::array<float,8> billboardUV;std::copy_n(g.m_sBillboard0.m_pTexCoords,8,billboardUV.begin());
        Mesh billboard;billboard.material=3;billboard.hasNormals=billboard.hasUV=billboard.hasVertexExtras=true;
        for(unsigned c=0;c<4;++c){Vertex v;v.position=Position(g.m_sBillboard0.m_pCoords+c*3);v.normal={0,0,1};std::copy_n(g.m_sBillboard0.m_pTexCoords+c*2,2,v.uv.begin());billboard.vertices.push_back(v);}billboard.indices={0,1,2,0,2,3};add(std::move(billboard),PartKind::Billboard,0);
        // Verify the actual corpus uses a single atlas frame, across azimuth and elevation.
        for(unsigned angle=0;angle<16;++angle)for(float z:{-.5f,0.f,.5f}){const float a=angle*6.28318530718f/16;const float dir[]={std::sin(a),std::cos(a),z};CSpeedTreeRT::SetCamera(eye,dir);tree.GetGeometry(g,SpeedTree_BillboardGeometry);Require(!g.m_sBillboard1.m_bIsActive&&!g.m_sHorizontalBillboard.m_bIsActive&&std::equal(billboardUV.begin(),billboardUV.end(),g.m_sBillboard0.m_pTexCoords),"direction-dependent billboard atlas unsupported");}
        CSpeedTreeRT::SetCamera(eye,direction);
        for(unsigned sample=0;sample<=256;++sample){tree.SetLodLevel(float(sample)/256);tree.GetGeometry(g);Vegetation::LodState state;
            auto set=[&](unsigned slot,unsigned kind,int lod,bool active,float alpha){if(active&&lod>=0){const auto it=lookup.find({kind,unsigned(lod)});Require(it!=lookup.end(),"LOD references unextracted geometry");state.meshes[slot]=it->second;state.alpha[slot]=std::clamp(alpha,0.f,255.f);}};
            set(0,0,g.m_sBranches.m_nDiscreteLodLevel,g.m_fBranchAlphaTestValue<255,g.m_fBranchAlphaTestValue);set(1,1,g.m_sFronds.m_nDiscreteLodLevel,g.m_fFrondAlphaTestValue<255,g.m_fFrondAlphaTestValue);
            set(2,2,g.m_sLeaves0.m_nDiscreteLodLevel,g.m_sLeaves0.m_bIsActive,g.m_sLeaves0.m_fAlphaTestValue);set(3,2,g.m_sLeaves1.m_nDiscreteLodLevel,g.m_sLeaves1.m_bIsActive,g.m_sLeaves1.m_fAlphaTestValue);set(4,3,0,g.m_sBillboard0.m_bIsActive,g.m_sBillboard0.m_fAlphaTestValue);m.lods.push_back(state);
        }
        for(unsigned i=0;i<tree.GetCollisionObjectCount();++i){Vegetation::Collision c;CSpeedTreeRT::ECollisionObjectType type;tree.GetCollisionObject(i,type,c.position.data(),c.dimensions.data());c.kind=unsigned(type);m.collisions.push_back(c);}
        // The binary shares lighting-mode storage. Finish owning static extraction before querying normals.
        CSpeedTreeRT normals;Setup(normals,path,true);CSpeedTreeRT::SGeometry n;
        for(const auto& part:m.parts){auto& mesh=scene.meshes[part.mesh];if(part.kind==PartKind::Billboard)continue;
            if(part.kind==PartKind::Leaf){normals.GetGeometry(n,SpeedTree_LeafGeometry,-1,-1,short(part.lod));const auto&leaf=n.m_sLeaves0;Require(mesh.vertices.size()==std::size_t(leaf.m_usLeafCount)*4&&leaf.m_pNormals,"leaf normal count mismatch");for(std::size_t v=0;v<mesh.vertices.size();++v){Require(mesh.vertices[v].pivot==Position(leaf.m_pCenterCoords+(v/4)*3),"leaf normal source mismatch");mesh.vertices[v].normal=Normal(leaf.m_pNormals+(v/4)*3);}}
            else{const bool frond=part.kind==PartKind::Frond;normals.GetGeometry(n,frond?SpeedTree_FrondGeometry:SpeedTree_BranchGeometry,frond?-1:short(part.lod),frond?short(part.lod):-1);const auto&g=frond?n.m_sFronds:n.m_sBranches;Require(mesh.vertices.size()==g.m_usVertexCount&&g.m_pNormals,"branch/frond normal count mismatch");for(std::size_t v=0;v<mesh.vertices.size();++v){Require(mesh.vertices[v].position==Position(g.m_pCoords+v*3),"branch/frond normal source mismatch");mesh.vertices[v].normal=Normal(g.m_pNormals+v*3);}}
        }
        for(auto& mesh:scene.meshes){
            std::vector<std::array<double,3>> sums(mesh.vertices.size()),firstFace(mesh.vertices.size());
            for(std::size_t i=0;i<mesh.indices.size();i+=3){const auto&a=mesh.vertices[mesh.indices[i]].position;const auto&b=mesh.vertices[mesh.indices[i+1]].position;const auto&c=mesh.vertices[mesh.indices[i+2]].position;const Vec3 u{b[0]-a[0],b[1]-a[1],b[2]-a[2]},v{c[0]-a[0],c[1]-a[1],c[2]-a[2]};const std::array<double,3> n{double(u[1])*v[2]-double(u[2])*v[1],double(u[2])*v[0]-double(u[0])*v[2],double(u[0])*v[1]-double(u[1])*v[0]};for(unsigned corner=0;corner<3;++corner){auto index=mesh.indices[i+corner];for(unsigned k=0;k<3;++k)sums[index][k]+=n[k];if(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]>1e-24)firstFace[index]=n;}}
            for(std::size_t i=0;i<mesh.vertices.size();++i)if(mesh.vertices[i].normal==Vec3{}){double length=0;for(double f:sums[i])length+=f*f;if(length<=1e-24){sums[i]=firstFace[i];for(double f:sums[i])length+=f*f;}if(length>1e-24){for(unsigned k=0;k<3;++k)mesh.vertices[i].normal[k]=float(sums[i][k]/std::sqrt(length));++source.reconstructedNormals;}else{mesh.vertices[i].normal={0,1,0};++source.unusedNormals;}}
        }
        // Conservative bounds for all camera pitches/yaws and the bounded leaf-rocking envelope.
        for(const auto&p:m.parts)if(p.kind==PartKind::Leaf||p.kind==PartKind::Billboard)for(const auto&v:scene.meshes[p.mesh].vertices){
            const Vec3 offset{(v.position[0]-v.pivot[0])*100,-(v.position[2]-v.pivot[2])*100,(v.position[1]-v.pivot[1])*100};float r=0;
            for(unsigned k=0;k<3;++k){const float maximum=std::abs(offset[k]-offset[2]*v.cardPitchCos[k])+std::abs(offset[2])*std::hypot(v.cardPitchCos[k],v.cardPitchSin[k]);r+=maximum*maximum;}
            r=std::sqrt(r)*(1+2*m.wind.leafAmplitude);const Vegetation::Vec3 center{v.pivot[0]*100,-v.pivot[2]*100,v.pivot[1]*100};for(unsigned k=0;k<3;++k){m.renderBounds.min[k]=std::min(m.renderBounds.min[k],center[k]-r);m.renderBounds.max[k]=std::max(m.renderBounds.max[k],center[k]+r);}
        }
        Report report;Require(Validate(scene,report),report.issues.empty()?"invalid neutral scene":report.issues.back().message.c_str());
        Vegetation::Metadata check;Require(bool(Vegetation::ParseMetadata(Vegetation::SerializeMetadata(m),check)),"invalid extracted metadata");result=std::move(source);return true;
    }catch(const std::exception&e){error=e.what();return false;}
}
}
