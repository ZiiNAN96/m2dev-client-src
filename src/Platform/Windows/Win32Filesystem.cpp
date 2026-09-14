#include "Platform/PlatformFilesystem.h"

#include <windows.h>

#include <utf8.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <utility>

namespace Platform::Filesystem
{
namespace
{
[[nodiscard]] std::wstring ToWide(std::string_view value)
{
    return Utf8ToWide(std::string(value));
}

[[nodiscard]] HANDLE ToHandle(std::uintptr_t value) noexcept
{
    return reinterpret_cast<HANDLE>(value);
}

[[nodiscard]] std::uintptr_t FromHandle(HANDLE value) noexcept
{
    return reinterpret_cast<std::uintptr_t>(value);
}

[[nodiscard]] bool HasDrivePrefix(std::string_view path) noexcept
{
    return path.size() >= 2 && path[1] == ':' &&
        ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'));
}
}

File::~File()
{
    Close();
}

File::File(File&& other) noexcept
    : m_nativeHandle(std::exchange(other.m_nativeHandle, 0)),
      m_size(std::exchange(other.m_size, 0))
{
}

File& File::operator=(File&& other) noexcept
{
    if (this != &other)
    {
        Close();
        m_nativeHandle = std::exchange(other.m_nativeHandle, 0);
        m_size = std::exchange(other.m_size, 0);
    }
    return *this;
}

bool File::Open(std::string_view path, OpenMode mode) noexcept
{
    Close();
    if (path.empty())
        return false;

    try
    {
        const std::wstring widePath = ToWide(path);
        if (widePath.empty())
            return false;

        const DWORD access = mode == OpenMode::Write ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ;
        const DWORD sharing = mode == OpenMode::Write ? FILE_SHARE_READ | FILE_SHARE_WRITE : FILE_SHARE_READ;
        const DWORD disposition = mode == OpenMode::Write ? OPEN_ALWAYS : OPEN_EXISTING;
        const HANDLE handle = CreateFileW(
            widePath.c_str(), access, sharing, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER size{};
        if (!GetFileSizeEx(handle, &size) || size.QuadPart < 0)
        {
            CloseHandle(handle);
            return false;
        }

        m_nativeHandle = FromHandle(handle);
        m_size = static_cast<std::uint64_t>(size.QuadPart);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void File::Close() noexcept
{
    if (IsOpen())
        CloseHandle(ToHandle(m_nativeHandle));
    m_nativeHandle = 0;
    m_size = 0;
}

bool File::IsOpen() const noexcept
{
    return m_nativeHandle != 0;
}

std::uint64_t File::Size() const noexcept
{
    return m_size;
}

std::uint64_t File::Position() const noexcept
{
    if (!IsOpen())
        return 0;

    LARGE_INTEGER zero{};
    LARGE_INTEGER position{};
    if (!SetFilePointerEx(ToHandle(m_nativeHandle), zero, &position, FILE_CURRENT) || position.QuadPart < 0)
        return 0;
    return static_cast<std::uint64_t>(position.QuadPart);
}

bool File::Seek(std::uint64_t offset, SeekOrigin origin) noexcept
{
    if (!IsOpen() || offset > static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max()))
        return false;

    LARGE_INTEGER distance{};
    distance.QuadPart = static_cast<LONGLONG>(offset);
    return SetFilePointerEx(
        ToHandle(m_nativeHandle), distance, nullptr,
        origin == SeekOrigin::Begin ? FILE_BEGIN : FILE_CURRENT) != FALSE;
}

bool File::Read(void* destination, std::size_t bytes) noexcept
{
    if (!IsOpen() || (bytes != 0 && destination == nullptr))
        return false;

    auto* output = static_cast<std::uint8_t*>(destination);
    std::size_t remaining = bytes;
    while (remaining != 0)
    {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, MAXDWORD));
        DWORD read = 0;
        if (!::ReadFile(ToHandle(m_nativeHandle), output, chunk, &read, nullptr) || read != chunk)
            return false;
        output += read;
        remaining -= read;
    }
    return true;
}

bool File::Write(const void* source, std::size_t bytes) noexcept
{
    if (!IsOpen() || (bytes != 0 && source == nullptr))
        return false;

    const auto* input = static_cast<const std::uint8_t*>(source);
    std::size_t remaining = bytes;
    while (remaining != 0)
    {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, MAXDWORD));
        DWORD written = 0;
        if (!::WriteFile(ToHandle(m_nativeHandle), input, chunk, &written, nullptr) || written != chunk)
            return false;
        input += written;
        remaining -= written;
    }

    LARGE_INTEGER size{};
    if (GetFileSizeEx(ToHandle(m_nativeHandle), &size) && size.QuadPart >= 0)
        m_size = static_cast<std::uint64_t>(size.QuadPart);
    return true;
}

std::string Normalize(std::string_view path)
{
    if (path.empty())
        return {};

    std::string generic(path);
    std::replace(generic.begin(), generic.end(), '\\', '/');

    std::string prefix;
    std::size_t cursor = 0;
    bool rooted = false;
    if (HasDrivePrefix(generic))
    {
        prefix.assign(generic.data(), 2);
        cursor = 2;
        if (cursor < generic.size() && generic[cursor] == '/')
        {
            rooted = true;
            ++cursor;
        }
    }
    else if (generic.rfind("//", 0) == 0)
    {
        prefix = "//";
        cursor = 2;
        rooted = true;
    }
    else if (generic[0] == '/')
    {
        prefix = "/";
        cursor = 1;
        rooted = true;
    }

    std::vector<std::string> components;
    while (cursor <= generic.size())
    {
        const std::size_t end = generic.find('/', cursor);
        const std::size_t count = (end == std::string::npos ? generic.size() : end) - cursor;
        std::string component = generic.substr(cursor, count);
        if (!component.empty() && component != ".")
        {
            const std::size_t protectedComponents = prefix == "//" ? 2 : 0;
            if (component == ".." && components.size() > protectedComponents && components.back() != "..")
                components.pop_back();
            else if (component != ".." || !rooted)
                components.push_back(std::move(component));
        }
        if (end == std::string::npos)
            break;
        cursor = end + 1;
    }

    std::string result = prefix;
    if (HasDrivePrefix(result) && rooted)
        result.push_back('/');
    const bool driveRelative = HasDrivePrefix(result) && !rooted;
    for (std::size_t index = 0; index < components.size(); ++index)
    {
        if (!result.empty() && result.back() != '/' && !(driveRelative && index == 0))
            result.push_back('/');
        result += components[index];
    }

    if (result.empty() && rooted)
        return "/";
    if (result.empty() && !generic.empty())
        return ".";
    return result;
}

std::string Join(std::string_view left, std::string_view right)
{
    if (left.empty())
        return Normalize(right);
    if (right.empty())
        return Normalize(left);

    const std::string normalizedRight = Normalize(right);
    if ((!normalizedRight.empty() && normalizedRight.front() == '/') || HasDrivePrefix(normalizedRight))
        return normalizedRight;

    std::string combined(left);
    if (combined.back() != '/' && combined.back() != '\\' &&
        !(combined.size() == 2 && HasDrivePrefix(combined)))
        combined.push_back('/');
    combined += right;
    return Normalize(combined);
}

bool Exists(std::string_view path) noexcept
{
    if (path.empty())
        return false;
    try
    {
        const std::wstring widePath = ToWide(path);
        return !widePath.empty() && GetFileAttributesW(widePath.c_str()) != INVALID_FILE_ATTRIBUTES;
    }
    catch (...)
    {
        return false;
    }
}

bool ReadFile(std::string_view path, std::vector<std::uint8_t>& output) noexcept
{
    output.clear();
    try
    {
        File file;
        if (!file.Open(path, OpenMode::Read) || file.Size() > std::numeric_limits<std::size_t>::max())
            return false;

        output.resize(static_cast<std::size_t>(file.Size()));
        if (!file.Read(output.data(), output.size()))
        {
            output.clear();
            return false;
        }
        return true;
    }
    catch (...)
    {
        output.clear();
        return false;
    }
}

bool FileSize(std::string_view path, std::uint64_t& output) noexcept
{
    output = 0;
    File file;
    if (!file.Open(path, OpenMode::Read))
        return false;
    output = file.Size();
    return true;
}

std::uint64_t FileSize(std::string_view path) noexcept
{
    std::uint64_t result = 0;
    (void)FileSize(path, result);
    return result;
}

bool ListDirectory(std::string_view path, std::vector<DirectoryEntry>& output) noexcept
{
    output.clear();
    HANDLE find = INVALID_HANDLE_VALUE;
    try
    {
        const std::string query = Join(path.empty() ? "." : path, "*");
        const std::wstring wideQuery = ToWide(query);
        if (wideQuery.empty())
            return false;

        WIN32_FIND_DATAW data{};
        find = FindFirstFileW(wideQuery.c_str(), &data);
        if (find == INVALID_HANDLE_VALUE)
            return false;

        do
        {
            if (data.cFileName[0] == L'.' &&
                (data.cFileName[1] == L'\0' || (data.cFileName[1] == L'.' && data.cFileName[2] == L'\0')))
                continue;
            output.push_back({WideToUtf8(data.cFileName), (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0});
        } while (FindNextFileW(find, &data));

        FindClose(find);
        return true;
    }
    catch (...)
    {
        if (find != INVALID_HANDLE_VALUE)
            FindClose(find);
        output.clear();
        return false;
    }
}

std::string ExecutablePath()
{
    std::vector<wchar_t> buffer(MAX_PATH + 1);
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return {};
        if (length < buffer.size() - 1)
            return Normalize(WideToUtf8(std::wstring(buffer.data(), length)));
        if (buffer.size() >= 32768)
            return {};
        buffer.resize(std::min<std::size_t>(buffer.size() * 2, 32768));
    }
}

std::string AppDirectory()
{
    const std::string executable = ExecutablePath();
    const std::size_t separator = executable.find_last_of('/');
    if (separator == std::string::npos)
        return {};
    if (separator == 2 && HasDrivePrefix(executable))
        return executable.substr(0, 3);
    return executable.substr(0, separator);
}

std::string CurrentDirectory()
{
    const DWORD required = GetCurrentDirectoryW(0, nullptr);
    if (required == 0)
        return {};
    std::vector<wchar_t> buffer(required);
    const DWORD length = GetCurrentDirectoryW(required, buffer.data());
    if (length == 0 || length >= required)
        return {};
    return Normalize(WideToUtf8(std::wstring(buffer.data(), length)));
}

std::string WritableDirectory()
{
    return CurrentDirectory();
}

std::string ConfigDirectory()
{
    // Keep the established relative-to-current-working-directory config contract.
    return "config";
}

std::string TemporaryDirectory()
{
    const DWORD required = GetTempPathW(0, nullptr);
    if (required == 0)
        return {};
    std::vector<wchar_t> buffer(required + 1);
    const DWORD length = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (length == 0 || length >= buffer.size())
        return {};
    return Normalize(WideToUtf8(std::wstring(buffer.data(), length)));
}

std::string CreateTemporaryFileName(std::string_view prefix)
{
    const std::wstring directory = ToWide(TemporaryDirectory());
    if (directory.empty())
        return {};

    std::wstring widePrefix = ToWide(prefix.empty() ? std::string_view{"etb"} : prefix);
    widePrefix.resize(std::min<std::size_t>(widePrefix.size(), 3));

    wchar_t name[MAX_PATH + 1]{};
    if (!GetTempFileNameW(directory.c_str(), widePrefix.c_str(), 0, name))
        return {};
    return Normalize(WideToUtf8(name));
}

bool CreateDirectories(std::string_view path) noexcept
{
    if (path.empty())
        return false;
    try
    {
        const std::wstring widePath = ToWide(path);
        if (widePath.empty())
            return false;
        std::error_code error;
        if (std::filesystem::is_directory(widePath, error))
            return true;
        error.clear();
        return std::filesystem::create_directories(widePath, error) && !error;
    }
    catch (...)
    {
        return false;
    }
}

bool RemoveFile(std::string_view path) noexcept
{
    try
    {
        const std::wstring widePath = ToWide(path);
        return !widePath.empty() && DeleteFileW(widePath.c_str()) != FALSE;
    }
    catch (...)
    {
        return false;
    }
}

bool RemoveEmptyDirectory(std::string_view path) noexcept
{
    try
    {
        const std::wstring widePath = ToWide(path);
        return !widePath.empty() && ::RemoveDirectoryW(widePath.c_str()) != FALSE;
    }
    catch (...)
    {
        return false;
    }
}

std::FILE* OpenCFile(const char* path, const char* mode) noexcept
{
    if (!path || !mode)
        return nullptr;
    return std::fopen(path, mode);
}
}
