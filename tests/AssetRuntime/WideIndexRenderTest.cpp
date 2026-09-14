#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "Renderer/DiligentActorRenderer.h"
#include "Renderer/AssetMaterialRenderData.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

Renderer::StaticObjectDraw MakeDraw(uint32_t vertices, uint32_t baseVertex)
{
    Renderer::StaticObjectDraw draw;
    for (auto* matrix : {&draw.matrices.world, &draw.matrices.view, &draw.matrices.projection, &draw.normalTransform})
        (*matrix)[0] = (*matrix)[5] = (*matrix)[10] = (*matrix)[15] = 1;
    draw.cull = Renderer::StaticObjectCull::None;
    draw.firstIndex = 3;
    draw.indexCount = 3;
    draw.vertexCount = vertices;
    draw.baseVertex = baseVertex;
    return draw;
}

void CheckPixels(Renderer::DiligentD3D11Backend& backend)
{
    std::vector<uint8_t> pixels;
    uint32_t width{}, height{};
    Check(backend.CaptureRGB(pixels, width, height) && width && height, "Readback succeeds");
    const auto center = (std::size_t(height / 2) * width + width / 2) * 3;
    Check(pixels[center] > 240 && pixels[center + 1] > 240 && pixels[center + 2] > 240,
        "Wide index triangle reaches the normal framebuffer");
    Check(pixels[0] < 8 && pixels[1] < 8 && pixels[2] < 8, "Clear pixels remain black");
}

void CheckMaterialPixels(Renderer::DiligentD3D11Backend& backend,
    Renderer::DiligentStaticObjectRenderer& renderer, const Renderer::StaticObjectSource& source)
{
    auto front = renderer.UploadGeometry(source);
    auto reversedSource = source;
    std::swap(reversedSource.indices[4], reversedSource.indices[5]);
    auto reversed = renderer.UploadGeometry(reversedSource);
    Check(front && reversed, "Upload both material face orientations");
    std::array<Renderer::TerrainTexturePtr, 4> textures;
    for (std::size_t i = 0; i < textures.size(); ++i) {
        const std::array<uint8_t, 4> rgba{255, 255, 255, std::array<uint8_t, 4>{0, 127, 128, 255}[i]};
        textures[i] = renderer.UploadTexture({1, 1, Renderer::TerrainTextureFormat::RGBA8,
            {{rgba.data(), rgba.size(), 4}}});
        Check(bool(textures[i]), "Upload material alpha fixture");
    }
    auto sample = [&](const AssetRuntime::MaterialAsset& material, std::size_t image,
        const Renderer::StaticObjectGeometryPtr& geometry, Renderer::ClearColor clear = {0, 0, 0, 1}) {
        renderer.ResetFrame();
        Check(backend.BeginFrame(), "Begin material sample");
        backend.Clear({true, clear});
        auto draw = MakeDraw(3, 0);
        Renderer::ApplyAssetMaterial(material, draw);
        renderer.Draw(geometry, textures[image], draw);
        Check(!renderer.Failed() && renderer.DrawCount() == 1, "Material uses ordinary draw pipeline");
        std::vector<uint8_t> pixels;
        uint32_t width{}, height{};
        Check(backend.CaptureRGB(pixels, width, height) && width && height, "Read material sample");
        const auto center = (std::size_t(height / 2) * width + width / 2) * 3;
        const std::array<int, 3> color{pixels[center], pixels[center + 1], pixels[center + 2]};
        backend.EndFrame();
        return color;
    };
    auto matches = [](const std::array<int, 3>& actual, const std::array<int, 3>& expected) {
        for (std::size_t channel = 0; channel < 3; ++channel)
            if (std::abs(actual[channel] - expected[channel]) > 2) return false;
        return true;
    };
    AssetRuntime::MaterialAsset material;
    material.explicitRenderState = true;
    material.culling = AssetRuntime::Culling::None;
    material.alphaTest = false;
    material.baseColorFactor = {.2f, .4f, .6f, 0};
    Check(matches(sample(material, 0, front), {51, 102, 153}),
        "OPAQUE ignores texture/factor alpha and multiplies RGB factor");

    material.alphaTest = true;
    material.baseColorFactor = {1, 1, 1, 1};
    Check(matches(sample(material, 1, front), {0, 0, 0}), "MASK discards alpha 127 below cutoff 0.5");
    Check(matches(sample(material, 2, front), {255, 255, 255}), "MASK keeps alpha 128 above cutoff 0.5");
    material.baseColorFactor[3] = .49f;
    Check(matches(sample(material, 3, front), {0, 0, 0}), "MASK cutoff sees multiplied factor alpha");
    material.baseColorFactor[3] = .51f;
    Check(matches(sample(material, 3, front), {255, 255, 255}), "MASK factor alpha above cutoff remains visible");

    material.alphaTest = false;
    material.blending = true;
    material.depthWrite = false;
    material.baseColorFactor = {.8f, .4f, .2f, .5f};
    // Texture alpha 128/255 times factor .5; ordinary SRCALPHA/INVSRCALPHA over this background.
    Check(matches(sample(material, 2, front, {.1f, .2f, .3f, 1}), {70, 64, 70}),
        "BLEND uses texture alpha times factor alpha and the destination color");

    material.blending = false;
    material.depthWrite = true;
    material.baseColorFactor = {1, 1, 1, 1};
    material.culling = AssetRuntime::Culling::Clockwise;
    const auto first = sample(material, 3, front);
    const auto second = sample(material, 3, reversed);
    Check((matches(first, {255, 255, 255}) && matches(second, {0, 0, 0})) ||
        (matches(second, {255, 255, 255}) && matches(first, {0, 0, 0})),
        "Single-sided material culls exactly the reversed face");
    material.culling = AssetRuntime::Culling::None;
    Check(matches(sample(material, 3, front), {255, 255, 255}) &&
        matches(sample(material, 3, reversed), {255, 255, 255}),
        "Double-sided material renders both face orientations");
    renderer.ReleaseBindings();
}
}

int main()
{
    HWND window = CreateWindowExA(0, "STATIC", "E1-X wide indices", WS_POPUP,
        0, 0, 160, 120, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    try {
        Check(window != nullptr, "Create hidden native test window");
        Renderer::DiligentD3D11Backend backend;
        Check(backend.Initialize({window, 160, 120}), "Initialize native D3D11");
        {
            Renderer::DiligentStaticObjectRenderer renderer(backend);
            Check(renderer.Initialize(), "Initialize normal static renderer");
            const std::array<uint8_t, 4> white{255, 255, 255, 255};
            const Renderer::TerrainTextureData image{1, 1, Renderer::TerrainTextureFormat::RGBA8,
                {{white.data(), white.size(), 4}}};
            auto texture = renderer.UploadTexture(image);
            Check(bool(texture), "Upload shared ordinary texture");

            Renderer::StaticObjectSource wide;
            wide.vertices.resize(65538);
            wide.vertices[1] = {-.8f, -.8f, .5f, 0, 0, 1, 0, 0};
            wide.vertices[65536] = {0, .8f, .5f, 0, 0, 1, .5f, 1};
            wide.vertices[65537] = {.8f, -.8f, .5f, 0, 0, 1, 1, 0};
            wide.indices32 = {0, 0, 0, 0, 65535, 65536};
            auto geometry = renderer.UploadGeometry(wide);
            Check(bool(geometry), "Upload uint32 stream containing index 65536");
            const auto wideDraw = MakeDraw(65537, 1);
            Check(backend.BeginFrame(), "Begin static wide-index frame");
            backend.Clear({true, Renderer::ClearColor{0, 0, 0, 1}});
            renderer.Draw(geometry, texture, wideDraw);
            Check(!renderer.Failed() && renderer.DrawCount() == 1, "Draw uint32 indices with nonzero index/vertex bases");
            CheckPixels(backend);
            backend.EndFrame();

            Renderer::StaticObjectSource legacy;
            legacy.vertices = {wide.vertices[1], wide.vertices[65536], wide.vertices[65537]};
            legacy.indices = {0, 0, 0, 0, 1, 2};
            auto legacyGeometry = renderer.UploadGeometry(legacy);
            Check(bool(legacyGeometry), "Original uint16 index stream still uploads");
            renderer.ResetFrame();
            Check(backend.BeginFrame(), "Begin legacy index frame");
            backend.Clear({true, Renderer::ClearColor{0, 0, 0, 1}});
            renderer.Draw(legacyGeometry, texture, MakeDraw(3, 0));
            Check(!renderer.Failed() && renderer.DrawCount() == 1, "Original uint16 draw stays valid");
            CheckPixels(backend);
            backend.EndFrame();

            CheckMaterialPixels(backend, renderer, legacy);

            {
                Renderer::DiligentActorRenderer actors(backend);
                Check(actors.Initialize(), "Initialize ordinary actor renderer");
                Renderer::ActorModelSource source;
                source.vertexCount = static_cast<uint32_t>(wide.vertices.size());
                source.rigidVertices = wide.vertices;
                source.indices32 = wide.indices32;
                Check(source.IsRigid(), "Wide neutral actor mesh remains rigid");
                auto actorGeometry = actors.CreateGeometry(source);
                auto actorTexture = actors.UploadTexture(image);
                Check(actorGeometry && actorTexture, "Rigid actor keeps uint32 stream");
                Check(backend.BeginFrame(), "Begin actor index frame");
                backend.Clear({true, Renderer::ClearColor{0, 0, 0, 1}});
                actors.Draw(&source, actorGeometry, actorTexture, wideDraw);
                Check(!actors.Failed() && actors.DrawCount() == 1 && actors.Uploads() == 0,
                    "Wide rigid actor draw needs no deformation upload");
                CheckPixels(backend);
                backend.EndFrame();
                actors.ReleaseBindings();
                actorGeometry.reset(); actorTexture.reset();
                Check(actors.LiveGeometryCount() == 0 && actors.LiveTextureCount() == 0, "Actor resources released");
            }
            // Renderer failures latch for diagnostics; fault cases follow all successful pixel checks.
            renderer.ResetFrame();
            Check(backend.BeginFrame(), "Begin invalid index frame");
            auto badDraw = wideDraw;
            --badDraw.vertexCount;
            renderer.Draw(geometry, texture, badDraw);
            Check(renderer.Failed() && renderer.DrawCount() == 0, "Mesh-local uint32 index outside vertex range is rejected");
            badDraw = wideDraw;
            badDraw.firstIndex = std::numeric_limits<uint32_t>::max();
            renderer.Draw(geometry, texture, badDraw);
            Check(renderer.DrawCount() == 0, "Overflowing first index is rejected");
            badDraw = wideDraw;
            badDraw.baseVertex = std::numeric_limits<uint32_t>::max();
            renderer.Draw(geometry, texture, badDraw);
            Check(renderer.DrawCount() == 0, "Overflowing base vertex is rejected");
            backend.EndFrame();
            wide.indices = {0, 1, 2};
            Check(!renderer.UploadGeometry(wide), "Ambiguous simultaneous index streams are rejected");
            wide.indices.clear();
            wide.indices32.back() = std::numeric_limits<uint32_t>::max();
            Check(!renderer.UploadGeometry(wide), "Invalid uint32 index is rejected before upload");
            renderer.ReleaseBindings();
            geometry.reset(); legacyGeometry.reset(); texture.reset();
            Check(renderer.LiveGeometryCount() == 0 && renderer.LiveTextureCount() == 0, "Static resources released");
        }
        backend.Shutdown();
        DestroyWindow(window);
        std::cout << "PASS: uint16/uint32 static and rigid-actor pixels, base offsets, invalid ranges, OPAQUE/MASK/BLEND/factors/culling, lifetime\n";
        return 0;
    } catch (const std::exception& error) {
        if (window) DestroyWindow(window);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
