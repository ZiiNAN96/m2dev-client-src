#include "StdAfx.h"
#include "EterBase/MapLoadTrace.h"
#include "Eterbase/Debug.h"
#include "Thing.h"
#include "ThingInstance.h"
#include "AssetRuntime/Providers.h"

CGraphicThing::CGraphicThing(const char* fileName) : CResource(fileName)
{
    Initialize();
}

CGraphicThing::~CGraphicThing()
{
    Clear();
}

void CGraphicThing::Initialize()
{
    m_asset = {};
    m_models = nullptr;
    m_motions = nullptr;
}

void CGraphicThing::OnClear()
{
    // ZiiNAN: Asset Runtime boundary - release users before their document owner.
    delete[] m_motions;
    delete[] m_models;
    Initialize();
}

CGraphicThing::TType CGraphicThing::Type()
{
    static TType type = StringToType("CGraphicThing");
    return type;
}

bool CGraphicThing::OnIsEmpty() const
{
    return !m_asset;
}

bool CGraphicThing::OnIsType(TType type)
{
    return type == CGraphicThing::Type() || CResource::OnIsType(type);
}

bool CGraphicThing::CreateDeviceObjects()
{
    for (int i = 0; i < GetModelCount(); ++i)
        if (!m_models[i].CreateDeviceObjects()) return false;
    return true;
}

void CGraphicThing::DestroyDeviceObjects()
{
    for (int i = 0; i < GetModelCount(); ++i)
        m_models[i].DestroyDeviceObjects();
}

bool CGraphicThing::CheckModelIndex(int index) const
{
    return index >= 0 && index < GetModelCount();
}

bool CGraphicThing::CheckMotionIndex(int index) const
{
    return index >= 0 && index < GetMotionCount();
}

CGrannyModel* CGraphicThing::GetModelPointer(int index)
{
    return CheckModelIndex(index) && m_models ? m_models + index : nullptr;
}

CGrannyMotion* CGraphicThing::GetMotionPointer(int index)
{
    return CheckMotionIndex(index) && m_motions ? m_motions + index : nullptr;
}

int CGraphicThing::GetModelCount() const
{
    return static_cast<int>(m_asset.ModelCount());
}

int CGraphicThing::GetMotionCount() const
{
    int count = 0;
    for (std::size_t i = 0; i < m_asset.AnimationCount(); ++i)
        if (!m_asset.Animation(i).Get()->metadataOnly) ++count;
    return count;
}

bool CGraphicThing::OnLoad(int size, const void* bytes)
{
    MapLoadTrace::Scope p0lScope("Assets","model adapters","cpu");
    MapLoadTrace::Count("model-adapter",GetFileName());

    if (!bytes || size <= 0) return false;
    auto loaded = AssetRuntime::LoadModel(GetFileNameString(),
        {static_cast<const std::byte*>(bytes), static_cast<size_t>(size)});
    if (!loaded) {
        TraceError("Asset Runtime load failed: %s error=%s detail=%s", GetFileName(),
            AssetRuntime::ErrorName(loaded.error), loaded.diagnostic.c_str());
        return false;
    }
    m_asset = std::move(loaded.asset);
    if (!LoadModels() || !LoadMotions()) {
        TraceError("Asset Runtime legacy adapter preparation failed: %s", GetFileName());
        OnClear();
        return false;
    }
    m_asset.ReleaseUploadData();
    return true;
}

// Existing resource manager and local-texture path convention remain authoritative.
static std::string gs_modelLocalPath;
const std::string& GetModelLocalPath()
{
    return gs_modelLocalPath;
}

bool CGraphicThing::LoadModels()
{
    assert(m_asset && !m_models);
    const auto& fileName = GetFileNameString();
    if (fileName.length() > 2 && fileName[1] != ':') {
        const auto separator = fileName.rfind('\\');
        gs_modelLocalPath.assign(fileName, 0,
            separator == std::string::npos ? 0 : separator + 1);
    }
    const int count = GetModelCount();
    if (!count) return true; // Animation-only documents are valid.
    m_models = new CGrannyModel[count];
    for (int i = 0; i < count; ++i) {
        auto& model = m_models[i];
        if (!model.CreateFromAsset(m_asset.Model(i))) return false;
        if (Renderer::staticObjectLoadDepth && !GetMotionCount() && !model.CaptureStaticObjectSource()) return false;
        if (!model.CaptureActorSource(m_actorAttachment)) return false;
    }
    return true;
}

bool CGraphicThing::LoadMotions()
{
    assert(m_asset && !m_motions);
    const int count = GetMotionCount();
    if (!count) return true;
    m_motions = new CGrannyMotion[count];
    int output = 0;
    for (std::size_t i = 0; i < m_asset.AnimationCount(); ++i) {
        const auto clip = m_asset.Animation(i);
        if (!clip.Get()->metadataOnly && !m_motions[output++].BindAsset(clip)) return false;
    }
    return true;
}
