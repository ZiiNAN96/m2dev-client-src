#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace AssetRuntime::GR2
{
enum class Failure { UnsupportedVersion, UnsupportedCompression, UnsupportedType, Malformed, Other };
class Error final : public std::runtime_error
{
public:
    Error(Failure kind, const std::string& message) : std::runtime_error(message), kind(kind) {}
    Failure kind;
};
[[noreturn]] inline void Bad(const std::string& message) { throw Error(Failure::Malformed, message); }
[[noreturn]] inline void Unsupported(const std::string& message) { throw Error(Failure::UnsupportedType, message); }
inline void Require(bool ok, std::string_view message) { if (!ok) Bad(std::string(message)); }
constexpr std::size_t MaximumFileBytes = 256 * 1024 * 1024;
constexpr std::size_t MaximumExpandedBytes = 256 * 1024 * 1024;
constexpr std::size_t MaximumElements = 4 * 1024 * 1024;
std::size_t Product(std::size_t count, std::size_t stride);
void Range(std::size_t offset, std::size_t size, std::size_t limit);
std::uint32_t U32(std::span<const std::byte> bytes, std::size_t offset);
struct Ref
{
    std::uint32_t section = UINT32_MAX, offset = 0;
    explicit operator bool() const { return section != UINT32_MAX; }
    auto operator<=>(const Ref&) const = default;
};
struct Section
{
    std::uint32_t compression{}, offset{}, compressed{}, expanded{}, alignment{}, stop0{}, stop1{};
    std::uint32_t fixupOffset{}, fixupCount{}, marshalOffset{}, marshalCount{};
};
struct Header
{
    std::uint32_t version{}, tag{}, headerSize{}, crc{};
    Ref rootType, rootObject;
    std::vector<Section> sections;
};
Header Inspect(std::span<const std::byte> bytes, bool verifyChecksum = true);
std::vector<std::byte> Decompress(const Section&, std::span<const std::byte>);
class File
{
public:
    explicit File(std::span<const std::byte> bytes);
    Header header;
    Ref Add(Ref ref, std::size_t offset) const;
    std::span<const std::byte> Bytes(Ref ref, std::size_t size) const;
    std::uint32_t Uint(Ref ref) const { return U32(Bytes(ref, 4), 0); }
    std::int32_t Int(Ref ref) const { return std::bit_cast<std::int32_t>(Uint(ref)); }
    float Float(Ref ref) const;
    Ref Pointer(Ref ref) const;
    std::string String(Ref ref) const;
    std::size_t ExpandedBytes() const;
private:
    std::vector<std::vector<std::byte>> sections_;
    std::map<Ref, Ref> relocations_;
};
}
