#include "EterGrnLib/StdAfx.h"
#include "EterGrnLib/Material.h"
#include "EterLib/ResourceManager.h"
#include "EterLib/Camera.h"
#include "PackLib/PackManager.h"
#include "Renderer/AssetMaterialRenderData.h"
#include "Renderer/EffectRenderData.h"
#include <stb_image_write.h>
#include <iostream>
#include <stdexcept>
#include <new>

float CCamera::CAMERA_MAX_DISTANCE = 2500.f;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

static std::shared_ptr<AssetRuntime::EncodedImage> Image(bool jpeg = false)
{
    auto image = std::make_shared<AssetRuntime::EncodedImage>();
    image->id = jpeg ? "asset-test:embedded.jpg" : "asset-test:embedded.png";
    image->mimeType = jpeg ? "image/jpeg" : "image/png";
    const auto append = [](void* context, void* data, int size) {
        auto& bytes = *static_cast<std::vector<std::byte>*>(context);
        const auto* begin = static_cast<const std::byte*>(data);
        bytes.insert(bytes.end(), begin, begin + size);
    };
    const unsigned char rgba[] = {255,0,0,255, 0,255,0,128, 0,0,255,0, 255,255,255,255};
    Check((jpeg ? stbi_write_jpg_to_func(append, &image->bytes, 2, 2, 4, rgba, 100) :
        stbi_write_png_to_func(append, &image->bytes, 2, 2, 4, rgba, 8)) != 0, "Deterministic image fixture encoding");
    return image;
}

struct UploadProbe final : Renderer::ITextureUploader
{
    unsigned uploads{};
    std::vector<uint8_t> pixels;
    Renderer::TerrainTexturePtr UploadTexture(const Renderer::TerrainTextureData& data) override
    {
        ++uploads;
        Check(data.width && data.height && !data.mips.empty(), "Decoded image reaches existing uploader");
        const auto* begin = static_cast<const uint8_t*>(data.mips[0].data);
        pixels.assign(begin, begin + data.mips[0].size);
        return std::make_shared<Renderer::TerrainTexture>();
    }
};

static void CheckEffectImageLifetime()
{
    struct FileImage : CGraphicImage {
        FileImage() : CGraphicImage("asset-test:effect.png") {}
        using CGraphicImage::OnLoad;
        using CGraphicImage::OnClear;
    } image;
    const auto encoded = Image();
    Check(image.OnLoad(static_cast<int>(encoded->bytes.size()), encoded->bytes.data()), "Ordinary effect image decoded");
    const auto source = image.GetTexturePointer()->GetTextureBinding().source;
    UploadProbe uploader;
    std::weak_ptr<Renderer::TerrainTexture> retained;
    for (unsigned instance = 0; instance != 30; ++instance) {
        Renderer::EffectResources effect;
        effect.textures[image.GetFileName()] = image.GetAssetTexture(uploader);
        Check(bool(effect.textures.begin()->second), "Short-lived effect receives image texture");
        if (instance == 0) retained = effect.textures.begin()->second;
        else Check(retained.lock() == effect.textures.begin()->second, "Repeated effects reuse the native image upload");
    }
    Check(uploader.uploads == 1 && !retained.expired(), "Ending particle instances does not evict a live image");
    Check(uploader.pixels == source->mips[0].pixels, "Effect upload preserves decoded image bytes and alpha");
    image.DestroyDeviceObjects();
    Check(retained.expired(), "Device destruction releases the image-owned GPU texture");
    Check(image.OnLoad(static_cast<int>(encoded->bytes.size()), encoded->bytes.data()), "Effect image reload");
    retained = image.GetAssetTexture(uploader);
    Check(uploader.uploads == 2 && !retained.expired(), "Reload receives a fresh GPU upload");
    image.OnClear();
    Check(retained.expired(), "Resource clear releases the cached effect texture");
}

static void CheckStates()
{
    using namespace Renderer;
    AssetRuntime::MaterialAsset material;
    StaticObjectDraw legacy;
    legacy.blend = true; legacy.depthWrite = false; legacy.alphaReference = 17;
    legacy.actorStage = ActorMaterialStage::Add; legacy.textureFactor = {.2f,.3f,.4f,.5f};
    ApplyAssetMaterial(material, legacy);
    Check(legacy.blend && !legacy.depthWrite && legacy.alphaReference == 17 &&
        legacy.actorStage == ActorMaterialStage::Add && legacy.textureFactor[0] == .2f, "GR2 pass defaults unchanged");
    material.explicitRenderState = true;
    material.baseColorFactor = {.2f,.4f,.6f,.3f};
    material.alphaTest = false;
    StaticObjectDraw draw;
    ApplyAssetMaterial(material, draw);
    Check(!draw.blend && draw.depthWrite && draw.alphaTest == StaticObjectAlphaTest::Disabled &&
        draw.factorAlphaOnly && !draw.factorAlpha && draw.textureFactor[3] == 1 &&
        draw.textureFactor[0] == .2f && draw.actorStage == ActorMaterialStage::Modulate, "OPAQUE ignores texture/factor alpha and retains RGB");
    material.alphaTest = true;
    material.alphaCutoff = .5f;
    material.culling = AssetRuntime::Culling::None;
    ApplyAssetMaterial(material, draw);
    Check(!draw.blend && draw.depthWrite && draw.alphaTest == StaticObjectAlphaTest::GreaterEqual &&
        draw.alphaReference == 128 && draw.factorAlpha && !draw.factorAlphaOnly && draw.textureFactor[3] == .3f &&
        draw.cull == StaticObjectCull::None, "MASK cutoff, factor alpha and double-sided state");
    material.alphaCutoff = 2.f;
    ApplyAssetMaterial(material, draw);
    Check(draw.alphaTest == StaticObjectAlphaTest::Greater && draw.alphaReference == 255, "MASK cutoff above one discards every pixel");
    material.alphaTest = false; material.blending = true; material.depthWrite = false;
    ApplyAssetMaterial(material, draw);
    Check(draw.blend && !draw.depthWrite && draw.alphaTest == StaticObjectAlphaTest::Disabled &&
        draw.factorAlpha && !draw.factorAlphaOnly, "BLEND routes factor alpha and disables depth writes");
}

int main()
{
    try {
        CheckStates();
        CPackManager packs;
        CResourceManager resources;
        const auto initialSources = Renderer::liveSourceTextures.load();
        CheckEffectImageLifetime();
        UploadProbe firstUploader, secondUploader;
        std::weak_ptr<const AssetRuntime::EncodedImage> retainedPayload;
        {
            AssetRuntime::MaterialAsset source;
            source.explicitRenderState = true; source.alphaTest = false;
            source.culling = AssetRuntime::Culling::None;
            source.embeddedImages[0] = Image();
            retainedPayload = source.embeddedImages[0];
            CGrannyMaterial first, second, copied;
            Check(first.CreateFromAsset(source) && second.CreateFromAsset(source), "Embedded materials construct through real resource decoder");
            Check(first.GetImagePointer(0) == second.GetImagePointer(0), "Repeated image identity shares cached resource");
            const auto decoded = first.GetTextureBinding(0).source;
            Check(decoded && decoded->desc.width == 2 && decoded->desc.height == 2 &&
                decoded->desc.format == Renderer::TerrainTextureFormat::RGBA8, "PNG decoded to normal texture source");
            const std::vector<uint8_t> expected{255,0,0,255, 0,255,0,128, 0,0,255,0, 255,255,255,255};
            Check(decoded->mips[0].pixels == expected, "PNG color, alpha and top-origin UV orientation preserved");
            auto texture = first.GetImagePointer(0)->GetAssetTexture(firstUploader);
            Check(texture && texture == second.GetImagePointer(0)->GetAssetTexture(firstUploader) && firstUploader.uploads == 1,
                "Texture uploaded once across materials and draws");
            Check(first.GetImagePointer(0)->GetAssetTexture(secondUploader) && secondUploader.uploads == 1,
                "Distinct uploader receives its own GPU resource");
            alignas(UploadProbe) unsigned char storage[sizeof(UploadProbe)];
            auto* recreated = new (storage) UploadProbe;
            auto oldTexture = first.GetImagePointer(0)->GetAssetTexture(*recreated);
            recreated->~UploadProbe();
            recreated = new (storage) UploadProbe;
            Check(first.GetImagePointer(0)->GetAssetTexture(*recreated) != oldTexture && recreated->uploads == 1,
                "Renderer recreation at the same address cannot reuse a stale GPU texture");
            recreated->~UploadProbe();
            copied.Copy(first);
            Check(copied.IsTwoSided() && copied.GetAsset().culling == AssetRuntime::Culling::None, "Authored double-sided state survives palette copy");
            source.embeddedImages[0].reset();
            Check(!retainedPayload.expired() && first.GetTextureBinding(0).source == decoded, "Image lifetime survives provider payload release");
            first.GetImagePointer(0)->DestroyDeviceObjects();
            Check(first.GetImagePointer(0)->CreateDeviceObjects() && first.GetTextureBinding(0).source->mips[0].pixels == expected,
                "Embedded image device restore decodes retained payload without a temporary file");
            Check(first.GetImagePointer(0)->GetAssetTexture(firstUploader) && firstUploader.uploads == 2,
                "Device restore invalidates the prior uploaded texture cache");
            auto changed = Image(); changed->bytes.back() ^= std::byte{1};
            Check(!resources.GetEncodedImagePointer(changed), "Same ID with different bytes rejected instead of reusing unrelated image");
            auto bad = Image(); bad->id = "asset-test:invalid.png"; bad->bytes.resize(8);
            source.embeddedImages[0] = bad;
            CGrannyMaterialPalette rejected;
            Check(rejected.RegisterMaterial(source) == CGrannyMaterialPalette::InvalidMaterial && rejected.GetMaterialCount() == 0,
                "Bad encoded texture prevents material construction");
            auto mislabeled = Image(); mislabeled->id = "asset-test:mislabeled.jpg"; mislabeled->mimeType = "image/jpeg";
            Check(!resources.GetEncodedImagePointer(mislabeled), "MIME and image signature mismatch rejected");
            auto oversized = Image(); oversized->id = "asset-test:oversized.png";
            oversized->bytes[16] = std::byte{0x00}; oversized->bytes[17] = std::byte{0x01};
            oversized->bytes[18] = std::byte{0x00}; oversized->bytes[19] = std::byte{0x00};
            Check(!resources.GetEncodedImagePointer(oversized), "Giant image dimensions rejected before pixel allocation");
            source.embeddedImages[0] = Image(true);
            CGrannyMaterial jpeg;
            Check(jpeg.CreateFromAsset(source) && jpeg.GetImagePointer(0)->GetWidth() == 2 && jpeg.GetImagePointer(0)->GetHeight() == 2,
                "Embedded JPEG uses existing decoder");
            source.embeddedImages[0].reset();
            source.blending = true; source.depthWrite = false;
            CGrannyMaterial white;
            Check(white.CreateFromAsset(source) && white.GetImagePointer(0)->GetWidth() == 1 &&
                white.GetType() == CGrannyMaterial::TYPE_BLEND_PNT && white.GetAsset().stage == AssetRuntime::MaterialStage::Diffuse,
                "Untextured base factor uses cached white source and correct transparent pass");
        }
        resources.DestroyDeletingList();
        resources.Destroy();
        Check(retainedPayload.expired(), "Resource shutdown releases encoded asset ownership");
        Check(Renderer::liveSourceTextures.load() == initialSources, "No decoded source leak after material shutdown");
        std::cout << "PASS embedded PNG/JPEG, strict errors, material alpha/cull/color, cache, uploads, lifetime and GR2 state isolation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n'; return 1;
    }
}
