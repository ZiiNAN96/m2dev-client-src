#include "AndroidFilesystem.h"
#include <android/native_activity.h>

namespace Platform::Android
{
AppStorage GetAppStorage(const ANativeActivity& activity)
{
    return {activity.assetManager, activity.internalDataPath ? activity.internalDataPath : ""};
}
}
