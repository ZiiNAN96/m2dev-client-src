#include "Platform/Android/AndroidLifecycle.h"
#include "Platform/TouchInput.h"

#include <iostream>

int main()
{
    Platform::Android::Lifecycle life;
    int failures = 0;
    const auto expect = [&](bool expected, const char* phase) {
        if (life.CanRender() != expected) { std::cerr << phase << '\n'; ++failures; }
    };
    expect(false, "created without surface");
    life.SetResumed(true);
    life.SetFocused(true);
    life.SetSurfaceSize(1920, 1080);
    expect(false, "surface without renderer");
    life.SetRendererReady(true);
    expect(true, "first frame allowed");
    life.SetResumed(false);
    expect(false, "pause must stop presenting");
    life.SetResumed(true);
    expect(true, "resume existing surface");
    life.SetFocused(false);
    expect(false, "focus loss must stop presenting");
    life.SetFocused(true);
    life.SetSurfaceSize(0, 1080);
    expect(false, "zero-width surface");
    life.SetSurfaceSize(1080, 0);
    expect(false, "zero-height surface");
    life.SetSurfaceSize(1080, 1920);
    expect(true, "orientation resize");
    life.DestroySurface();
    expect(false, "surface destroyed");
    life.SetSurfaceSize(1920, 1080);
    expect(false, "replacement surface needs a new renderer");
    life.SetRendererReady(true);
    expect(true, "replacement surface ready");
    life.Destroy();
    expect(false, "application destroyed");
    life.SetResumed(true);
    life.SetFocused(true);
    life.SetSurfaceSize(1920, 1080);
    life.SetRendererReady(true);
    expect(false, "late callbacks cannot resurrect destroyed application");
    life.DestroySurface();
    life.Destroy();
    expect(false, "idempotent shutdown");
    std::cout << "Android lifecycle host contract: " << (failures ? "FAIL" : "PASS") << '\n';
    return failures ? 1 : 0;
}
