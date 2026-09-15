#include "GraphicsSettingsFile.h"
#include <fstream>
#include <iterator>
#include <system_error>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Graphics
{
namespace
{
bool Read(const std::filesystem::path& path, std::string& text, std::string& error)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) && !ec) return true;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > 65536) { error = "Graphics config is unreadable or exceeds 64 KiB"; return false; }
    std::ifstream input(path, std::ios::binary);
    if (!input) { error = "Cannot read graphics config"; return false; }
    text.resize(static_cast<std::size_t>(size));
    if (!text.empty() && !input.read(text.data(), static_cast<std::streamsize>(text.size())))
    { error = "Cannot read complete graphics config"; return false; }
    return true;
}
}
LoadResult LoadGraphicsSettingsFile(const std::filesystem::path& path, const GraphicsSettings& defaults, std::string& error)
{
    error.clear();
    std::string text;
    if (!Read(path, text, error)) return {Validate(defaults), false, 1};
    auto result = LoadGraphicsSettings(text, defaults);
    if (!result.writable) error = "Unsupported graphics config version; original file preserved";
    return result;
}
bool SaveGraphicsSettingsFile(const std::filesystem::path& path, const GraphicsSettings& settings, std::string& error)
{
    error.clear();
    std::string previous;
    if (!Read(path, previous, error)) return false;
    const auto text = SaveGraphicsSettings(settings, previous);
    if (text.empty()) { error = "Unsupported graphics config version; refusing to overwrite"; return false; }
    // A separate temporary file, closed and checked before atomic replacement.
    // Failed writes leave the existing config untouched.
    auto temporary = path;
    temporary += ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.flush();
    const bool written = bool(output);
    output.close();
    if (!written || output.fail()) { error = "Cannot write graphics config temporary file"; return false; }
    std::error_code ec;
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
    std::filesystem::rename(temporary, path, ec);
#endif
    if (ec) { error = "Cannot replace graphics config: " + ec.message(); return false; }
    return true;
}
}
