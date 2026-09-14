#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace Platform::Filesystem
{
enum class OpenMode
{
    Read,
    Write,
};

enum class SeekOrigin
{
    Begin,
    Current,
};

class File final
{
public:
    File() noexcept = default;
    ~File();

    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;

    bool Open(std::string_view path, OpenMode mode) noexcept;
    void Close() noexcept;
    [[nodiscard]] bool IsOpen() const noexcept;
    [[nodiscard]] std::uint64_t Size() const noexcept;
    [[nodiscard]] std::uint64_t Position() const noexcept;
    bool Seek(std::uint64_t offset, SeekOrigin origin) noexcept;
    bool Read(void* destination, std::size_t bytes) noexcept;
    bool Write(const void* source, std::size_t bytes) noexcept;

private:
    std::uintptr_t m_nativeHandle = 0;
    std::uint64_t m_size = 0;
};

struct DirectoryEntry
{
    std::string name;
    bool isDirectory = false;
};

// ZiiNAN: Platform abstraction
[[nodiscard]] std::string Normalize(std::string_view path);
[[nodiscard]] std::string Join(std::string_view left, std::string_view right);
[[nodiscard]] bool Exists(std::string_view path) noexcept;
[[nodiscard]] bool ReadFile(std::string_view path, std::vector<std::uint8_t>& output) noexcept;
[[nodiscard]] bool FileSize(std::string_view path, std::uint64_t& output) noexcept;
[[nodiscard]] std::uint64_t FileSize(std::string_view path) noexcept;
[[nodiscard]] bool ListDirectory(std::string_view path, std::vector<DirectoryEntry>& output) noexcept;

[[nodiscard]] std::string ExecutablePath();
[[nodiscard]] std::string AppDirectory();
[[nodiscard]] std::string CurrentDirectory();
// C2-X intentionally keeps all writable client data relative to the current working directory.
[[nodiscard]] std::string WritableDirectory();
[[nodiscard]] std::string ConfigDirectory();
[[nodiscard]] std::string TemporaryDirectory();
[[nodiscard]] std::string CreateTemporaryFileName(std::string_view prefix);

[[nodiscard]] bool CreateDirectories(std::string_view path) noexcept;
[[nodiscard]] bool RemoveFile(std::string_view path) noexcept;
[[nodiscard]] bool RemoveEmptyDirectory(std::string_view path) noexcept;

// Preserves the existing narrow CRT path interpretation for legacy config/log callers.
[[nodiscard]] std::FILE* OpenCFile(const char* path, const char* mode) noexcept;
}
