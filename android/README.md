# C3-X Android/Vulkan bootstrap

This is an isolated clear/present NativeActivity, not the Metin2 client. It has no login,
assets, gameplay, audio, Python, Granny or SpeedTree dependency. The Android sources,
NDK build and APK are **not yet compile/runtime verified** because this environment
has no Android SDK/NDK, Java or Gradle. No toolchain or emulator was installed.

## Reproducible build prerequisites

- JDK 17, Gradle 8.11.1 and Android Gradle plugin 8.9.2.
- Android platform 35, SDK Build Tools 35.0.0, NDK 27.0.12077973, CMake 3.31.6.
- Android ABI `arm64-v8a`, minimum API 26, Vulkan-capable device.
- Existing DiligentCore v2.5.6 commit stays pinned. The Android CMake branch enables
  its Vulkan target and required submodules; shader compilation is disabled for this clear-only app.

The AGP/Gradle/JDK/SDK/NDK versions follow the official
[AGP 8.9 compatibility table](https://developer.android.com/build/releases/agp-8-9-0-release-notes).
CMake 3.31.6 is the explicit project build tool choice. This project intentionally contains
no downloaded Gradle wrapper JAR, SDK paths, toolchain binaries, signing keys or APK.
Use an existing compatible Gradle installation. Set `ANDROID_HOME` and `JAVA_HOME`
locally, never in versioned files. Locate the selected CMake through `PATH` or the ignored
`local.properties` entry `cmake.dir`, as described in the official
[NDK/CMake configuration guide](https://developer.android.com/studio/projects/install-ndk).
Building may resolve pinned dependencies from their
official repositories; install missing tools separately with the user's authorization.

From this directory after tools are available:

```text
gradle --no-daemon :app:assembleDebug
```

Default output, relative to the repository's parent:
`m2dev-client-builds/c3x/android/app/outputs/apk/debug/app-debug.apk`.
Override the external build root with `-Pm2BuildRoot=<absolute-output-directory>`.
Native CMake intermediates use a sibling directory with the suffix `-native`, outside
Gradle's cleanable build directory. Gradle's project cache is ignored.

```text
adb devices
adb install -r <output-directory>/app/outputs/apk/debug/app-debug.apk
adb shell am start -n com.ziinan.m2bootstrap/android.app.NativeActivity
adb logcat -s M2Bootstrap
```

Expected evidence: platform init, surface available, Vulkan device/context initialized,
swapchain created, first present returned without a reported error, pause/resume,
surface destroy and shutdown. `Present` is a void Diligent API: its log marker alone
does not prove that a frame was visibly displayed. Observe the dark blue clear on the device.
Check Home/return, rotation, Back/exit and activity recreation, with no presents while paused
or after a surface has been destroyed. `am force-stop` is an emergency process stop and
does not prove orderly application cleanup. No device or first present is claimed here.

## Lifecycle and boundaries

The NDK's `android_native_app_glue` owns the NativeActivity callback handoff. The
application uses `ALooper_pollOnce`; there is no Win32 pump. It drains pending events
before each frame and blocks when paused, unfocused, failed or without a usable surface.
The host-testable `AndroidLifecycle.h` drives the real render gate. A retained
`ANativeWindow` outlives the Vulkan swapchain. Surface termination first disables
rendering, cancels touches, idles/releases Vulkan, then releases the retained window.
Surface recreation constructs a new renderer; initialization failure finishes the activity.

Touch down/move/up/cancel are translated to the portable `Platform::TouchEventHandler`
boundary. The bootstrap has no gameplay consumer. This does not implement the existing
keyboard-oriented `PlatformInput` class on Android.

`AndroidFilesystem` exposes the Activity's read-only `AAssetManager` and absolute private
`internalDataPath`. Clear/present needs no files, so no asset loading or pack API is fabricated.
Current working directory and Windows-style paths are never used. `AndroidTime` implements
the existing clock API with a steady clock; Windows timer-period requests explicitly return
false. `AndroidDynamicLibrary` implements `.so` loading through `dlopen`/`dlsym`/`dlclose`;
Android linker namespace restrictions still apply. It is not used by the clear bootstrap.

## Optional validation

Validation is off by default. With an already available compatible validation layer,
build with `-Pm2VulkanValidation=ON`. Diligent warnings/errors go to logcat; the log states
whether validation was requested, not whether a layer actually loaded. Follow Android's
[validation-layer setup](https://developer.android.com/ndk/guides/graphics/validation-layer)
and verify the layer load. This project does not download or package validation binaries.

The architecture uses the official [NativeActivity/native glue model](https://developer.android.com/ndk/guides/concepts)
and Android's [Vulkan API](https://developer.android.com/ndk/guides/graphics/).
