# C3-X toolchain audit

Audit date: 2026-09-14. This inventory was performed before implementation, without installing software. Evidence is retained locally in the ignored `build-c3x/toolchain-audit.txt` and `build-c3x/cygwin-probe-*.log` files.

## Windows tools

| Component | Observed version | Observed path / result |
|---|---|---|
| CMake on PATH | 4.4.3 | `C:\Program Files\CMake\bin\cmake.exe` |
| Visual Studio | Community 2022 17.14.4; installation 17.14.36202.13 | `C:\Program Files\Microsoft Visual Studio\2022\Community` |
| MSVC x64 | compiler 19.44.35209; toolset directory 14.44.35207 | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe` |
| Windows SDK | 10.0.22621.0 and 10.0.26100.0 | `C:\Program Files (x86)\Windows Kits\10\Include` and `Lib` have both versions |
| Ninja | 1.12.1 | `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`; not on PATH |
| VS-bundled CMake | 3.31.6-msvc6 | `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` |
| clang / clang-cl | Not detected | Absent on PATH, in standard LLVM root, both VS LLVM `bin` locations, and Cygwin `bin` |
| MSVC ARM64 compiler | Not detected | `VC\Tools\MSVC\14.44.35207\bin\Hostx64\arm64\cl.exe` absent; SDK ARM64 libraries are present, but are not an Android compiler |

`vswhere.exe -all -products '*' -format json`, executable version output, and installed SDK directory inventories supplied these values. `cl` is available by its concrete path even though it is not in the current shell's PATH.

## Android tools and device access

| Component | Observed result | Consequence |
|---|---|---|
| Android SDK | Not detected in audited locations | SDK platform, package tools and platform tools unavailable |
| Android NDK | Not detected | No Android ARM64 native compiler/sysroot |
| NDK CMake toolchain | Not detected | No `build/cmake/android.toolchain.cmake` from a real NDK |
| adb | Not on PATH; standard SDK location absent | Connected devices cannot be enumerated through adb |
| Android Studio | Not detected in standard program locations or installed-app entries | No existing Studio toolchain found |
| Java / JDK | `java` and `javac` absent on PATH; standard JDK roots absent | No usable JDK located |
| Gradle | Not on PATH; user `.gradle` absent | No Gradle installation/cache detected |
| Emulator / AVD | `emulator` and `avdmanager` absent on PATH; user `.android` absent | No usable emulator configuration located |

`JAVA_HOME`, `ANDROID_HOME`, `ANDROID_SDK_ROOT`, `ANDROID_NDK_HOME`, and `ANDROID_NDK_ROOT` are empty in process, user and machine environments. The process `VULKAN_SDK` is also empty; an Android NDK, if supplied later, is the relevant source of the Android Vulkan loader/header toolchain.

The audit checked these expected roots without a whole-disk scan:

- `C:\Users\ZiiNAN\AppData\Local\Android`, `C:\Users\ZiiNAN\Android`, `C:\Android`, `C:\android-sdk`, and `C:\android-ndk`.
- `C:\Program Files\Android`, `C:\Program Files (x86)\Android`, `C:\Program Files\Android Studio`, and the exact user-local `Programs\Android Studio` path.
- `C:\ProgramData\Microsoft\AndroidSDK`, the VS `Common7\IDE\Extensions\Xamarin\Android` directory, and user-local `Xamarin\MonoForAndroid`.
- Standard Java, Eclipse Adoptium, Zulu, AdoptOpenJDK and user `.jdks` roots; standard Scoop and Chocolatey Android/JDK package roots.
- Relevant current-user, native-machine and WOW6432 installed-application registry entries.

The only relevant Android registry entry is `Microsoft.NET.Sdk.Android.Manifest-9.0.100 (x64)` version `35.0.61`. A .NET workload manifest is not an installed Android SDK or NDK. `C:\Program Files\JetBrains` exists but contained no product directories. General enumeration of `C:\Users\ZiiNAN\AppData\Local\Programs` was access-denied; the explicit `Android Studio` subpath was tested absent. Nonstandard unconfigured installations cannot be excluded.

**Disposition:** Android configure, native compile, link and packaging are **environment-blocked**. **Device runtime not verified.** No claim is made that a physical device is absent: adb is unavailable, so device enumeration was not possible. No software installation or emulator provisioning was attempted.

## Available non-MSVC LP64 host probe

`C:\cygwin64\bin\gcc.exe` and `g++.exe` are GCC **12.4.0**, target **x86_64-pc-cygwin**. `C:\cygwin64\bin\make.exe` is GNU Make **4.4.1**. Cygwin CMake and clang are absent.

A small, ignored probe was successfully configured and built using Windows CMake, VS Ninja, and Cygwin GCC with these explicit options:

```text
-G Ninja
-DCMAKE_MAKE_PROGRAM=<installed Ninja executable>
-DCMAKE_SYSTEM_NAME=CYGWIN
-DCMAKE_SYSTEM_PROCESSOR=x86_64
-DCMAKE_CXX_COMPILER=C:/cygwin64/bin/g++.exe
```

CTest executed the resulting Cygwin program: **1/1 passed**. Its compile-time assertions and runtime output confirmed `sizeof(void*) = 8`, `sizeof(long) = 8`, `sizeof(int) = 4`, and `sizeof(std::uint32_t) = 4`. Configure, build and test evidence is in `build-c3x/cygwin-probe-configure.log`, `cygwin-probe-build.log`, and `cygwin-probe-test.log`.

This provides a usable non-MSVC LP64 host for actual portable tests. It proves neither Android NDK compilation nor ARM64 execution; those remain separate evidence requirements.

## Real repository portable fast gate

The final repository common configuration was then built with the same CMake/Ninja/GCC toolchain in `build-c3x/cygwin-common`, using:

```text
-DM2_BUILD_WINDOWS_CLIENT=OFF
-DM2_BUILD_RENDERER_TESTS=OFF
-DM2_BUILD_ANDROID_BOOTSTRAP=OFF
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
-DCMAKE_C_COMPILER=C:/cygwin64/bin/gcc.exe
-DCMAKE_CXX_COMPILER=C:/cygwin64/bin/g++.exe
-DCMAKE_SYSTEM_NAME=CYGWIN
-DCMAKE_SYSTEM_PROCESSOR=x86_64
```

Configure, compilation, link and **4/4 CTest cases passed**: `Platform.PortableHeaders`, `Platform.PortableAbi`, `Platform.AndroidLifecycle`, and `Platform.SourceBoundaries`. This includes the production water-height decoder's modern signed-32 and legacy unsigned-16 disk formats, malformed inputs and unaligned input. The ABI executable confirmed `pointer=8 long=8 size_t=8, little-endian`. The final test run took 2.07 seconds while the Windows build was also active.

The compile database and Ninja build graph contain only the three portable test translation units. They contain no MSVC-only `/MP`, `/MD[d]`, or `/permissive` switches, no Win32 backend sources, and none of the audited Windows-only libraries. Evidence: `build-c3x/cygwin-common-{configure,build,test}.log`, `build-c3x/cygwin-common-source-audit.txt`, and the build directory's `compile_commands.json` / `platform-target-audit.txt`.

An initial attempt to reuse the full C2 header test on Cygwin exposed its `DrawStateTypes.h -> Math/Math.h -> DirectXMath.h` dependency. The existing full C2 test remains unchanged in the Windows gate. A separate portable header test covers the actual common platform/native-handle and renderer-data boundary without importing legacy renderer math. Portable renderer math remains future work; no math or skinning algorithm was rewritten for this gate.

`.github/workflows/portable-common.yml` adds a five-minute Linux LP64 GCC/Clang host gate for these common targets. Workflow contents were reviewed locally; GitHub-hosted Linux/Clang execution has **not** been observed in this session. The locally proven non-MSVC result is Cygwin GCC, not a claim of a completed Linux client port.

