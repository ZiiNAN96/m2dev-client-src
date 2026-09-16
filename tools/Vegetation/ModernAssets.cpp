// Original procedural H2 demonstration art. No third-party art or runtime SDK.
#include "AssetTool/Scene.h"
#include "Vegetation/VegetationRuntime.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <random>

namespace Tool=ZiiNAN::AssetTool;
namespace fs=std::filesystem;
using V=Tool::Vec3;
constexpr float pi=std::numbers::pi_v<float>;
V Add(V a,V b){for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
V Mul(V a,float s){for(auto&v:a)v*=s;return a;}
V Cross(V a,V b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
float Dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
V Normal(V a){return Mul(a,1/std::max(.00001f,std::sqrt(Dot(a,a))));}
// Author in native Z-up centimeters; write the established Y-up meter scene.
V Gltf(V a){return {a[0]*.01f,a[2]*.01f,-a[1]*.01f};}
V GltfNormal(V a){return {a[0],a[2],-a[1]};}
struct Pixel {unsigned char r{},g{},b{},a{};};
struct Image {unsigned w{},h{};std::vector<Pixel> pixels;};
void DDS(const fs::path&path,const Image&image){
    std::ofstream out(path,std::ios::binary);auto u32=[&](std::uint32_t n){for(unsigned s=0;s<32;s+=8)out.put(char(n>>s));};
    u32(0x20534444);u32(124);u32(0x100f);u32(image.h);u32(image.w);u32(image.w*4);u32(0);u32(0);
    for(unsigned i=0;i<11;++i)u32(0);u32(32);u32(0x41);u32(0);u32(32);u32(0xff);u32(0xff00);u32(0xff0000);u32(0xff000000);
    u32(0x1000);for(unsigned i=0;i<4;++i)u32(0);out.write(reinterpret_cast<const char*>(image.pixels.data()),image.pixels.size()*4);
    if(!out)throw std::runtime_error("DDS write failed");
}
Image LeafTexture(){
    Image image{64,64,std::vector<Pixel>(4096)};
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){float u=(x+.5f)/64*2-1,v=(y+.5f)/64;
        const float width=std::sin(pi*v)*(.82f+.06f*std::sin(v*70));
        const bool inside=std::abs(u)<width;
        const float vein=std::abs(u)<.025f?1.f:0.f,light=.85f+.12f*std::sin(u*2+v*3);
        image.pixels[y*64+x]={static_cast<unsigned char>((73+vein*14)*light),static_cast<unsigned char>((111+vein*10)*light),static_cast<unsigned char>((37+vein*5)*light),static_cast<unsigned char>(inside?255:0)};
    }return image;
}
Image FoliageTexture(){
    // Small leaves on a spray rather than a single meter-sized leaf per card.
    constexpr unsigned size=256;Image image{size,size,std::vector<Pixel>(size*size)};
    for(unsigned leaf=0;leaf<15;++leaf) {
        const float row=float(leaf/2),side=leaf%2?1.f:-1.f;
        const float cx=.5f+side*(.15f+.06f*std::sin(row*1.3f)),cy=.12f+row*.103f;
        const float angle=side*(.5f+.2f*std::sin(row)),s=std::sin(angle),c=std::cos(angle);
        for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x){
            const float dx=(x+.5f)/size-cx,dy=(y+.5f)/size-cy;
            const float u=(dx*c-dy*s)/.12f,v=(dx*s+dy*c)/.205f+.5f;
            if(v<=0||v>=1||std::abs(u)>std::sin(pi*v)*(.85f+.035f*std::sin(v*65)))continue;
            const float vein=std::abs(u)<.025f?1.f:0.f,light=.78f+.12f*std::sin(leaf*2.7f)+.08f*v;
            image.pixels[y*size+x]={static_cast<unsigned char>((73+vein*14)*light),static_cast<unsigned char>((111+vein*10)*light),static_cast<unsigned char>((37+vein*5)*light),255};
        }
    }return image;
}
Image BarkTexture(){Image image{64,64,std::vector<Pixel>(4096)};
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){float f=.8f+.15f*std::sin(x*1.7f+std::sin(y*.2f))+.07f*std::sin(x*8.f+y*2.3f);image.pixels[y*64+x]={static_cast<unsigned char>(107*f),static_cast<unsigned char>(84*f),static_cast<unsigned char>(57*f),255};}return image;}
void Vertex(Tool::Mesh&m,V p,V n,Tool::Vec2 uv,V pivot,float flex,Tool::Vec4 color={1,1,1,1}){
    Tool::Vertex v;v.position=Gltf(p);v.normal=GltfNormal(n);v.uv=uv;v.pivot=Gltf(pivot);v.flexibility=flex;v.color=color;m.vertices.push_back(v);
}
void Card(Tool::Mesh&m,V center,V right,V up,float width,float height,float tint=1){
    const auto base=static_cast<unsigned>(m.vertices.size());const V n=Normal(Cross(right,up));
    const V corners[]{Add(Add(center,Mul(right,-width*.5f)),Mul(up,-height*.5f)),Add(Add(center,Mul(right,width*.5f)),Mul(up,-height*.5f)),Add(Add(center,Mul(right,width*.5f)),Mul(up,height*.5f)),Add(Add(center,Mul(right,-width*.5f)),Mul(up,height*.5f))};
    const Tool::Vec2 uv[]={{0,1},{1,1},{1,0},{0,0}};
    for(unsigned i=0;i<4;++i)Vertex(m,corners[i],n,uv[i],center,1,{tint,tint,tint,1});
    for(unsigned i:{0u,1u,2u,0u,2u,3u})m.indices.push_back(base+i);
}
void Tube(Tool::Mesh&m,V start,V end,float r0,float r1,unsigned sides){
    const V along=Normal(Add(end,Mul(start,-1))),right=Normal(Cross(along,std::abs(along[2])>.9f?V{0,1,0}:V{0,0,1})),up=Cross(along,right);
    const auto base=static_cast<unsigned>(m.vertices.size());
    for(unsigned ring=0;ring<2;++ring)for(unsigned i=0;i<=sides;++i){const float angle=2*pi*i/sides;const V n=Add(Mul(right,std::cos(angle)),Mul(up,std::sin(angle)));Vertex(m,Add(ring?end:start,Mul(n,ring?r1:r0)),n,{float(i)/sides,float(ring)},start,0);}
    for(unsigned i=0;i<sides;++i)for(unsigned n:{i,i+1,i+sides+2,i,i+sides+2,i+sides+1})m.indices.push_back(base+n);
}
Tool::Mesh Mesh(std::string name,unsigned material){Tool::Mesh m;m.name=std::move(name);m.material=material;m.hasNormals=m.hasUV=m.hasVertexExtras=true;return m;}
// Reproducible orthographic atlas rasterized offline from the high LOD mesh.
// Authored alpha is sampled, and flat source normals provide restrained shading.
Image Impostor(const std::vector<Tool::Mesh>&meshes,const Image&leaf,const Image&bark,float width,float height){
    constexpr unsigned size=192,views=8;Image atlas{size*views,size,std::vector<Pixel>(size*size*views)};
    for(unsigned view=0;view<views;++view){std::vector<float>depth(size*size,-1e9f);const float a=view*2*pi/views;V right{std::cos(a),std::sin(a),0},forward{-std::sin(a),std::cos(a),0};
        for(const auto&mesh:meshes)for(unsigned i=0;i<mesh.indices.size();i+=3){struct P{float x,y,z;Tool::Vec2 uv;V n;};P p[3];
            for(unsigned k=0;k<3;++k){const auto&v=mesh.vertices[mesh.indices[i+k]];V pos{v.position[0]*100,-v.position[2]*100,v.position[1]*100};p[k]={(Dot(pos,right)/width+.5f)*size,(1-pos[2]/height)*size,Dot(pos,forward),v.uv,{v.normal[0],-v.normal[2],v.normal[1]}};}
            const auto edge=[](const P&a,const P&b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);};const float area=edge(p[0],p[1],p[2].x,p[2].y);if(std::abs(area)<.0001f)continue;
            const int minX=std::max(0,int(std::floor(std::min({p[0].x,p[1].x,p[2].x})))),maxX=std::min(int(size)-1,int(std::ceil(std::max({p[0].x,p[1].x,p[2].x}))));
            const int minY=std::max(0,int(std::floor(std::min({p[0].y,p[1].y,p[2].y})))),maxY=std::min(int(size)-1,int(std::ceil(std::max({p[0].y,p[1].y,p[2].y}))));
            const auto&texture=mesh.material==0?bark:leaf;
            for(int y=minY;y<=maxY;++y)for(int x=minX;x<=maxX;++x){const float w0=edge(p[1],p[2],x+.5f,y+.5f)/area,w1=edge(p[2],p[0],x+.5f,y+.5f)/area,w2=1-w0-w1;if(std::min({w0,w1,w2})<0)continue;
                const float z=w0*p[0].z+w1*p[1].z+w2*p[2].z;if(z<depth[y*size+x])continue;
                const float u=w0*p[0].uv[0]+w1*p[1].uv[0]+w2*p[2].uv[0],v=w0*p[0].uv[1]+w1*p[1].uv[1]+w2*p[2].uv[1];
                auto color=texture.pixels[std::min(texture.h-1,unsigned(std::max(0.f,v)*texture.h))*texture.w+std::min(texture.w-1,unsigned(std::max(0.f,u)*texture.w))];if(color.a<128)continue;
                const float shade=.8f+.2f*std::abs(p[0].n[2]);color.r=static_cast<unsigned char>(color.r*shade);color.g=static_cast<unsigned char>(color.g*shade);color.b=static_cast<unsigned char>(color.b*shade);
                atlas.pixels[y*atlas.w+view*size+x]=color;depth[y*size+x]=z;
            }
        }
    }return atlas;
}
void Generate(const fs::path&root,const std::string&name,Vegetation::PlantKind kind){
    const bool grass=kind==Vegetation::PlantKind::Grass,bush=kind==Vegetation::PlantKind::Bush;
    const float height=grass?68:bush?210:1000,width=grass?100:bush?360:850;
    Tool::Scene scene;scene.name=name;scene.sourceFormat="H2 original procedural art";scene.unitPolicy="Y-up meters";
    for(const auto&texture:{std::string("bark"),std::string(grass?"leaf":"foliage"),name+"-impostor"}){Tool::Image image;image.name=texture;image.packPath="d:/ymir work/vegetation/modern/"+texture+".dds";scene.images.push_back(image);}
    for(unsigned i=0;i<3;++i){Tool::Material material;material.name=i==0?"rough bark":i==1?"thin foliage":"far atlas";material.baseTexture=int(i);material.alpha=i==0?Tool::AlphaMode::Opaque:Tool::AlphaMode::Mask;material.doubleSided=i!=0;scene.materials.push_back(material);}
    Vegetation::Metadata meta;meta.version=2;meta.geometry="vegetation/modern/"+name+".glb";meta.plantKind=kind;
    meta.bounds={{-width*.5f,-width*.5f,0},{width*.5f,width*.5f,height},true};meta.renderBounds={{-width*.7f,-width*.7f,-10},{width*.7f,width*.7f,height+20},true};
    meta.wind.branchAmplitude=grass?.065f:.018f;meta.wind.leafAmplitude=grass?.025f:.018f;meta.wind.frequency=grass?1.5f:1.15f;
    meta.nearDistance=grass?500:bush?1000:2500;meta.farDistance=grass?14000:bush?5500:11000;meta.cullDistance=grass?16000:bush?7500:18000;
    meta.lodDistances=grass?std::array<float,3>{700,1500,3500}:bush?std::array<float,3>{1400,3000,5500}:std::array<float,3>{2500,5500,9500};meta.lods.resize(4);
    std::vector<Tool::Mesh> high;
    for(unsigned lod=0;lod<3;++lod){auto bark=Mesh("bark-lod"+std::to_string(lod),0),leaves=Mesh("foliage-lod"+std::to_string(lod),1);
        std::mt19937 random(67312);auto unit=[&](){return (random()>>8)*(1.f/16777216.f);};
        if(grass){const unsigned blades=lod==0?16:lod==1?9:4;for(unsigned blade=0;blade<blades;++blade){const float angle=unit()*2*pi,h=height*(.55f+.45f*unit()),x=(unit()-.5f)*42,y=(unit()-.5f)*42;const V right{std::cos(angle),std::sin(angle),0};Card(leaves,{x,y,h*.5f},right,{.1f,.1f,1},9,h,.85f+.15f*unit());}}
        else {
            if(!bush)Tube(bark,{0,0,0},{12,-8,height*.82f},26,4,lod==0?12:6);
            const unsigned clusters=bush?22:48;
            for(unsigned cluster=0;cluster<clusters;++cluster){const float angle=cluster*2.399963f,radius=std::sqrt(unit())*width*.30f,z=bush?height*(.3f+.48f*unit()):height*(.40f+.43f*unit());
                const V center{std::cos(angle)*radius,std::sin(angle)*radius,z};
                if(cluster%2==0)Tube(bark,{0,0,bush?20.f:z*.6f},center,bush?5.f:9.f,1.2f,lod==0?7:4);
                const unsigned count=lod==0?16:lod==1?9:5;const float leafSize=(bush?36.f:63.f)*(lod==0?1.f:lod==1?1.22f:1.55f);
                for(unsigned leaf=0;leaf<count;++leaf){const float azimuth=unit()*2*pi;const V pos=Add(center,{(unit()-.5f)*width*.24f,(unit()-.5f)*width*.24f,(unit()-.5f)*height*.18f});
                    const V right{std::cos(azimuth),std::sin(azimuth),0},up=Normal({-.35f*std::sin(azimuth),.35f*std::cos(azimuth),.7f+unit()*.5f});Card(leaves,pos,right,up,leafSize*.6f,leafSize,.82f+.18f*unit());}
            }
        }
        if(!bark.vertices.empty()){meta.parts.push_back({Vegetation::PartKind::Branch,lod,unsigned(scene.meshes.size())});meta.lods[3-lod].meshes[0]=int(scene.meshes.size());meta.lods[3-lod].alpha[0]=0;scene.meshes.push_back(bark);if(lod==0)high.push_back(bark);}
        meta.parts.push_back({Vegetation::PartKind::Leaf,lod,unsigned(scene.meshes.size())});meta.lods[3-lod].meshes[2]=int(scene.meshes.size());meta.lods[3-lod].alpha[2]=128;scene.meshes.push_back(leaves);if(lod==0)high.push_back(leaves);
    }
    auto far=Mesh("eight-view-impostor",2);Card(far,{0,0,height*.5f},{1,0,0},{0,0,1},width,height);
    meta.parts.push_back({Vegetation::PartKind::Billboard,0,unsigned(scene.meshes.size())});meta.lods[0].meshes[4]=int(scene.meshes.size());meta.lods[0].alpha[4]=128;scene.meshes.push_back(far);
    // Match the roughly 23 m authored B1 beech selected for the sample override.
    // Apply to the offline content, leaving map transforms and collision data intact.
    if(!grass&&!bush) {
        constexpr float scale=2.3f;
        for(auto&mesh:scene.meshes)for(auto&vertex:mesh.vertices){vertex.position=Mul(vertex.position,scale);vertex.pivot=Mul(vertex.pivot,scale);}
        for(auto*bounds:{&meta.bounds,&meta.renderBounds})for(unsigned k=0;k<3;++k){bounds->min[k]*=scale;bounds->max[k]*=scale;}
        for(auto&distance:meta.lodDistances)distance*=scale;
        meta.nearDistance*=scale;meta.farDistance*=scale;meta.cullDistance*=scale;
    }
    Tool::Node node;node.name=name;for(unsigned i=0;i<scene.meshes.size();++i)node.meshes.push_back(i);scene.nodes.push_back(node);
    Tool::Report report;Tool::Options options;
    if(!Tool::Process(scene,options,report)||!Tool::WriteGLB(scene,root/meta.geometry,report)){for(const auto&issue:report.issues)std::cerr<<issue.message<<'\n';throw std::runtime_error("offline asset pipeline rejected generated scene");}
    std::ofstream(root/"vegetation/modern"/(name+".zveg"))<<Vegetation::SerializeMetadata(meta);
    DDS(root/"vegetation/modern"/(name+"-impostor.dds"),Impostor(high,grass?LeafTexture():FoliageTexture(),BarkTexture(),width,height));
    std::cout<<name<<" meshes="<<scene.meshes.size()<<" vertices="<<scene.optimization.verticesBefore<<"->"<<scene.optimization.verticesAfter<<" cache="<<scene.optimization.cacheMissRatioBefore<<"->"<<scene.optimization.cacheMissRatioAfter<<'\n';
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("usage: ZiiNANModernVegetationAssets output-root");const fs::path root=argv[1];fs::create_directories(root/"vegetation/modern");DDS(root/"vegetation/modern/leaf.dds",LeafTexture());DDS(root/"vegetation/modern/bark.dds",BarkTexture());
    DDS(root/"vegetation/modern/foliage.dds",FoliageTexture());
    Generate(root,"beech",Vegetation::PlantKind::Tree);Generate(root,"grass",Vegetation::PlantKind::Grass);Generate(root,"bush",Vegetation::PlantKind::Bush);return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
