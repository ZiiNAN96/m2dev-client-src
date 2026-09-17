#include "WinHPreface.h"
#include <Windows.h>
#include <shellapi.h>
#include <d3d11shader.h>
#include <atlcomcli.h>
#include "WinHPostface.h"
#include "ShaderBytecodeCache.h"
#include "ShaderLoadAudit.h"
#include "ShaderCacheBuildIdentity.h"
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>

namespace ShaderBytecodeCache {
namespace {
using Diligent::XXH128Hash;
using Diligent::XXH128State;
namespace fs = std::filesystem;
constexpr unsigned FormatVersion = 1;
constexpr unsigned CacheVersion = 1; // Bump for changes to cache/compiler semantics.
constexpr std::size_t HeaderSize = 56;
constexpr std::size_t MaxBytecodeSize = 16 * 1024 * 1024;
constexpr char Magic[8] = {'M','2','D','X','B','C','0','1'};

class File {
    HANDLE handle_;
public:
    explicit File(HANDLE handle) : handle_(handle) {}
    ~File() { Close(); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    operator HANDLE() const { return handle_; }
    explicit operator bool() const { return handle_ != INVALID_HANDLE_VALUE; }
    bool Close() {
        if (!*this) return true;
        const auto handle = handle_; handle_ = INVALID_HANDLE_VALUE;
        return CloseHandle(handle) != FALSE;
    }
};

XXH128Hash Hash(const void* data, std::size_t size) {
    XXH128State hash; hash.UpdateRaw(data, size); return hash.Digest();
}
bool Equal(XXH128Hash a, XXH128Hash b) {
    return a.LowPart == b.LowPart && a.HighPart == b.HighPart;
}
std::string Hex(XXH128Hash hash) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash.HighPart
        << std::setw(16) << hash.LowPart;
    return out.str();
}
void Put(unsigned char* out, std::uint64_t value, unsigned size) {
    for (unsigned i = 0; i < size; ++i) out[i] = static_cast<unsigned char>(value >> (i * 8));
}
std::uint64_t Get(const unsigned char* in, unsigned size) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < size; ++i) value |= std::uint64_t(in[i]) << (i * 8);
    return value;
}
bool Read(HANDLE file, void* data, std::size_t size) {
    DWORD actual = 0;
    return ReadFile(file, data, static_cast<DWORD>(size), &actual, nullptr) && actual == size;
}
bool Write(HANDLE file, const void* data, std::size_t size) {
    DWORD actual = 0;
    return WriteFile(file, data, static_cast<DWORD>(size), &actual, nullptr) && actual == size;
}
std::wstring Environment(const wchar_t* name) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) return {};
    std::wstring result(size, L'\0');
    const DWORD length = GetEnvironmentVariableW(name, result.data(), size);
    if (length == 0 || length >= size) return {};
    result.resize(length); return result;
}
fs::path Directory() {
    // An elevated client does not necessarily inherit the launcher's environment.
    // This explicit diagnostic path also survives the normal Windows launch flow.
    static const std::wstring commandLinePath = [] {
        std::wstring result;
        int count = 0;
        auto** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!arguments) return result;
        const std::wstring prefix = L"--shader-cache-dir=";
        for (int i = 1; i < count; ++i) {
            const std::wstring_view argument = arguments[i];
            if (argument.substr(0, prefix.size()) == prefix) result = argument.substr(prefix.size());
        }
        LocalFree(arguments); return result;
    }();
    auto overridePath = commandLinePath.empty() ? Environment(L"M2_SHADER_CACHE_DIR") : commandLinePath;
    fs::path path = overridePath.empty()
        ? fs::path(Environment(L"LOCALAPPDATA")) / L"ZiiNAN" / L"m2dev-client" / L"cache" / L"shaders"
        : fs::path(overridePath);
    // Do not accidentally write into the source/current directory if the user
    // cache location cannot be resolved. Relative diagnostic overrides bypass.
    return path.is_absolute() ? path : fs::path{};
}

struct CompilerIdentity { bool valid = false; XXH128Hash hash{}; };
const CompilerIdentity& Compiler() {
    static const CompilerIdentity identity = [] {
        CompilerIdentity result;
        try {
            HMODULE module = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&D3DCompile), &module)) return result;
            std::wstring path(32768, L'\0');
            const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
            if (!length || length >= path.size()) return result;
            path.resize(length);
            File file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            LARGE_INTEGER size{};
            if (!file || !GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 64 * 1024 * 1024) return result;
            std::vector<unsigned char> bytes(static_cast<std::size_t>(size.QuadPart));
            if (!Read(file, bytes.data(), bytes.size())) return result;
            result.hash = Hash(bytes.data(), bytes.size()); result.valid = true;
        } catch (...) { /* Cache identity failure must never prevent compilation. */ }
        return result;
    }();
    return identity;
}

XXH128Hash Key(const char* source, std::size_t size, const Diligent::ShaderCreateInfo& shader,
              const char* profile, unsigned flags, const D3D_SHADER_MACRO* macros) {
    XXH128State hash;
    hash.Update(FormatVersion, CacheVersion, "D3D11/FXC", M2_SHADER_CACHE_CORE_ID,
                Compiler().hash.LowPart, Compiler().hash.HighPart, shader, profile, flags);
    // Includes are covered by Diligent's ShaderCreateInfo hasher. Hash the
    // assembled compiler source too, including generated stage/backend preamble.
    hash.Update(size); hash.UpdateRaw(source, size);
    for (auto* macro = macros; macro && macro->Name; ++macro) {
        const std::string name = macro->Name;
        const std::string value = macro->Definition ? macro->Definition : "";
        hash.Update(name.size(), name, value.size(), value);
    }
    return hash.Digest();
}

bool ValidBytecode(const void* data, std::size_t size, const char* profile) {
    if (size < 32 || size > MaxBytecodeSize || std::memcmp(data, "DXBC", 4) != 0) return false;
    const auto* bytes = static_cast<const unsigned char*>(data);
    if (Get(bytes + 24, 4) != size) return false;
    const auto chunks = Get(bytes + 28, 4);
    if (chunks > (size - 32) / 4) return false;
    for (std::uint64_t i = 0; i < chunks; ++i) {
        const auto offset = Get(bytes + 32 + i * 4, 4);
        if (offset < 32 + chunks * 4 || offset > size - 8) return false;
        if (Get(bytes + offset + 4, 4) > size - offset - 8) return false;
    }
    CComPtr<ID3D11ShaderReflection> reflection;
    if (FAILED(D3DReflect(data, size, __uuidof(ID3D11ShaderReflection), reinterpret_cast<void**>(&reflection)))) return false;
    D3D11_SHADER_DESC desc{};
    if (FAILED(reflection->GetDesc(&desc)) || std::strlen(profile) < 6) return false;
    const char* prefixes[] = {"ps", "vs", "gs", "hs", "ds", "cs"};
    const auto type = D3D11_SHVER_GET_TYPE(desc.Version);
    return type < 6 && std::strncmp(profile, prefixes[type], 2) == 0 &&
        D3D11_SHVER_GET_MAJOR(desc.Version) == static_cast<unsigned>(profile[3] - '0') &&
        D3D11_SHVER_GET_MINOR(desc.Version) == static_cast<unsigned>(profile[5] - '0');
}

bool Load(const fs::path& path, XXH128Hash key, unsigned stage, const char* profile,
          ID3DBlob** output, const std::string& name) {
    File file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) return false;
    LARGE_INTEGER size{};
    std::array<unsigned char, HeaderSize> header{};
    auto invalid = [&] { MapLoadTrace::Count("shader-cache-invalid", name); return false; };
    if (!GetFileSizeEx(file, &size) || size.QuadPart < HeaderSize || size.QuadPart > HeaderSize + MaxBytecodeSize ||
        !Read(file, header.data(), header.size())) return invalid();
    const auto bytes = Get(header.data() + 32, 8);
    if (std::memcmp(header.data(), Magic, 8) != 0 || Get(header.data() + 8, 4) != FormatVersion ||
        Get(header.data() + 12, 4) != stage || Get(header.data() + 16, 8) != key.LowPart ||
        Get(header.data() + 24, 8) != key.HighPart || bytes == 0 || bytes != size.QuadPart - HeaderSize) return invalid();
    CComPtr<ID3DBlob> blob;
    if (FAILED(D3DCreateBlob(static_cast<SIZE_T>(bytes), &blob))) return false;
    if (!Read(file, blob->GetBufferPointer(), static_cast<std::size_t>(bytes))) return invalid();
    const auto checksum = Hash(blob->GetBufferPointer(), blob->GetBufferSize());
    if (!Equal(checksum, {Get(header.data() + 40, 8), Get(header.data() + 48, 8)}) ||
        !ValidBytecode(blob->GetBufferPointer(), blob->GetBufferSize(), profile)) return invalid();
    *output = blob.Detach(); return true;
}

bool Store(const fs::path& path, XXH128Hash key, unsigned stage, ID3DBlob* blob) {
    std::array<unsigned char, HeaderSize> header{};
    std::memcpy(header.data(), Magic, 8);
    Put(header.data() + 8, FormatVersion, 4); Put(header.data() + 12, stage, 4);
    Put(header.data() + 16, key.LowPart, 8); Put(header.data() + 24, key.HighPart, 8);
    Put(header.data() + 32, blob->GetBufferSize(), 8);
    const auto checksum = Hash(blob->GetBufferPointer(), blob->GetBufferSize());
    Put(header.data() + 40, checksum.LowPart, 8); Put(header.data() + 48, checksum.HighPart, 8);
    fs::create_directories(path.parent_path());
    static std::atomic<std::uint64_t> sequence{0};
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        fs::path temporary = path;
        temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(++sequence) + L".tmp";
        File file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file) {
            if (GetLastError() == ERROR_FILE_EXISTS) continue;
            return false;
        }
        bool written = Write(file, header.data(), header.size()) &&
            Write(file, blob->GetBufferPointer(), blob->GetBufferSize()) && FlushFileBuffers(file);
        written = file.Close() && written;
        if (written && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
        DeleteFileW(temporary.c_str()); // Only the unique temporary file we created.
        return false;
    }
    return false;
}
void BytecodeEvidence(const std::string& name, XXH128Hash key, ID3DBlob* blob) {
    if (!MapLoadTrace::state.active) return;
    MapLoadTrace::Count("shader-cache-bytecode", name + " | key=" + Hex(key) +
        " | dxbc=" + Hex(Hash(blob->GetBufferPointer(), blob->GetBufferSize())), blob->GetBufferSize());
}
} // namespace

HRESULT Compile(const char* source, std::size_t sourceLength,
                const Diligent::ShaderCreateInfo& shader, const char* profile,
                unsigned flags, const D3D_SHADER_MACRO* macros, ID3DInclude* includes,
                ID3DBlob** bytecode, ID3DBlob** errors) {
    const auto name = ShaderLoadAudit::ShaderName(shader);
    MapLoadTrace::Count("shader-cache-request", name);
    fs::path path;
    XXH128Hash key{};
    try {
        ShaderLoadAudit::Scope timing("shader-cache-key", name);
        const auto directory = Directory();
        if (MapLoadTrace::state.active) MapLoadTrace::Count("shader-cache-directory", directory.u8string());
        if (!directory.empty() && Compiler().valid) {
            key = Key(source, sourceLength, shader, profile, flags, macros);
            path = directory / (Hex(key) + ".shadercache");
        }
    } catch (...) { path.clear(); }
    try {
        ShaderLoadAudit::Scope timing("shader-cache-read", name);
        CComPtr<ID3DBlob> cached;
        if (!path.empty() && Load(path, key, unsigned(shader.Desc.ShaderType), profile, &cached, name)) {
            MapLoadTrace::Count("shader-cache-hit", name, cached->GetBufferSize());
            BytecodeEvidence(name, key, cached);
            *bytecode = cached.Detach();
            return S_OK;
        }
    } catch (...) { /* Treat inaccessible or malformed cache data as a miss. */ }
    MapLoadTrace::Count("shader-cache-miss", name);
    HRESULT result;
    {
        ShaderLoadAudit::Scope timing("shader-compilation", name);
        MapLoadTrace::Count("shader-runtime-compile", name);
        // This is exactly the original FXC call, with the original inputs/flags.
        result = D3DCompile(source, sourceLength, nullptr, macros, includes, shader.EntryPoint,
                            profile, flags, 0, bytecode, errors);
    }
    if (SUCCEEDED(result) && *bytecode) {
        try {
            ShaderLoadAudit::Scope timing("shader-cache-write", name);
            if (!path.empty() && ValidBytecode((*bytecode)->GetBufferPointer(), (*bytecode)->GetBufferSize(), profile)) {
                if (Store(path, key, unsigned(shader.Desc.ShaderType), *bytecode)) MapLoadTrace::Count("shader-cache-store", name);
                else MapLoadTrace::Count("shader-cache-write-failure", name);
                BytecodeEvidence(name, key, *bytecode);
            }
        } catch (...) { MapLoadTrace::Count("shader-cache-write-failure", name); }
    }
    return result;
}
} // namespace ShaderBytecodeCache
