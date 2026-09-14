#pragma once

#include <string>

struct ANativeActivity;
struct AAssetManager;

namespace Platform::Android
{
struct AppStorage
{
    AAssetManager* readOnlyAssets = nullptr;
    std::string writableDirectory;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return readOnlyAssets != nullptr && !writableDirectory.empty() && writableDirectory.front() == '/';
    }
};

[[nodiscard]] AppStorage GetAppStorage(const ANativeActivity& activity);
}
