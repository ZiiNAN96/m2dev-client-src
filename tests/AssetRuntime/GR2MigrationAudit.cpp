// Gate A preflight. Uses the production pack loader and explicitly separate
// native/reference loads. This is an audit utility, not a passing CTest gate.
#include "EterGrnLib/StdAfx.h"
#include "PackLib/PackManager.h"
#include "AssetRuntime/Providers.h"
#include "AssetRuntime/GR2ReaderMode.h"
#include "AssetRuntime/GR2/GR2AssetProvider.h"
#include "AssetRuntime/Granny/GrannyAssetProvider.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

static void Require(bool ok, const std::string& message)
{
    if (!ok) throw std::runtime_error(message);
}
static std::vector<std::byte> Read(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    Require(bool(stream), "missing source: " + path.string());
    const auto size = stream.tellg();
    Require(size > 0 && size < 256 * 1024 * 1024, "invalid source size");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    stream.seekg(0); stream.read(reinterpret_cast<char*>(bytes.data()), size);
    Require(bool(stream), "source read failed");
    return bytes;
}
int main(int argc, char** argv)
{
    try {
        Require(argc == 5, "client-root pack-order.txt inputs.tsv output.tsv required");
        const std::filesystem::path client = argv[1];
        std::ifstream order(argv[2]), inputs(argv[3]);
        std::ofstream output(argv[4]);
        Require(bool(order) && bool(inputs) && bool(output), "audit files unavailable");
        CPackManager packs;
        std::string line;
        while (std::getline(order, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            Require(!line.empty() && packs.AddPack((client / "pack" / (line + ".pck")).string()), "pack load failed: " + line);
        }
        output << "kind\tsource\tvirtual_path\tpacked\tidentical\tnative\treference\tdiagnostic\traw_native\traw_diagnostic\n";
        std::size_t rejected = 0, rawRejected = 0, checked = 0, configurationMismatches = 0;
        while (std::getline(inputs, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream row(line);
            std::string kind, source, id;
            Require(bool(std::getline(row, kind, '\t')) && bool(std::getline(row, source, '\t')) && bool(std::getline(row, id)), "invalid manifest row");
            auto original = Read(client / "assets" / source);
            TPackFile packed;
            const bool exists = packs.GetFile(id, packed);
            const auto bytes = std::as_bytes(std::span(packed));
            const bool identical = exists && std::ranges::equal(original, bytes);
            output << kind << '\t' << source << '\t' << id << '\t' << exists << '\t' << identical;
            if (kind == "gr2" && exists) {
                // Exercise the same single-provider dispatch as CGraphicThing::OnLoad.
                AssetRuntime::startupGR2Reader = AssetRuntime::GR2ReaderMode::ZiiNAN;
                const auto before = AssetRuntime::grannyFileReads.load();
                auto native = AssetRuntime::LoadModel(id, bytes);
                auto raw = identical ? AssetRuntime::LoadResult{} : AssetRuntime::LoadModel(id, original);
                Require(AssetRuntime::grannyFileReads == before, "silent Granny fallback detected");
                // Reference is requested explicitly by this audit, never by LoadModel.
                auto reference = AssetRuntime::GetGrannyAssetProvider().Load(id, bytes);
                output << '\t' << bool(native) << '\t' << bool(reference) << '\t' << native.diagnostic;
                const bool rawOk = identical ? bool(native) : bool(raw);
                output << '\t' << rawOk << '\t' << (identical ? native.diagnostic : raw.diagnostic);
                if (!rawOk) ++rawRejected;
                if (identical && !native) ++rejected;
                ++checked;
            } else {
                if (kind == "config" && !identical) ++configurationMismatches;
                output << "\tNA\tNA\t\tNA\t";
            }
            output << '\n';
        }
        Require(checked == 52, "all 52 rejected corpus entries must be found in packs");
        Require(AssetRuntime::liveDocuments == 0 && AssetRuntime::GR2::liveReaderDocuments == 0,
            "audit asset documents leaked");
        output.flush(); Require(bool(output), "audit output failed");
        Require(configurationMismatches == 0, "source configuration differs from packed configuration; reachability inventory must be refreshed");
        std::cout << "PackedGR2Checked=" << checked << " RawRejects=" << rawRejected << " IdenticalPackedRejects=" << rejected
                  << " AssetDocuments=0 GR2ReaderResources=0 SilentFallbacks=0\n";
        // Presence alone does not prove use; combine this evidence with config chains.
        return rejected ? 2 : 0;
    } catch (const std::exception& error) {
        std::cerr << "AUDIT ERROR: " << error.what() << '\n'; return 1;
    }
}
