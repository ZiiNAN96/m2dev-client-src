#include "Platform/PlatformDynamicLibrary.h"
#include "Platform/PlatformFilesystem.h"
#include "Platform/PlatformNetworking.h"
#include "Platform/PlatformTime.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
class TestContext
{
public:
    void Check(bool condition, const char* description)
    {
        if (condition)
            return;

        std::cerr << "FAILED: " << description << '\n';
        ++m_failures;
    }

    [[nodiscard]] int Result() const noexcept
    {
        return m_failures == 0 ? 0 : 1;
    }

private:
    int m_failures = 0;
};

struct TemporaryTree
{
    std::string seed;
    std::string root;
    std::string nested;
    std::string payload;
    std::string empty;

    ~TemporaryTree()
    {
        if (!payload.empty())
            (void)Platform::Filesystem::RemoveFile(payload);
        if (!empty.empty())
            (void)Platform::Filesystem::RemoveFile(empty);
        if (!nested.empty())
            (void)Platform::Filesystem::RemoveEmptyDirectory(nested);
        if (!root.empty())
            (void)Platform::Filesystem::RemoveEmptyDirectory(root);
        if (!seed.empty())
            (void)Platform::Filesystem::RemoveFile(seed);
    }
};

void TestPathSemantics(TestContext& test)
{
    using Platform::Filesystem::Join;
    using Platform::Filesystem::Normalize;

    test.Check(Normalize(R"(pack\locale\..\root.pck)") == "pack/root.pck",
               "Normalize resolves separators and lexical parent components");
    test.Check(Normalize("pack//./root.pck") == "pack/root.pck",
               "Normalize removes duplicate separators and current-directory components");
    test.Check(Normalize(".") == "." && Normalize("pack/..") == ".",
               "Normalize preserves a relative current-directory result");
    test.Check(Join("C:", "root.pck") == "C:root.pck",
               "Join preserves a bare drive-relative prefix");
    test.Check(Normalize(R"(C:\pack\..\root.pck)") == "C:/root.pck",
               "Normalize preserves an absolute drive root");
    test.Check(Normalize(R"(C:pack\..\root.pck)") == "C:root.pck",
               "Normalize preserves drive-relative semantics");
    test.Check(Normalize(R"(\\server\share\pack\..\root.pck)") == "//server/share/root.pck",
               "Normalize protects the UNC server and share components");
    test.Check(Join("pack", "root.pck") == "pack/root.pck",
               "Join preserves the legacy relative pack path");
    test.Check(Join("pack/", "./root.pck") == "pack/root.pck",
               "Join normalizes its result");
    test.Check(Platform::Filesystem::ConfigDirectory() == "config",
               "ConfigDirectory preserves the relative current-directory contract");
}

void TestFilesystem(TestContext& test)
{
    using namespace Platform::Filesystem;

    test.Check(!ExecutablePath().empty(), "ExecutablePath is available");
    test.Check(!AppDirectory().empty(), "AppDirectory is available");
    test.Check(!CurrentDirectory().empty(), "CurrentDirectory is available");
    test.Check(WritableDirectory() == CurrentDirectory(),
               "WritableDirectory preserves the current-directory contract");
    test.Check(!TemporaryDirectory().empty(), "TemporaryDirectory is available");

    TemporaryTree tree;
    tree.seed = CreateTemporaryFileName("c2x");
    test.Check(!tree.seed.empty(), "CreateTemporaryFileName returns a path");
    if (tree.seed.empty())
        return;

    test.Check(Exists(tree.seed), "CreateTemporaryFileName creates the seed file");
    test.Check(RemoveFile(tree.seed), "temporary seed file can be removed");

    tree.root = tree.seed + ".directory";
    tree.nested = Join(tree.root, "nested");
    tree.payload = Join(tree.nested, "payload.bin");
    tree.empty = Join(tree.nested, "empty.bin");
    test.Check(CreateDirectories(tree.nested), "CreateDirectories creates a nested tree");
    test.Check(Exists(tree.nested), "created directory exists");

    constexpr std::array<std::uint8_t, 6> expected = {0x00, 0x11, 0x7f, 0x80, 0xfe, 0xff};
    File file;
    test.Check(file.Open(tree.payload, OpenMode::Write), "File opens a new file for writing");
    if (file.IsOpen())
    {
        test.Check(file.Write(expected.data(), expected.size()), "File writes the complete payload");
        test.Check(file.Size() == expected.size(), "File updates its size after writing");
        test.Check(file.Position() == expected.size(), "File reports its write position");
        test.Check(file.Seek(0, SeekOrigin::Begin), "File seeks to the beginning");

        std::array<std::uint8_t, expected.size()> sameHandle{};
        test.Check(file.Read(sameHandle.data(), sameHandle.size()), "File reads after seeking");
        test.Check(sameHandle == expected, "File read preserves binary bytes");
        file.Close();
    }

    std::vector<std::uint8_t> bytes;
    test.Check(ReadFile(tree.payload, bytes), "ReadFile reads an existing file");
    test.Check(bytes == std::vector<std::uint8_t>(expected.begin(), expected.end()),
               "ReadFile returns the complete binary payload");

    std::uint64_t size = 999;
    test.Check(FileSize(tree.payload, size), "FileSize bool overload succeeds for an existing file");
    test.Check(size == expected.size(), "FileSize reports the payload size");
    test.Check(FileSize(tree.payload) == expected.size(), "FileSize value overload reports the payload size");

    File emptyFile;
    test.Check(emptyFile.Open(tree.empty, OpenMode::Write), "File creates an empty file");
    emptyFile.Close();
    size = 999;
    test.Check(FileSize(tree.empty, size), "FileSize bool overload distinguishes an empty file from an error");
    test.Check(size == 0, "empty file size is zero");

    const std::string missing = Join(tree.nested, "missing.bin");
    bytes.assign(3, 0xaa);
    test.Check(!ReadFile(missing, bytes), "ReadFile fails for a missing file");
    test.Check(bytes.empty(), "ReadFile clears output on failure");
    size = 999;
    test.Check(!FileSize(missing, size), "FileSize bool overload fails for a missing file");
    test.Check(size == 0, "FileSize bool overload clears output on failure");
    test.Check(FileSize(missing) == 0,
               "FileSize value overload retains its documented empty-or-error zero result");
}

void TestTime(TestContext& test)
{
    test.Check(Platform::Time::Initialize(), "high-resolution monotonic clock initializes");
    const std::uint64_t before = Platform::Time::MonotonicNanoseconds();
    Platform::Time::SleepMilliseconds(5);
    const std::uint64_t after = Platform::Time::MonotonicNanoseconds();
    test.Check(after > before, "monotonic time advances across SleepMilliseconds");
}

void TestNetworking(TestContext& test)
{
    const bool firstStartup = Platform::Networking::Startup();
    test.Check(firstStartup, "first networking startup succeeds");
    const bool secondStartup = firstStartup && Platform::Networking::Startup();
    test.Check(secondStartup, "nested networking startup succeeds");

    if (secondStartup)
    {
        std::uint32_t address = 0;
        test.Check(Platform::Networking::ResolveIPv4("127.0.0.1", address),
                   "numeric IPv4 address resolves");
        test.Check(address == 0x7f000001U, "resolved IPv4 address uses host byte order");

        std::array<char, 256> hostName{};
        test.Check(Platform::Networking::GetLocalHostName(hostName.data(), hostName.size()),
                   "local host name is available after nested startup");
        test.Check(hostName.front() != '\0', "local host name is non-empty");
    }

    if (secondStartup)
        Platform::Networking::Shutdown();
    if (firstStartup)
        Platform::Networking::Shutdown();
}

void TestDynamicLibrary(TestContext& test)
{
    Platform::DynamicLibrary library;
    test.Check(library.LoadSystem("kernel32.dll"), "kernel32 loads from the system directory");
    test.Check(library.IsLoaded(), "loaded system library reports loaded state");
    test.Check(library.GetSymbol("GetCurrentProcessId") != nullptr,
               "known kernel32 symbol resolves");
    test.Check(library.GetSymbol("C2X_Missing_Kernel32_Symbol") == nullptr,
               "missing symbol reports failure");

    Platform::DynamicLibrary moved(std::move(library));
    test.Check(!library.IsLoaded(), "moving a dynamic library clears the source");
    test.Check(moved.IsLoaded(), "moving a dynamic library preserves the destination");
    moved.Reset();
    test.Check(!moved.IsLoaded(), "Reset unloads the dynamic library");

    test.Check(!moved.LoadSystem("c2x-definitely-missing-library.dll"),
               "missing system library reports failure");
    test.Check(!moved.IsLoaded(), "failed library load leaves an unloaded object");
}
}

int main()
{
    TestContext test;
    TestPathSemantics(test);
    TestFilesystem(test);
    TestTime(test);
    TestNetworking(test);
    TestDynamicLibrary(test);
    return test.Result();
}
