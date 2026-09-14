#pragma once

#include "AssetRuntime/AssetRuntime.h"
#include "AnimationRuntime/AnimationRuntime.h"
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace AssetRuntime::GrannyAnimationAdapter
{
struct CurveInfo
{
    std::string bone;
    int dimension{}, degree{}, format{}, knotCount{};
    bool keyframed{}, identity{};
};

struct ImportedAnimation
{
    // Index bits: 1 = previous-loop neighborhood, 2 = next-loop neighborhood.
    std::array<std::shared_ptr<const AnimationRuntime::RuntimeAnimationClip>, 4> boundaryClips;
    std::shared_ptr<const AnimationRuntime::RuntimeAnimationClip> clip;
    std::vector<CurveInfo> curves;
    std::vector<std::string> ignoredSourceTracks; // Explicit binding warnings, e.g. a reduced body LOD.
    int groupFlags{};
    int animationFlags{}, defaultLoopCount{};
    float sourceTimeStep{};
    std::array<float, 3> loopTranslation{};
    std::size_t importSamples{}, sampledBoneTransforms{}, storedKeys{}, keyBytes{};
    std::uint64_t importMicroseconds{};
};

// Cheap skeleton ownership stays with the model document. Expensive clip data
// is retained in the private, content-keyed process cache across document loads.
// Access remains on the existing asset/update thread.
struct RuntimeImportCache
{
    static constexpr std::size_t ModelKeyByteLimit = 128 * 1024 * 1024;
    static constexpr std::size_t DocumentKeyByteLimit = ModelKeyByteLimit;
    static constexpr std::size_t ProcessKeyByteLimit = 512 * 1024 * 1024;
    std::vector<std::shared_ptr<const AnimationRuntime::RuntimeSkeleton>> skeletons;
    std::optional<std::array<std::uint8_t, 32>> modelFingerprint;
    RuntimeImportCache() = default;
    RuntimeImportCache(const RuntimeImportCache&) = delete;
    RuntimeImportCache& operator=(const RuntimeImportCache&) = delete;
    ~RuntimeImportCache() = default;
    std::size_t RetainedKeyBytes(std::size_t modelIndex) const;
    std::shared_ptr<const ImportedAnimation> Find(const AnimationHandle& animation,
        std::size_t modelIndex, std::uint64_t bindingId);
    void Retain(const ModelHandle& model, const AnimationHandle& animation,
        std::uint64_t bindingId, std::shared_ptr<const ImportedAnimation> runtime);
};

// Releases only neutral cached data. Call after actor/document teardown and
// before the shutdown owner audit; active instances are never invalidated.
void ClearImportCache() noexcept;

std::shared_ptr<const AnimationRuntime::RuntimeSkeleton> ImportSkeleton(
    const ModelHandle& model, std::string& error);
std::shared_ptr<const ImportedAnimation> ImportAnimation(const ModelHandle& model,
    const AnimationHandle& animation,
    std::shared_ptr<const AnimationRuntime::RuntimeSkeleton> skeleton, std::string& error);
}
