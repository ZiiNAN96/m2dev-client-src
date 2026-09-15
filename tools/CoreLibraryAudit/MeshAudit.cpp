#include "AuditCommon.h"
#include <meshoptimizer.h>
#include <cstring>
#include <set>
using namespace CLib;
static void Audit(const std::string& label,size_t mesh,const std::vector<std::byte>& vertices,size_t stride,const std::vector<uint32_t>& original) {
    const size_t count=vertices.size()/stride;Check(count&&original.size()%3==0,"Mesh input");
    auto current=original;meshopt_optimizeVertexCache(current.data(),current.data(),current.size(),count);
    auto overdraw=current;meshopt_optimizeOverdraw(overdraw.data(),current.data(),current.size(),reinterpret_cast<const float*>(vertices.data()),count,stride,1.05f);
    auto acmr=[&](const auto& indices){return meshopt_analyzeVertexCache(indices.data(),indices.size(),count,16,0,0).acmr;};
    auto od=[&](const auto& indices){return meshopt_analyzeOverdraw(indices.data(),indices.size(),reinterpret_cast<const float*>(vertices.data()),count,stride).overdraw;};
    auto fetchedIndices=current;
    std::vector<std::byte> fetched(vertices.size());const size_t fetchedCount=meshopt_optimizeVertexFetch(fetched.data(),fetchedIndices.data(),fetchedIndices.size(),vertices.data(),count,stride);fetched.resize(fetchedCount*stride);
    std::vector<unsigned char> vb(meshopt_encodeVertexBufferBound(fetchedCount,stride)),ib(meshopt_encodeIndexBufferBound(current.size(),fetchedCount));
    const auto vbSize=meshopt_encodeVertexBuffer(vb.data(),vb.size(),fetched.data(),fetchedCount,stride);const auto ibSize=meshopt_encodeIndexBuffer(ib.data(),ib.size(),fetchedIndices.data(),fetchedIndices.size());Check(vbSize&&ibSize,"Mesh codec");
    std::vector<std::byte> roundtrip(fetched.size());std::vector<uint32_t> decoded(current.size());
    Check(meshopt_decodeVertexBuffer(roundtrip.data(),fetchedCount,stride,vb.data(),vbSize)==0&&roundtrip==fetched,"Lossless vertex roundtrip");
    Check(meshopt_decodeIndexBuffer(decoded.data(),decoded.size(),4,ib.data(),ibSize)==0,"Index decode");
    // Index codec may rotate each triangle cyclically. Winding and triangle order must survive.
    for(size_t i=0;i<decoded.size();i+=3){bool match=false;for(size_t r=0;r<3;++r)match|=decoded[i]==fetchedIndices[i+r]&&decoded[i+1]==fetchedIndices[i+(r+1)%3]&&decoded[i+2]==fetchedIndices[i+(r+2)%3];Check(match,"Index triangle parity");}
    std::vector<uint32_t> lod(original.size());float lodError=0;const auto lodCount=meshopt_simplify(lod.data(),original.data(),original.size(),reinterpret_cast<const float*>(vertices.data()),count,stride,(original.size()/6)*3,.01f,meshopt_SimplifyLockBorder,&lodError);
    auto timings=Bench([&](int){meshopt_optimizeOverdraw(overdraw.data(),current.data(),current.size(),reinterpret_cast<const float*>(vertices.data()),count,stride,1.05f);},4);
    std::cout<<"{\"case\":\""<<label<<"\",\"mesh\":"<<mesh<<",\"vertices\":"<<count<<",\"indices\":"<<original.size()<<",\"fetch_vertices\":"<<fetchedCount<<",\"raw_stream_bytes\":"<<vertices.size()+original.size()*4<<",\"encoded_stream_bytes\":"<<vbSize+ibSize<<",\"original_acmr\":"<<acmr(original)<<",\"current_cache_acmr\":"<<acmr(current)<<",\"extra_overdraw_acmr\":"<<acmr(overdraw)<<",\"original_overdraw_proxy\":"<<od(original)<<",\"current_overdraw_proxy\":"<<od(current)<<",\"extra_overdraw_proxy\":"<<od(overdraw)<<",\"lod_indices\":"<<lodCount<<",\"lod_relative_error\":"<<lodError<<",\"codec_roundtrip\":true";
    PrintStats("offline_overdraw",timings);std::cout<<"}\n";
}
int main(int argc,char** argv) {
    try {Check(argc==3,"CoreMeshAudit asset-root manifest.tsv");std::ifstream manifest(argv[2]);Check(bool(manifest),"Mesh manifest missing");std::string line;std::cout<<std::setprecision(9);
        while(std::getline(manifest,line)){if(line.empty()||line[0]=='#')continue;const auto tab=line.find('\t');Check(tab!=std::string::npos,"Mesh manifest line");const auto label=line.substr(0,tab);const auto path=std::filesystem::path(argv[1])/line.substr(tab+1);auto bytes=Read(path);
            if(path.extension()==".gr2") {auto contents=GR::Read(GR::File(bytes));Check(!contents.modelData.empty(),"GR2 meshes");for(size_t m=0;m<contents.modelData[0].meshes.size();++m){const auto& mesh=contents.modelData[0].meshes[m];std::vector<std::byte> data(mesh.vertices.size()*sizeof(GR::Vertex));std::memcpy(data.data(),mesh.vertices.data(),data.size());Audit(label,m,data,sizeof(GR::Vertex),mesh.indices);}}
            else {auto loaded=AssetRuntime::GetGlTFAssetProvider().Load(path.string(),bytes);Check(bool(loaded),loaded.diagnostic);const auto* doc=loaded.asset.Get();for(size_t m=0;m<doc->Models()[0].meshes.size();++m){const auto& mesh=doc->Models()[0].meshes[m];const auto stride=AssetRuntime::VertexStride(mesh.vertexLayout);std::vector<std::byte> data(mesh.vertexCount*stride);std::vector<uint32_t> indices(mesh.indexCount);Check(doc->CopyVertices(0,m,mesh.vertexLayout,data)==AssetRuntime::AssetError::None,"GLB vertices");if(mesh.indexWidth==AssetRuntime::IndexWidth::UInt16){std::vector<uint16_t> small(indices.size());Check(doc->CopyIndices(0,m,mesh.indexWidth,std::as_writable_bytes(std::span(small)))==AssetRuntime::AssetError::None,"GLB short indices");std::copy(small.begin(),small.end(),indices.begin());}else Check(doc->CopyIndices(0,m,mesh.indexWidth,std::as_writable_bytes(std::span(indices)))==AssetRuntime::AssetError::None,"GLB indices");Audit(label,m,data,stride,indices);}}
        }Check(!AssetRuntime::liveDocuments,"Mesh lifetime");return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
