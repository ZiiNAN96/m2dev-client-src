#include "EterLib/ControlPackets.h"
#include "EterLib/PacketReader.h"
#include "EterLib/PacketWriter.h"
#include "PackLib/PackFormat.h"
#include "PRTerrainLib/WaterHeightFormat.h"
#include "Platform/NativeTypes.h"
#include "Platform/PlatformFilesystem.h"
#include "Platform/PlatformNetworking.h"
#include "Platform/PlatformWindow.h"
#include "Renderer/SkinningData.h"

#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

// ZiiNAN: Cross-platform bootstrap
static_assert(CHAR_BIT == 8);
static_assert(sizeof(void*) == 8 && alignof(void*) == 8);
static_assert(sizeof(std::size_t) == 8 && sizeof(std::ptrdiff_t) == 8);
static_assert(sizeof(std::intptr_t) == 8 && sizeof(std::uintptr_t) == 8);
static_assert(sizeof(std::uint16_t) == 2 && sizeof(std::uint32_t) == 4);
static_assert(sizeof(std::uint64_t) == 8 && sizeof(std::int32_t) == 4);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(std::endian::native == std::endian::little,
    "Existing packet and pack formats require little-endian targets; do not silently change the protocol.");
#if defined(_WIN32)
static_assert(sizeof(long) == 4, "Windows x64 uses LLP64.");
#else
static_assert(sizeof(long) == 8, "The portable 64-bit target uses LP64.");
#endif

static_assert(std::is_trivially_copyable_v<TPacketGCPhase>);
static_assert(sizeof(TPacketGCPhase) == 5 && alignof(TPacketGCPhase) == 1);
static_assert(offsetof(TPacketGCPhase, phase) == 4);
static_assert(sizeof(TPacketGCPing) == 8 && alignof(TPacketGCPing) == 1);
static_assert(offsetof(TPacketGCPing, server_time) == 4);
static_assert(sizeof(TPacketCGPong) == 4 && alignof(TPacketCGPong) == 1);
static_assert(sizeof(TPacketGCKeyChallenge) == 72 && alignof(TPacketGCKeyChallenge) == 1);
static_assert(offsetof(TPacketGCKeyChallenge, server_time) == 68);
static_assert(sizeof(TPacketCGKeyResponse) == 68 && alignof(TPacketCGKeyResponse) == 1);
static_assert(offsetof(TPacketCGKeyResponse, challenge_response) == 36);
static_assert(sizeof(TPacketGCKeyComplete) == 76 && alignof(TPacketGCKeyComplete) == 1);
static_assert(offsetof(TPacketGCKeyComplete, nonce) == 52);
static_assert(std::is_standard_layout_v<TPackFileHeader> && std::is_trivially_copyable_v<TPackFileHeader>);
static_assert(std::is_standard_layout_v<TPackFileEntry> && std::is_trivially_copyable_v<TPackFileEntry>);
static_assert(std::is_same_v<decltype(TPackFileEntry::offset), std::uint64_t>);
static_assert(std::is_same_v<decltype(TPacketGCPing::server_time), std::uint32_t>);
static_assert(sizeof(TerrainFormat::WaterHeight) == 4 && alignof(TerrainFormat::WaterHeight) == 4);

static_assert(sizeof(Renderer::SkinningVertex) == 40 && alignof(Renderer::SkinningVertex) == 4);
static_assert(offsetof(Renderer::SkinningVertex, weights) == 12);
static_assert(offsetof(Renderer::SkinningVertex, indices) == 16);
static_assert(offsetof(Renderer::SkinningVertex, normal) == 20);
static_assert(offsetof(Renderer::SkinningVertex, uv) == 32);
static_assert(sizeof(Renderer::SkinningMatrix) == 64 && alignof(Renderer::SkinningMatrix) == 4);
static_assert(sizeof(Renderer::SkinMaterialGroup) == 12 && alignof(Renderer::SkinMaterialGroup) == 4);
static_assert(sizeof(Renderer::SkinningVertex::indices) == 4);
static_assert(sizeof(decltype(Renderer::BoneRemap::meshToSkeleton)::value_type) == 2);
static_assert(sizeof(decltype(Renderer::StaticSkinnedMeshData::indices)::value_type) == 2);
static_assert(std::is_same_v<decltype(Renderer::StaticSkinnedMeshData::deformVertexOffset), std::uint32_t>);
static_assert(Renderer::productionSkinningMode == Renderer::SkinningMode::GPU);

static_assert(sizeof(Platform::NativeWindowHandle) == 8 && alignof(Platform::NativeWindowHandle) == 8);
static_assert(std::is_standard_layout_v<Platform::NativeWindowHandle>);
static_assert(std::is_trivially_copyable_v<Platform::NativeWindowHandle>);
static_assert(sizeof(Platform::NativeCursorHandle) == 8 && alignof(Platform::NativeCursorHandle) == 8);
static_assert(sizeof(Platform::Networking::NativeSocket) == 8 && alignof(Platform::Networking::NativeSocket) == 8);
static_assert(sizeof(Platform::NativeMessage) == 32 && alignof(Platform::NativeMessage) == 8);
static_assert(offsetof(Platform::NativeMessage, id) == 8);
static_assert(offsetof(Platform::NativeMessage, wParam) == 16);
static_assert(offsetof(Platform::NativeMessage, lParam) == 24);
static_assert(std::is_same_v<decltype(std::declval<Platform::Filesystem::File>().Size()), std::uint64_t>);
static_assert(std::is_same_v<decltype(std::declval<Platform::Filesystem::File>().Position()), std::uint64_t>);

namespace
{
bool Check(bool passed, const char* description)
{
    if (!passed)
        std::fprintf(stderr, "Portable ABI: %s\n", description);
    return passed;
}

bool CheckSerialization()
{
    // An odd starting address also exercises the memcpy path on alignment-sensitive targets.
    std::array<std::uint8_t, 24> storage{};
    PacketWriter writer(storage.data() + 1, storage.size() - 1);
    if (!writer.WriteU16(0x1234) || !writer.WriteU32(0x89abcdefu) || !writer.WriteI32(-2) ||
        !writer.WriteU64(0x0123456789abcdefULL) || !writer.WriteFloat(1.0f))
        return false;
    const std::array<std::uint8_t, 22> expected{
        0x34, 0x12, 0xef, 0xcd, 0xab, 0x89, 0xfe, 0xff, 0xff, 0xff,
        0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01, 0x00, 0x00, 0x80, 0x3f};
    if (writer.Written() != expected.size() || std::memcmp(writer.Data(), expected.data(), expected.size()) != 0)
        return false;
    PacketReader reader(storage.data() + 1, writer.Written());
    return reader.ReadU16() == 0x1234 && reader.ReadU32() == 0x89abcdefu &&
        reader.ReadI32() == -2 && reader.ReadU64() == 0x0123456789abcdefULL &&
        reader.ReadFloat() == 1.0f && reader.Remaining() == 0;
}

bool CheckPacketsAndDisk()
{
    const TPacketGCPhase phase{GC::PHASE, sizeof(TPacketGCPhase), 3};
    const std::array<std::uint8_t, 5> phaseBytes{0x08, 0x00, 0x05, 0x00, 0x03};
    if (std::memcmp(&phase, phaseBytes.data(), phaseBytes.size()) != 0)
        return false;
    TPackFileEntry entry{};
    entry.offset = 0x0123456789abcdefULL;
    entry.file_size = 0x0000000100000001ULL;
    entry.compressed_size = 0x0000000200000002ULL;
    entry.encryption = 1;
    std::array<std::uint8_t, sizeof(TPackFileEntry) + 1> bytes{};
    std::memcpy(bytes.data() + 1, &entry, sizeof(entry));
    const std::array<std::uint8_t, 8> offsetBytes{0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01};
    if (std::memcmp(bytes.data() + 1 + 261, offsetBytes.data(), offsetBytes.size()) != 0)
        return false;
    TPackFileEntry decoded{};
    std::memcpy(&decoded, bytes.data() + 1, sizeof(decoded));
    return decoded.offset == entry.offset && decoded.file_size == entry.file_size &&
        decoded.compressed_size == entry.compressed_size && decoded.encryption == 1;
}

bool CheckPointerRoundTrip()
{
    int local = 0;
    const std::array<void*, 3> values{nullptr, &local,
        reinterpret_cast<void*>(std::uintptr_t{0x0000000123456789ULL})};
    for (void* pointer : values)
    {
        const auto encoded = reinterpret_cast<std::uintptr_t>(pointer);
        Platform::NativeWindowHandle handle{reinterpret_cast<void*>(encoded)};
        const Platform::NativeMessage message{handle, 0x1234u, encoded, static_cast<std::intptr_t>(encoded)};
        if (handle.value != pointer || reinterpret_cast<void*>(message.wParam) != pointer ||
            reinterpret_cast<void*>(message.lParam) != pointer || bool(handle) != (pointer != nullptr))
            return false;
    }
    const Platform::Networking::NativeSocket invalid{};
    const Platform::Networking::NativeSocket validZero{0};
    return !invalid && bool(validZero);
}

bool CheckWaterHeights()
{
    const std::array<std::uint8_t, 13> modern{
        0xaa, 0xff, 0xff, 0xff, 0xff, 0x67, 0x45, 0x23, 0x01, 0x00, 0x00, 0x00, 0x80};
    std::array<TerrainFormat::WaterHeight, 5> heights{-7, 0, 0, 0, -9};
    const auto output = std::span(heights).subspan(1, 3);
    if (!TerrainFormat::DecodeWaterHeights(std::span(modern).subspan(1), output) ||
        heights != std::array<TerrainFormat::WaterHeight, 5>{-7, -1, 0x01234567,
            std::numeric_limits<std::int32_t>::min(), -9})
        return false;

    const std::array<std::uint8_t, 6> legacy{0xff, 0xff, 0x34, 0x12, 0x00, 0x00};
    if (!TerrainFormat::DecodeWaterHeights(legacy, output) ||
        heights != std::array<TerrainFormat::WaterHeight, 5>{-7, 65535, 0x1234, 0, -9})
        return false;
    const auto before = heights;
    const std::array<std::uint8_t, 7> tooLong{};
    if (TerrainFormat::DecodeWaterHeights(std::span(legacy).first(5), output) ||
        TerrainFormat::DecodeWaterHeights(tooLong, output) ||
        TerrainFormat::DecodeWaterHeights({}, output) || heights != before)
        return false;
    return TerrainFormat::DecodeWaterHeights({}, {});
}

bool CheckBounds()
{
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    std::array<std::uint8_t, 8> bytes{};
    PacketWriter writer(bytes.data(), bytes.size());
    if (!writer.WriteU8(0x5a))
        return false;
    const auto before = bytes;
    if (writer.WriteBytes(bytes.data(), maximum) || writer.WriteString("x", maximum) ||
        writer.PatchU16(maximum, 1) || writer.PatchU32(maximum - 1, 1) ||
        writer.WriteString("", 0) || bytes != before || writer.Written() != 1)
        return false;
    if (!writer.WriteBytes(nullptr, 0) || !writer.WriteString(nullptr, 3) || writer.Written() != 4 ||
        !writer.PatchU16(2, 0x1234) || writer.PatchU32(2, 1))
        return false;
    PacketReader reader(bytes.data(), bytes.size());
    if (reader.ReadU8() != 0x5a)
        return false;
    std::uint8_t destination = 0x7b;
    if (reader.HasBytes(maximum) || reader.Skip(maximum) || reader.ReadBytes(&destination, maximum) ||
        reader.ReadString(reinterpret_cast<char*>(&destination), 0) || reader.Position() != 1 || destination != 0x7b)
        return false;
    if (!reader.ReadBytes(nullptr, 0) || !reader.Skip(7) || reader.HasBytes(1))
        return false;
    TPacketGCPing ping{};
    if (reader.ReadStruct(ping) || reader.Position() != bytes.size())
        return false;
    PacketReader empty(nullptr, 0);
    return empty.ReadBytes(nullptr, 0) && empty.Skip(0) && !empty.HasBytes(1);
}
}

int main()
{
    bool passed = Check(CheckSerialization(), "fixed-width serialized bytes or unaligned roundtrip changed");
    passed = Check(CheckPacketsAndDisk(), "existing packet/disk bytes or 64-bit offsets changed") && passed;
    passed = Check(CheckPointerRoundTrip(), "native handles lost pointer bits or sentinel semantics") && passed;
    passed = Check(CheckWaterHeights(), "modern/legacy water height bytes or malformed-length rejection changed") && passed;
    passed = Check(CheckBounds(), "size_t overflow or zero-length operation changed buffer/cursor") && passed;
    if (passed)
        std::printf("Portable ABI passed: pointer=%zu long=%zu size_t=%zu, little-endian\n",
            sizeof(void*), sizeof(long), sizeof(std::size_t));
    return passed ? 0 : 1;
}
