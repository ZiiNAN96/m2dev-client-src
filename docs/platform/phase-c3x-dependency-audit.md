# C3-X dependency and Android bootstrap audit

Audit date: 2026-09-14. This audit describes the existing repository and pinned
Diligent source. Android SDK/NDK and device execution are unavailable in this
environment. “Portable candidate” below means source and platform branches exist;
it does **not** mean that the dependency was built or tested for Android.

## Dependency matrix

| Dependency | Current Windows x64 form | Source available here? | Android ARM64 / portability | Blocker and severity | Future work / phase |
|---|---|---|---|---|---|
| Granny 2.11.8.0 header/API | `granny2_static.lib`, static imported library | Public API headers and client wrappers; no Granny runtime implementation | Existing binary is x64 COFF. Header contains an Android ARM64 branch, but no matching runtime or build | High: full actor/animation/model client cannot link | Resolve licensed ARM64 runtime/source feasibility or a separately authorized replacement; Phase D/F planning |
| SpeedTreeRT header release 1.6.0 | `speedtree_static.lib` and `speedtree_staticd.lib` | Public API header and client wrappers; no proprietary runtime implementation | Existing binaries are x64 COFF. Header has historical static/dynamic and endian branches; this does not prove ARM64 runtime support | High: existing tree runtime cannot link | Inventory licensed runtime/source options later; no replacement in C3-X |
| CPython headers 3.14.3 | `python314_static.lib`, `pathcch`, `bcrypt`; Windows `pyconfig.h` | CPython API/internal headers and game extension modules; no CPython runtime source tree | Upstream 3.14 supports Android embedding; this local static library/configuration is Windows-specific | High: full script/UI client cannot link | Build/package matching ARM64 Python, regenerate configuration and compile extensions later; no upgrade |
| DiligentCore v2.5.6 | Pinned source, production D3D11 static target | Yes, cached exact Git revision | Same revision has explicit Android platform and Vulkan static target | Environment: NDK unavailable; integration uncompiled | Compile/link isolated Vulkan app when existing/provisioned toolchain is available; C3-X validation follow-up |
| LZO 2.10 | Built from bundled C source, `lzo2` | Yes | Portable candidate; CMake already uses PIC | Low integration risk, unverified NDK build | Build only when pack runtime enters scope; Phase D |
| zstd 1.5.7 | Bundled source, `libzstd_static` | Yes | Portable candidate with upstream CMake | Low integration risk, unverified NDK build | NDK build with future pack runtime; Phase D |
| libsodium 1.0.20 wrapper | Bundled C source, `sodium` | Yes | ARM64/Android CPU branches exist; wrapper has Windows/non-Windows definitions | Medium: target feature detection and cryptography need real NDK validation | Preserve protocol/cipher; build integration in later Android networking/assets work |
| mio 1.1.0 | Header-oriented mapped-file library | Yes | Windows and POSIX branches exist | Medium: APK assets are not ordinary host files; storage integration needed | Decide read-only asset/package mapping separately; Phase D |
| FreeType 2.13.3 | Bundled source, optional dependencies disabled | Yes | Portable source candidate | Low dependency risk, but text/UI runtime not ported | Validate with later Android UI work |
| WebView2 | Imported `WebView2LoaderStatic.lib`, Windows COM integration | Headers and own Windows integration only; no browser runtime source | Windows loader binary cannot be reused | High for web feature; irrelevant to clear-only app | Android web presentation boundary later, no replacement now |
| Windows video | Own `Win32MoviePlayer`, DirectShow/COM and Windows codec interfaces | Own integration source; OS components external | Windows API implementation cannot compile for Android | High for video feature; irrelevant to bootstrap | Define Android media implementation later |
| Audio / miniaudio 0.11.22 | Bundled `miniaudio.c/.h`, own `AudioLib` | Yes | Source includes AAudio and OpenSL ES Android backends | Medium: app pause/audio focus, assets and own wrapper dependencies remain | Later audio integration; no sound engine in bootstrap |
| Networking | Own stream/protocol code, WinSock service implementation | Yes | Existing platform API only covers a subset of socket operations | High for full client connection; no network dependency in app | Add POSIX socket boundary later without protocol rewrite |
| DirectInput, shell, registry, crash diagnostics | Windows SDK plus own implementations | Own wrappers and SDK headers | Windows implementations deliberately excluded | Feature-specific; no bootstrap blocker | Separate future Android services as needed |

All rows except Diligent and the small Platform implementation are excluded from
the Android target. No dependency was upgraded, replaced or ported during this audit.

## Binary and version evidence

`dumpbin /headers` examined all five imported library files: Granny, SpeedTree
Release and Debug, Python and WebView2. The combined output contained 482 x64 COFF
machine records (`8664 machine (x64)`), and no other machine records. Each requested
archive appeared in the dump. This confirms existing library architecture, not
their Android compatibility. The header version of a proprietary library is not
proof of the precise binary build version; no proprietary runtime version probe
was performed for this audit.

Source references (repository-relative paths and audited line numbers):

- Granny product string: `extern/include/granny.h:1254`; Linux/Android ARM64
  pointer selection: `extern/include/granny.h:211-239`; imported library:
  `extern/library/Granny/CMakeLists.txt:1-6`.
- SpeedTree header release: `extern/include/SpeedTreeRT.h:21`; dynamic Windows
  export/import macros: lines 26-34; endian selection: lines 37-41. Imported
  Release/Debug binaries: `extern/library/SpeedTree/CMakeLists.txt:7-11`.
- Python 3.14.3: `extern/include/python/patchlevel.h:20-27`. Windows build
  configuration: `extern/include/python/pyconfig.h:67-68`, `:351-352`, `:393-395`.
  Static library plus OS link dependencies: `extern/library/Python/CMakeLists.txt:1-7`.
- miniaudio 0.11.22: `extern/include/miniaudio.h:3751-3754`; Android selection
  around `:3905`; AAudio/OpenSL capabilities around `:6606-6659`.
- Dependency additions and exact versioned source directories:
  `vendor/CMakeLists.txt:1-16`; LZO version and PIC:
  `vendor/lzo-2.10/CMakeLists.txt:3`, `:23`; zstd macros:
  `vendor/zstd-1.5.7/lib/zstd.h:112-114`; libsodium wrapper version:
  `vendor/libsodium/CMakeLists.txt:22`; mio version:
  `vendor/mio/CMakeLists.txt:21`; FreeType version:
  `vendor/freetype-2.13.3/include/freetype/freetype.h:5175-5177`.
- libsodium ARM64 CPU-feature source:
  `vendor/libsodium/src/libsodium/sodium/runtime.c:67-85`, `:120-136`.
  The wrapper currently generates `sodium/version.h` in its own source tree;
  Android bootstrap does not configure this dependency. A future integration
  should revisit that generated-file location as well as target feature detection.

## Granny and SpeedTree scope

Direct Granny users found under `src` are `EterGrnLib`, `GameLib`, `ScriptLib` and
`UserInterface`. Representative entry points are
`src/EterGrnLib/StdAfx.h:6` and `src/UserInterface/PythonApplication.cpp:859`
(shared deformation buffer creation), `:1071` and `:1085` (shutdown).
The `granny.h` Android branch is only a conditional declaration/configuration
path; the repository has neither a licensed Android binary nor Granny runtime
source. Source portability, license availability and required SDK options cannot
be concluded from this header. Granny remains a central full-client link blocker.

SpeedTree client-side wrappers in `src/SpeedTreeLib` and the Diligent tree renderer
are not the proprietary SpeedTree runtime source. They continue using the existing
Windows library. The historical header supports more than one platform convention,
but no current Android ARM64 runtime build is available here. No changes were made
to tree geometry, materials, rendering or animation behavior.

## Python and extension modules

The existing Python version is **3.14.3**, not Python 2.x. It is statically linked
and configured for Windows. Reusing its `pyconfig.h` would incorrectly assert
`SIZEOF_LONG == 4` on LP64. Both the runtime and generated configuration must match
the target ABI. `src/ScriptLib/PythonUtils.h:85` creates native `PyModuleDef`
modules; `src/PythonModules`, `src/EterPythonLib` and numerous `UserInterface`
modules are game-specific C API consumers, not portable Android modules already
ready for reuse.

The official [Python 3.14 Android guide](https://docs.python.org/3.14/using/android.html)
describes embedded `libpython`, private standard-library/application assets and a
source-build workflow. Thus Android embedding is feasible upstream; this repository's
matching configuration, modules and packaging still require separate work. No
Python upgrade or Android integration is part of C3-X.

## Networking and threading

`src/Platform/PlatformNetworking.h` is pointer-width safe and Windows-header free,
but its interface covers only initialization, shutdown, close, errors, local host
name and IPv4 resolution. `src/Platform/Windows/Win32Networking.h` still converts
the neutral handle to a WinSock `SOCKET`, and `NetStream` uses that implementation:

- `src/EterLib/NetStream.cpp:55` and `:85`: direct `recv` and `send`.
- `:105-121`: `SOCKET`, `FD_SET` and `select(0, ...)`, whose first argument has
  different significance on POSIX.
- `:201-225`: socket creation, `ioctlsocket(FIONBIO)`, `setsockopt` and `connect`.
- `src/Platform/Windows/Win32Networking.cpp:12-35`: WinSock startup/cleanup,
  `closesocket`, `WSAGetLastError` and `WSAEWOULDBLOCK`.

A later POSIX backend needs descriptor conversion and invalid-handle rules,
`errno`, nonblocking `fcntl`, `EAGAIN/EWOULDBLOCK/EINPROGRESS`, close, send/receive,
connection completion and `select(max_fd + 1, ...)` or an equivalent readiness API.
No Android server connection was attempted and no packet/cipher behavior changed.

The old thread helpers use `CRITICAL_SECTION` (`src/EterLib/Mutex.h:15`),
`_beginthreadex`/`HANDLE` (`src/EterLib/Thread.cpp:11`) and Win32 events/thread
creation (`src/UserInterface/ProcessScanner.cpp:129-132`). They are excluded
from Android/common targets. NativeActivity's NDK glue manages the bootstrap
thread; first-party Android code adds no custom thread or Win32 event emulation.

## Diligent and CMake review

Reviewed revision: `b036337d68be2353c9950a85929acf796b9a6d50`, v2.5.6, unchanged
from the Windows setup. In the cached source tree:

- `CMakeLists.txt:193-197` explicitly enables Android Vulkan support.
- `Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h:78-99` declares
  `CreateDeviceAndContextsVk` and `CreateSwapChainVk`.
- `Platforms/Android/interface/AndroidNativeWindow.h:34-37` carries the native
  window as `pAWindow`.
- `Graphics/GraphicsEngineVulkan/CMakeLists.txt:151` creates
  `Diligent-GraphicsEngineVk-static`; the Android branch selects volk and
  `VK_USE_PLATFORM_ANDROID_KHR`.
- `ThirdParty/CMakeLists.txt:22-29`, `:37-43`, `:95-123`, `:158-163` establish
  Vulkan-Headers, SPIRV-Headers, SPIRV-Cross and volk dependencies. xxHash also
  remains required. Even with HLSL/glslang disabled, SPIRV-Cross is still built.
- `BuildTools/CMake/BuildUtils.cmake:175-179` enables PIC for the relevant
  non-Windows static libraries, allowing them to link into the app's shared library.

`buildtool/Diligent.cmake` now selects exactly those submodules for Android and
sets `DILIGENT_NO_HLSL=ON`, `DILIGENT_NO_GLSLANG=ON`. The clear-only app has no
shader compilation requirement; it does not pull SPIRV-Tools or glslang. The
existing Windows branch continues selecting D3D11 with its existing shader options.
Using an old Windows-only populated dependency cache for Android will not magically
provide missing submodules; the fresh Android build must resolve the pinned subset.

The root separates Windows client targets from Android bootstrap targets before
adding `vendor`, `src` gameplay modules or `extern`. No Granny, Python, SpeedTree,
DirectShow, DirectInput, WebView2 or WinSock binary is linked to the Android app.
The app links `M2PlatformAndroid`, `M2RendererVulkanBootstrap`, NDK
`android_native_app_glue`, `android`, `log` and `dl`. `CMAKE_ANDROID_NDK` is the
CMake/NDK-discovered root used to locate the glue source, not a committed local path.
The glue entry point is retained through `-uANativeActivity_onCreate`.

The Gradle project requests `arm64-v8a`, API 26+, shared libc++, NDK r27 flexible
page-size support, and the CMake target `M2AndroidBootstrap`. The manifest's
`m2_android_bootstrap` library name matches CMake's `OUTPUT_NAME`. Source-root
resolution from `android/app/build.gradle` uses `../../CMakeLists.txt` correctly.
Gradle output defaults outside the repository; CMake staging is a sibling with
the suffix `-native`, outside Gradle's cleanable build directory. No wrapper binary,
SDK path or generated APK is versioned. See [Android build instructions](../../android/README.md).

## Verification limit and small host compile

The Android application, NDK headers, Vulkan implementation and Gradle project
were reviewed against their actual API contracts but **not compiled, linked,
packaged or run** here. Android Levels B-E remain unproven.

The platform clock and dynamic-library translation units have no NDK-specific
headers. As an additional bounded check they were compiled individually with
the installed Cygwin GCC 12.4.0 LP64 compiler, C++20 and
`-Wall -Wextra -Werror`: both passed. Their object files are in ignored
`build-c3x/android-time-lp64.o` and `android-dynamic-library-lp64.o`.
This is a real non-MSVC source compile, not an Android binary or `.so` runtime test.
The main C3-X report contains the separate portable ABI/lifecycle and Windows results.
