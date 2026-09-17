// Focused production-path checks. No GPU, network or game assets required.
#include <Windows.h>
#include "Renderer/ShaderLoadAudit.h"
#include "Graphics/GraphicsEngineD3DBase/include/ShaderD3DBase.hpp"
#include "Graphics/GraphicsEngine/include/DefaultShaderSourceStreamFactory.h"
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace Diligent;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::uint64_t Count(const std::string& kind) {
    std::uint64_t value = 0;
    for (const auto& [key, event] : MapLoadTrace::state.events)
        if (key.rfind(kind + "\t", 0) == 0) value += event.count;
    return value;
}
struct Result { std::vector<unsigned char> bytes; std::string key; };
Result Compile(const ShaderCreateInfo& shader, bool hit, unsigned invalid = 0) {
    MapLoadTrace::state = {}; MapLoadTrace::state.active = true;
    const auto blob = CompileD3DBytecode(shader, {5, 0}, nullptr, nullptr);
    Require(bool(blob), "CompileD3DBytecode returned null");
    Require(Count("shader-cache-request") == 1, "request count");
    Require(Count("shader-cache-hit") == unsigned(hit), "hit count");
    Require(Count("shader-cache-miss") == unsigned(!hit), "miss count");
    Require(Count("shader-runtime-compile") == unsigned(!hit), "runtime compiler must only run on miss");
    Require(Count("shader-cache-invalid") == invalid, "invalid count");
    Result result;
    const auto* bytes = static_cast<const unsigned char*>(blob->GetConstDataPtr());
    result.bytes.assign(bytes, bytes + blob->GetSize());
    for (const auto& [key, event] : MapLoadTrace::state.events) {
        if (key.rfind("shader-cache-bytecode\t", 0) != 0) continue;
        const auto start = key.find(" | key=");
        Require(start != std::string::npos, "missing cache key");
        result.key = key.substr(start + 7, 32);
    }
    return result;
}
std::vector<unsigned char> Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
void Write(const fs::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    Require(bool(file), "fixture write failed");
}
int main(int argc, char** argv) try {
    Require(argc == 2, "isolated output directory argument required");
    const auto root = fs::absolute(fs::path(argv[1]) / (std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64())));
    Require(fs::create_directories(root), "fresh directory required");
    const auto cache = root / "cache";
    Require(SetEnvironmentVariableW(L"M2_SHADER_CACHE_DIR", cache.c_str()), "cache override failed");
    const std::string source = "#include \"cache_test.fxh\"\n"
        "float4 Main(float4 p:POSITION):SV_POSITION{return p * Value * FACTOR;}\n"
        "float4 Other(float4 p:POSITION):SV_POSITION{return p + Value;}\n";
    auto includeFile = root / "cache_test.fxh";
    std::ofstream(includeFile) << "static const float Value = 1.0;\n";
    RefCntAutoPtr<IShaderSourceInputStreamFactory> factory;
    CreateDefaultShaderSourceStreamFactory(root.string().c_str(), &factory);
    Require(bool(factory), "include factory");
    ShaderMacro macro[] = {{"FACTOR", "1"}};
    ShaderCreateInfo shader;
    shader.Desc.Name = "cache-test"; shader.Desc.ShaderType = SHADER_TYPE_VERTEX;
    shader.Source = source.c_str(); shader.EntryPoint = "Main";
    shader.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    shader.Macros = {macro, 1}; shader.pShaderSourceStreamFactory = factory;
    const auto first = Compile(shader, false);
    Require(!first.key.empty(), "cache write evidence");
    Require(Compile(shader, true).bytes == first.bytes, "cached/fresh bytecode differs");
    auto unaffected = shader; unaffected.EntryPoint = "Other";
    const auto second = Compile(unaffected, false);

    // Exactly one relevant input of Main changes; Other stays a hit.
    ShaderMacro changedMacro[] = {{"FACTOR", "2"}};
    auto changed = shader; changed.Macros = {changedMacro, 1};
    const auto changedResult = Compile(changed, false);
    Require(changedResult.key != first.key && changedResult.bytes != first.bytes, "macro invalidation");
    Require(Compile(unaffected, true).bytes == second.bytes, "unaffected input stopped hitting");
    Require(Compile(shader, true).bytes == first.bytes, "restoring original input did not hit");
    std::cout << "INVALIDATION single macro: MISS; unchanged shader: HIT; restored shader: HIT\n";

    std::ofstream(includeFile) << "static const float Value = 2.0;\n";
    Require(Compile(shader, false).key != first.key, "include content invalidation");
    std::ofstream(includeFile) << "static const float Value = 1.0;\n";
    Require(Compile(shader, true).bytes == first.bytes, "include restore");
    const auto changedSource = source + "\n// cache source invalidation\n";
    changed = shader; changed.Source = changedSource.c_str();
    Require(Compile(changed, false).key != first.key, "source content invalidation");
    changed = shader; changed.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;
    Require(Compile(changed, false).key != first.key, "effective flags invalidation");
    Require(first.key != second.key, "entry point invalidation");

    const auto entry = cache / (first.key + ".shadercache");
    const auto original = Read(entry);
    Require(original.size() == 56 + first.bytes.size(), "cache layout");
    for (const auto field : {0u, 8u, 12u, 16u, 32u, 40u, 56u}) {
        auto corrupt = original; corrupt[field] ^= 0x80;
        Write(entry, corrupt);
        Require(Compile(shader, false, 1).bytes == first.bytes, "corruption fallback changed bytecode");
        Require(Compile(shader, true).bytes == first.bytes, "corrupt entry not repaired");
    }
    Write(entry, {original.begin(), original.begin() + 55});
    Require(Compile(shader, false, 1).bytes == first.bytes, "truncated header fallback");
    Write(entry, {original.begin(), original.end() - 1});
    Require(Compile(shader, false, 1).bytes == first.bytes, "truncated payload fallback");
    // Even a correct outer checksum must not admit malformed DXBC.
    auto corrupt = original; corrupt[56] ^= 1;
    XXH128State hash; hash.UpdateRaw(corrupt.data() + 56, corrupt.size() - 56);
    const auto checksum = hash.Digest();
    for (unsigned i = 0; i < 8; ++i) {
        corrupt[40 + i] = static_cast<unsigned char>(checksum.LowPart >> (8 * i));
        corrupt[48 + i] = static_cast<unsigned char>(checksum.HighPart >> (8 * i));
    }
    Write(entry, corrupt);
    Require(Compile(shader, false, 1).bytes == first.bytes, "DXBC validation fallback");
    std::cout << "CORRUPTION magic/version/stage/key/size/checksum/payload/truncation/DXBC: PASS\n";

    const auto blocked = root / "not-a-directory";
    std::ofstream(blocked) << "blocked cache fixture";
    SetEnvironmentVariableW(L"M2_SHADER_CACHE_DIR", blocked.c_str());
    Require(Compile(shader, false).bytes == first.bytes, "unwritable cache prevented compilation");
    Require(Count("shader-cache-write-failure") == 1, "missing write failure evidence");
    SetEnvironmentVariableW(L"M2_SHADER_CACHE_DIR", cache.c_str());
    Require(Compile(shader, true).bytes == first.bytes, "restore writable cache");

    // Two concurrent cold writers: the result must be one complete valid file.
    const auto concurrentSource = source + "\n// concurrent cold writers\n";
    changed = shader; changed.Source = concurrentSource.c_str();
    auto worker = [changed] {
        MapLoadTrace::state = {};
        const auto blob = CompileD3DBytecode(changed, {5, 0}, nullptr, nullptr);
        return bool(blob);
    };
    auto one = std::async(std::launch::async, worker);
    auto two = std::async(std::launch::async, worker);
    Require(one.get() && two.get(), "concurrent writers failed");
    Require(Compile(changed, true).bytes == first.bytes, "concurrent publication is invalid");
    for (const auto& file : fs::directory_iterator(cache))
        Require(file.path().extension() != ".tmp", "unfinished temporary file");
    MapLoadTrace::state = {};
    std::cout << "SOURCE/INCLUDES/ENTRY/FLAGS, UNWRITABLE, ATOMIC CONCURRENT WRITE, BYTECODE: PASS\n"
              << "EVIDENCE " << root.string() << "\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n'; return 1;
}
