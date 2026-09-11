#pragma once

#include "StartupOptions.h"

namespace Renderer
{
// Clear/present only: called before Python, game resources and the legacy device exist.
int RunRendererBootstrap(void* instance, const StartupOptions& options);
}
