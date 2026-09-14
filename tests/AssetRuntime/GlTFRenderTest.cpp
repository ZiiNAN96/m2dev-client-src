#include "AssetRuntime/Providers.h"
#include "EterGrnLib/StdAfx.h"
#include "EterGrnLib/Thing.h"
#include "EterGrnLib/ModelInstance.h"
#include "EterLib/Camera.h"
#include "EterLib/ResourceManager.h"
#include "PackLib/PackManager.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/DiligentActorRenderer.h"
#include "Renderer/AssetMaterialRenderData.h"
#include "Renderer/SkinningBenchmark.h"
#include "GlTFFixtures.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace AssetRuntime;
float CCamera::CAMERA_MAX_DISTANCE = 2500.f;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static constexpr Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
static CResource* NewModel(const char* path) { return new CGraphicThing(path); }
static void SaveBMP(const std::vector<std::uint8_t>& rgb, std::uint32_t width, std::uint32_t height, int camera, bool special)
{
    const auto pitch = (width * 3 + 3) & ~3u;
    BITMAPFILEHEADER file{0x4d42, DWORD(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + pitch * height), 0, 0,
        sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)};
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info); info.biWidth = LONG(width); info.biHeight = -LONG(height);
    info.biPlanes = 1; info.biBitCount = 24; info.biSizeImage = pitch * height;
    std::ofstream out(std::string("e1x-market-stall-") + (special ? "blend" : "static") + "-camera-" + std::to_string(camera) + ".bmp", std::ios::binary);
    out.write(reinterpret_cast<const char*>(&file), sizeof(file));
    out.write(reinterpret_cast<const char*>(&info), sizeof(info));
    std::vector<std::uint8_t> row(pitch);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            row[x*3] = rgb[(y*width+x)*3+2]; row[x*3+1] = rgb[(y*width+x)*3+1]; row[x*3+2] = rgb[(y*width+x)*3];
        }
        out.write(reinterpret_cast<const char*>(row.data()), row.size());
    }
    Check(bool(out), "Render evidence image saved");
}

int main(int argc, char** argv)
{
    HWND window = nullptr;
    try {
        Check(argc == 2, "Expected original deterministic GLB path");
        CPackManager packs;
        CResourceManager resources;
        for (const auto extension : ModelExtensions()) resources.RegisterResourceNewFunctionPointer(extension.data(), NewModel);
        window = CreateWindowW(L"STATIC", L"E1-X normal static asset renderer", WS_OVERLAPPEDWINDOW,
            0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Renderer::DiligentD3D11Backend backend;
        Check(window && backend.Initialize({window,640,480}), "Existing Diligent backend initializes");
        {
            Renderer::DiligentStaticObjectRenderer renderer(backend);
            Check(renderer.Initialize(), "Existing static renderer initializes");
            Renderer::DiligentActorRenderer specialRenderer(backend);
            Check(specialRenderer.Initialize(), "Existing special-object renderer initializes");
            Renderer::staticObjectRenderer = &renderer;
            Renderer::actorRenderer = &specialRenderer;
            {
                const auto bytes = GlTFFixtures::Triangle(1,false).Bytes();
                auto plain = LoadModel("untextured.GLB",bytes);
                Check(bool(plain), "Provider selection accepts case-insensitive GLB suffix");
                auto* plainModel = new CGrannyModel;
                if (!plainModel->CreateFromAsset(plain.asset.Model(0))) {
                    delete plainModel;
                    throw std::runtime_error("Untextured GLB with generated normals/UV reaches real model consumer");
                }
                plainModel->Release(); plain.asset.Reset();
                Check(!LoadModel("unknown.format",bytes), "Unknown format is explicitly rejected");
            }
            {
                Renderer::StaticObjectLoadScope scope;
                CGraphicThing::TRef thing(resources.GetResourcePointer(argv[1]));
                Check(!thing.IsNull() && !thing->IsEmpty(), "Real GLB loads through ResourceManager and provider selection");
                CGraphicThing::TRef second(resources.GetResourcePointer(argv[1]));
                Check(thing.GetPointer() == second.GetPointer() && liveDocuments == 1, "Repeated load shares resource and parsed document");
                auto* model = thing->GetModelPointer(0);
                Check(model && model->GetMeshCount() >= 8 && model->GetActorSource(), "Scene nodes reach normal immutable mesh capture");
                const bool special = !model->GetStaticObjectSource();
                Renderer::ITextureUploader& uploader = special ? static_cast<Renderer::ITextureUploader&>(specialRenderer) : renderer;
                CGrannyModelInstance instance;
                instance.SetMainModelPointer(model, nullptr);
                Check(!instance.IsEmpty(), "Rigid instance uses provider static pose boundary");
                Math::Matrix world;
                std::memcpy(&world, identity.data(), 64);
                instance.DeformNoSkin(&world);
                Math::Vector3 low, high;
                instance.GetBoundBox(&low, &high);
                Check(low.x < -130 && high.x > 130 && high.z > 230 && low.z < 5, "World bounds reflect transformed meter-scale nodes");
                auto geometry = special ? specialRenderer.CreateGeometry(*model->GetActorSource()) : renderer.UploadGeometry(*model->GetStaticObjectSource());
                Check(bool(geometry), "Normal static GPU buffers created");
                auto& palette = instance.GetStaticObjectMaterialPalette();
                std::vector<Renderer::TerrainTexturePtr> textures;
                bool mask = false, blend = false, twoSided = false;
                for (DWORD i = 0; i < palette.GetMaterialCount(); ++i) {
                    auto& material = palette.GetMaterialRef(i);
                    auto* image = material.GetImagePointer(0);
                    Check(image != nullptr, "Embedded or neutral white texture is a normal image resource");
                    auto texture = image->GetAssetTexture(uploader);
                    Check(bool(texture) && texture == image->GetAssetTexture(uploader), "Image decode/upload cache is reused");
                    textures.push_back(texture);
                    const auto& asset = material.GetAsset();
                    mask |= asset.alphaTest; blend |= asset.blending; twoSided |= asset.culling == Culling::None;
                }
                Check(mask && blend == special && twoSided, "Material classification selects the existing static or transparent object path");
                const auto liveTextures = [&] { return renderer.LiveTextureCount()+specialRenderer.LiveTextureCount(); };
                const auto textureCount = liveTextures();
                for (int camera = 0; camera != 3; ++camera) {
                    const std::uint32_t w = camera == 1 ? 720 : 640, h = 480;
                    Check(backend.Resize(w,h) && backend.BeginFrame(), "Camera/resize frame begins");
                    renderer.ResetFrame(); specialRenderer.ResetFrame(); backend.Clear({true,Renderer::ClearColor{.045f,.055f,.075f,1}});
                    Math::Vector3 eye(camera == 2 ? -360.f : 360.f, camera == 1 ? 520.f : -520.f, 330.f), target(0,0,110), up(0,0,1);
                    Math::Matrix view, projection;
                    Math::MatrixLookAtRH(&view,&eye,&target,&up);
                    Math::MatrixPerspectiveFovRH(&projection, .75f, float(w)/float(h), 1.f, 2000.f);
                    unsigned expectedDraws = 0;
                    for (const auto type : {CGrannyMaterial::TYPE_DIFFUSE_PNT, CGrannyMaterial::TYPE_BLEND_PNT}) {
                        for (auto* node = model->GetMeshNodeList(CGrannyMesh::TYPE_RIGID,type); node; node=node->pNextMeshNode) {
                            for (auto* group=node->pMesh->GetTriGroupNodeList(type); group; group=group->pNextTriGroupNode) {
                                Renderer::StaticObjectDraw draw;
                                std::memcpy(draw.matrices.world.data(), instance.GetStaticObjectWorldMatrix(node->iMesh),64);
                                std::memcpy(draw.matrices.view.data(), &view,64); std::memcpy(draw.matrices.projection.data(), &projection,64);
                                draw.normalTransform = identity; draw.ambient = {.85f,.85f,.85f,1};
                                Renderer::ApplyAssetMaterial(palette.GetMaterialRef(group->mtrlIndex).GetAsset(), draw);
                                draw.firstIndex=group->idxPos; draw.indexCount=group->triCount*3;
                                draw.baseVertex=node->pMesh->GetVertexBasePosition(); draw.vertexCount=node->pMesh->GetVertexCount();
                                if (special) specialRenderer.Draw(&instance,geometry,textures[group->mtrlIndex],draw);
                                else renderer.Draw(geometry,textures[group->mtrlIndex],draw);
                                ++expectedDraws;
                            }
                        }
                    }
                    Check(!renderer.Failed() && !specialRenderer.Failed() && renderer.DrawCount()+specialRenderer.DrawCount()==expectedDraws && expectedDraws>=8, "All real scene meshes submit through normal Diligent renderer");
                    std::vector<std::uint8_t> image; std::uint32_t iw{},ih{};
                    Check(backend.CaptureRGB(image,iw,ih), "Actual native image readback succeeds");
                    Check(std::count_if(image.begin(),image.end(),[](auto v){return v>70;}) > 2000, "Asset remains visible after camera change");
                    SaveBMP(image,iw,ih,camera,special);
                    backend.EndFrame(); backend.Present(); renderer.ReleaseBindings(); specialRenderer.ReleaseBindings();
                    Check(liveTextures()==textureCount && renderer.LiveGeometryCount()+specialRenderer.LiveGeometryCount()==1, "No per-frame GPU asset creation");
                }
                Check(backend.Resize(0,0) && backend.Resize(640,480), "Minimize/restore backend lifecycle succeeds");
                instance.Clear(); textures.clear(); geometry.reset(); second.Clear(); thing.Clear();
            }
            resources.DestroyDeletingList(); resources.Destroy(); renderer.ReleaseBindings(); specialRenderer.ReleaseBindings();
            Check(liveDocuments==0 && liveAnimationInstances==0 && liveMeshBindings==0, "All GLB document/session/binding owners released");
            Check(renderer.LiveGeometryCount()==0 && renderer.LiveTextureCount()==0 && specialRenderer.LiveGeometryCount()==0 && specialRenderer.LiveTextureCount()==0, "All GLB GPU resources released");
            Check(Renderer::skinningCpuCalls==0 && Renderer::skinningFallbacks==0, "No CPU skinning or hidden fallback");
            Renderer::staticObjectRenderer = nullptr;
            Renderer::actorRenderer = nullptr;
        }
        backend.Shutdown(); DestroyWindow(window); window=nullptr;
        std::cout << "PASS real GLB resource cache -> AssetRuntime -> model/mesh/material -> normal static Diligent buffers/draws; 3 camera readbacks; resize/suspend/restore; owners/GPU resources=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n'; if(window) DestroyWindow(window); return 1;
    }
}
