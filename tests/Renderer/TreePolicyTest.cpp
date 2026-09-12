// ZiiNAN: Diligent SpeedTree rendering integration; API-neutral scope and owner tests also run OFF.
#include "Renderer/TreeRenderData.h"
#include <iostream>
using namespace Renderer;
struct TreeProbe final : ITreeRenderer {
    TreeGeometryPtr UploadGeometry(const TreeSource&,bool) override { return std::make_shared<TreeGeometry>(); }
    bool UpdateVertices(const TreeGeometryPtr&,const std::vector<TreeVertex>&) override { return true; }
    TerrainTexturePtr UploadTexture(const TerrainTextureData&) override { return {}; }
    void Draw(const void*,const TreeGeometryPtr&,const TerrainTexturePtr&,const TerrainTexturePtr&,const TreeDraw&) override {}
    void ReportFailure() override {}
};
int main()
{
    TreeProbe renderer;
    { TreeDrawScope scope(true); if(treeDrawScope) return 1; }
    treeRenderer=&renderer;
    { TreeDrawScope scope(true); if(treeDrawScope) return 2; }
    treeWorldFrame=true;
    {
        TreeDrawScope world(true); if(!treeDrawScope) return 3;
        { TreeDrawScope shadowOrMinimap(false); if(treeDrawScope) return 4; }
        if(!treeDrawScope) return 5;
    }
    if(treeDrawScope) return 6;
    auto model=std::make_shared<TreeModelData>();
    model->branches.geometry=std::make_shared<TreeGeometry>();
    std::weak_ptr<TreeGeometry> lifetime=model->branches.geometry;
    auto firstInstance=model,secondInstance=model;
    model.reset(); firstInstance.reset();
    if(lifetime.expired()) return 7;
    secondInstance.reset(); if(!lifetime.expired()) return 8;
    TreeDraw a; a.blend=true; a.alphaReference=84; a.stage1=2;
    TreeDraw fresh;
    if(fresh.blend || fresh.alphaReference || fresh.stage1 || !fresh.depthTest || !fresh.depthWrite) return 9;
    treeWorldFrame=false; treeRenderer=nullptr;
    std::cout << "Tree world/shadow/minimap scopes, shared ownership, fresh draw state: PASS\n";
}
