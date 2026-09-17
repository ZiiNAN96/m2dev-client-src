#include "GR2File.h"
#include "EterBase/MapLoadTrace.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace AssetRuntime::GR2
{
std::size_t Product(std::size_t count, std::size_t stride)
{
    Require(!stride || count <= std::numeric_limits<std::size_t>::max() / stride, "size multiplication overflow");
    return count * stride;
}
void Range(std::size_t offset, std::size_t size, std::size_t limit)
{
    Require(offset <= limit && size <= limit - offset, "out-of-range offset/size");
}
std::uint32_t U32(std::span<const std::byte> bytes, std::size_t offset)
{
    Range(offset, 4, bytes.size());
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(std::to_integer<std::uint8_t>(bytes[offset + i])) << (i * 8);
    return value;
}
Header Inspect(std::span<const std::byte> bytes, bool verifyChecksum)
{
    MapLoadTrace::Scope p0lScope("Assets","GR2 header checksum","cpu");

    Require(bytes.size() <= MaximumFileBytes, "file allocation limit");
    Range(0, 32, bytes.size());
    const std::array<std::uint32_t, 4> oldMagic{0xcab067b8,0x0fb16df8,0x7e8c7284,0x1e00195e};
    const std::array<std::uint32_t, 4> newMagic{0xc06cde29,0x2b53a4ba,0xa5b7f525,0xeee266f6};
    const std::array<std::uint32_t, 4> wideMagic{0x5e499be5,0x141f636f,0xa9eb131e,0xc4edbe90};
    std::array<std::uint32_t, 4> magic{};
    for (unsigned i = 0; i < 4; ++i) magic[i] = U32(bytes, i * 4);
    if (magic == wideMagic) throw Error(Failure::UnsupportedVersion, "64-bit disk pointers not in audited corpus");
    Require(magic == oldMagic || magic == newMagic, "invalid or unsupported endian magic");
    Header result;
    result.version = U32(bytes, 32);
    if (result.version != 6 && result.version != 7)
        throw Error(Failure::UnsupportedVersion, "unsupported version " + std::to_string(result.version));
    const std::size_t headerBytes = result.version == 6 ? 56 : 72;
    Range(32, headerBytes, bytes.size());
    Require(U32(bytes, 36) == bytes.size(), "declared file size differs from input");
    result.crc = U32(bytes, 40);
    Require(U32(bytes, 44) == headerBytes, "invalid section table offset");
    const auto count = U32(bytes, 48);
    Require(count > 0 && count <= 64, "invalid section count");
    const auto tableEnd = 32 + headerBytes + Product(count, 44);
    Range(32 + headerBytes, Product(count, 44), bytes.size());
    result.headerSize = U32(bytes, 16);
    Require(result.headerSize == tableEnd, "invalid header size");
    Require(U32(bytes, 20) == 0 && U32(bytes, 24) == 0 && U32(bytes, 28) == 0, "unsupported header encoding");
    result.rootType = {U32(bytes, 52), U32(bytes, 56)};
    result.rootObject = {U32(bytes, 60), U32(bytes, 64)};
    result.tag = U32(bytes, 68);
    struct Extent { std::size_t start, end; };
    std::vector<Extent> extents{{0, tableEnd}};
    auto extent = [&](std::size_t offset, std::size_t size) {
        Range(offset, size, bytes.size());
        if (size) extents.push_back({offset, offset + size});
    };
    std::size_t expanded = 0, fixups = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto at = 32 + headerBytes + i * 44;
        Section s{U32(bytes,at),U32(bytes,at+4),U32(bytes,at+8),U32(bytes,at+12),U32(bytes,at+16),
            U32(bytes,at+20),U32(bytes,at+24),U32(bytes,at+28),U32(bytes,at+32),U32(bytes,at+36),U32(bytes,at+40)};
        Require(s.alignment && s.alignment <= 4096 && std::has_single_bit(s.alignment), "invalid section alignment");
        Require(s.stop0 <= s.stop1 && s.stop1 <= s.expanded, "invalid compression stops");
        Require((s.stop0 % 4) == 0 && (s.stop1 % 2) == 0, "invalid mixed-data alignment");
        Range(expanded, s.expanded, MaximumExpandedBytes); expanded += s.expanded;
        Range(fixups, s.fixupCount, MaximumElements); fixups += s.fixupCount;
        extent(s.offset, s.compressed);
        extent(s.fixupOffset, Product(s.fixupCount, 12));
        extent(s.marshalOffset, Product(s.marshalCount, 16));
        if (!s.compression) Require(s.compressed == s.expanded, "raw section size mismatch");
        Require(bool(s.compressed) == bool(s.expanded), "empty section size mismatch");
        result.sections.push_back(s);
    }
    std::sort(extents.begin(), extents.end(), [](auto a, auto b) { return a.start < b.start; });
    for (std::size_t i = 1; i < extents.size(); ++i) Require(extents[i].start >= extents[i-1].end, "overlapping file ranges");
    auto valid = [&](Ref r) { return r.section < count && r.offset < result.sections[r.section].expanded; };
    Require(valid(result.rootType) && valid(result.rootObject), "invalid root references");
    if (verifyChecksum) {
        static const auto table = [] {
            std::array<std::uint32_t,256> t{};
            for (unsigned i=0;i<256;++i) { auto c=i; for(unsigned j=0;j<8;++j) c=(c>>1)^((c&1)?0xedb88320u:0); t[i]=c; }
            return t;
        }();
        std::uint32_t crc = 0xffffffffu;
        for (auto b : bytes.subspan(32 + headerBytes)) crc = (crc >> 8) ^ table[(crc ^ std::to_integer<unsigned>(b)) & 255];
        Require((crc ^ 0xffffffffu) == result.crc, "file checksum mismatch");
    }
    return result;
}
File::File(std::span<const std::byte> bytes) : header(Inspect(bytes))
{
    MapLoadTrace::Scope p0lScope("Assets","GR2 relocations","cpu");

    for (const auto& section : header.sections) sections_.push_back(Decompress(section, bytes.subspan(section.offset, section.compressed)));
    for (std::uint32_t i = 0; i < header.sections.size(); ++i) {
        const auto& s = header.sections[i];
        for (std::uint32_t j = 0; j < s.fixupCount; ++j) {
            const auto at = std::size_t(s.fixupOffset) + j * 12;
            Ref from{i,U32(bytes,at)}, to{U32(bytes,at+4),U32(bytes,at+8)};
            Bytes(from,4); Bytes(to,1);
            Require(relocations_.emplace(from,to).second, "duplicate relocation source");
        }
        for (std::uint32_t j = 0; j < s.marshalCount; ++j) {
            const auto at = std::size_t(s.marshalOffset) + j * 16;
            Require(U32(bytes,at) <= MaximumElements, "marshalling count limit");
            Bytes({i,U32(bytes,at+4)},1); Bytes({U32(bytes,at+8),U32(bytes,at+12)},4);
        }
    }
}
Ref File::Add(Ref ref, std::size_t offset) const
{
    Bytes(ref,0); Range(ref.offset, offset, sections_[ref.section].size());
    return {ref.section, static_cast<std::uint32_t>(ref.offset + offset)};
}
std::span<const std::byte> File::Bytes(Ref ref, std::size_t size) const
{
    Require(ref.section < sections_.size(), "invalid section reference");
    const auto& section = sections_[ref.section]; Range(ref.offset,size,section.size());
    return std::span<const std::byte>(section).subspan(ref.offset,size);
}
float File::Float(Ref ref) const
{
    auto value=std::bit_cast<float>(Uint(ref)); Require(std::isfinite(value), "nonfinite float"); return value;
}
Ref File::Pointer(Ref ref) const
{
    Bytes(ref,4);
    const auto found=relocations_.find(ref);
    if(found!=relocations_.end()) return found->second;
    Require(Uint(ref)==0, "non-null pointer without relocation at "+std::to_string(ref.section)+":"+std::to_string(ref.offset)+" value="+std::to_string(Uint(ref))); return {};
}
std::string File::String(Ref ref) const
{
    if(!ref) return {};
    Bytes(ref,1);
    const auto bytes=Bytes(ref,std::min<std::size_t>(4097,sections_[ref.section].size()-ref.offset));
    std::string result;
    for(auto b:bytes) { if(b==std::byte{}) return result; result.push_back(static_cast<char>(b)); }
    Bad("unterminated or oversized string");
}
std::size_t File::ExpandedBytes() const
{
    std::size_t total=0; for(const auto& section:sections_) total+=section.size(); return total;
}
}
