#include <AssetRuntime/AnimationStallAudit.h>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Audit = AssetRuntime::AnimationStallAudit;
using namespace std::chrono_literals;

namespace
{
void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void Reset()
{
    Audit::enabled = false;
    Audit::frames.clear(); Audit::events.clear();
    Audit::processStats = {}; Audit::displayStats = {}; Audit::totals = {};
    Audit::origin = {}; Audit::processStart = {}; Audit::accountedUntil = {};
    Audit::previousEnd = {}; Audit::lastDisplay = {};
    Audit::processSerial = Audit::displaySerial = Audit::displayUpdates = 0;
    Audit::swapchainPresents = 0;
    Audit::droppedFrames = Audit::droppedEvents = 0;
    Audit::previousCpu = Audit::previousFallbacks = 0;
    Audit::previousGpuPrepare = 0;
    Audit::inProcess = Audit::processPresented = Audit::processGame = false;
    Audit::processMinimized = Audit::displayGame = Audit::displayMinimized = false;
    Audit::processInactive = Audit::displayInactive = false;
    Audit::peakDisplayUs = Audit::peakGameDisplayUs = 0;
    Audit::peakGamePoseFrameUs = Audit::peakGameSinglePoseUs = 0;
}

Audit::Digest Digest(unsigned seed)
{
    Audit::Digest result{};
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<std::uint8_t>(seed + i * 7);
    return result;
}

const Audit::Frame& DisplayRow()
{
    for (auto it = Audit::frames.rbegin(); it != Audit::frames.rend(); ++it)
        if (it->displayRow) return *it;
    throw std::runtime_error("missing display row");
}

void Disabled()
{
    Reset();
    const auto digest = Digest(1);
    Audit::BeginProcess(true, false);
    {
        Audit::WorkScope pose(Audit::Work::Pose);
        Audit::WorkScope fingerprint(Audit::Work::Fingerprint);
        Audit::ImportStarted(); Audit::InstanceHit(); Audit::ReferencePose();
        Audit::ImportKey(&digest, &digest, 1, 2, 3);
        Audit::CacheLookup(&digest, &digest, 1, 2, 3, false);
        Audit::External(10, 2, 40);
        pose.Stop(); pose.Stop();
    }
    Audit::Presented(); Audit::EndProcess();
    Check(Audit::events.empty() && Audit::frames.empty(), "disabled audit emitted rows");
    Check(Audit::totals.imports == 0 && Audit::totals.poses == 0 &&
        Audit::totals.fingerprints == 0 && Audit::totals.misses == 0 &&
        Audit::totals.cpu == 0 && Audit::totals.granny == 0, "disabled audit recorded work");
    Check(Audit::processSerial == 0 && Audit::displaySerial == 0 && !Audit::inProcess &&
        Audit::previousCpu == 0, "disabled audit changed frame state");
}

void AcrossUpdates()
{
    Reset(); Audit::Enable();
    Check(Audit::frames.capacity() >= Audit::frameLimit &&
        Audit::events.capacity() >= Audit::eventLimit, "capture buffers were not reserved");
    Audit::origin -= 100ms;
    Audit::previousEnd -= 30ms; Audit::lastDisplay -= 30ms;
    auto model = Digest(3), animation = Digest(97);
    const auto originalModel = model, originalAnimation = animation;
    constexpr std::uint64_t binding = 0xfedcba9876543210ULL;
    Audit::BeginProcess(true, false);
    Audit::CacheLookup(&model, &animation, 17, 29, binding, false);
    Audit::ImportStarted(); Audit::ImportKey(&model, &animation, 17, 29, binding);
    model.back() ^= 0x80; animation.back() ^= 0x40;
    Check(Audit::events[0].model == originalModel && Audit::events[0].animation == originalAnimation &&
        Audit::events[1].model == originalModel && Audit::events[1].animation == originalAnimation,
        "content identity does not own every digest byte");
    Check(Audit::events[0].modelIndex == 17 && Audit::events[0].animationIndex == 29 &&
        Audit::events[0].binding == binding && Audit::events[0].process == 1 &&
        Audit::events[0].display == 1 && Audit::events[0].kind == 'M' &&
        Audit::events[1].kind == 'I', "content identity or attribution was truncated");
    Audit::InstanceHit(); Audit::ReferencePose();
    {
        Audit::WorkScope pose(Audit::Work::Pose);
        { Audit::WorkScope palette(Audit::Work::Palette); }
        pose.Stop();
        const auto stopped = Audit::totals.poseUs;
        pose.Stop();
        Check(Audit::totals.poseUs == stopped, "explicit Stop double-counted work");
    }
    Check(Audit::totals.poses == 1 && Audit::totals.paletteUs >= 0 &&
        Audit::totals.poseUs >= Audit::totals.paletteUs, "nested palette/pose accounting is invalid");
    for (auto work : {Audit::Work::Fingerprint, Audit::Work::Import, Audit::Work::Submission,
        Audit::Work::Presentation, Audit::Work::PresentWait, Audit::Work::Update, Audit::Work::Sleep}) {
        Audit::WorkScope scope(work);
        scope.Stop();
    }
    Audit::External(2, 1, 8);
    Audit::EndProcess();
    Check(!Audit::inProcess && Audit::processSerial == 1 && Audit::displaySerial == 0 &&
        Audit::displayUpdates == 1 && Audit::displayStats.imports == 1 &&
        Audit::displayStats.poses == 1, "unpresented update discarded display work");
    Check(Audit::displayStats.outsideUs >= 30000, "outside-process gap was lost");

    Audit::BeginProcess(true, false);
    Check(Audit::processStats.imports == 0 && Audit::processStats.poses == 0,
        "new process did not reset process-only counters");
    Audit::ImportStarted();
    { Audit::WorkScope pose(Audit::Work::Pose); }
    Audit::CacheLookup(&model, &animation, 17, 29, binding, true);
    Check(Audit::events.back().model != originalModel && Audit::events.back().animation != originalAnimation,
        "changed final digest bytes were lost");
    Audit::External(5, 3, 20);
    Check(Audit::processStats.cpu == 3 && Audit::processStats.fallbacks == 2 &&
        Audit::processStats.gpuPrepareUs == 12, "external cumulative counters were counted twice");
    Audit::lastDisplay -= 30ms; // Guarantee a logged display interval without sleeping.
    Audit::Presented();
    const auto row = DisplayRow();
    Check(row.process == 2 && row.display == 1 && row.updates == 2 && row.presented &&
        row.game && !row.minimized && !row.inactive && row.frameUs > 20000,
        "display row did not aggregate the two updates");
    Check(row.stats.imports == 2 && row.stats.poses == 2 && row.stats.hits == 1 &&
        row.stats.misses == 1 && row.stats.fingerprints == 1 && row.stats.instanceHits == 1 &&
        row.stats.granny == 1 && row.stats.cpu == 5 && row.stats.fallbacks == 3 &&
        row.stats.gpuPrepareUs == 20, "display counters disagree with accumulated work");
    Check(Audit::totals.imports == 2 && Audit::totals.poses == 2 &&
        Audit::totals.cpu == 5 && Audit::displayStats.imports == 0 && Audit::displayUpdates == 0,
        "presentation duplicated totals or failed to clear interval state");
    Check(Audit::peakGameDisplayUs == row.frameUs, "active game peak excluded a valid display");
    Audit::EndProcess();
    Audit::CacheLookup(nullptr, &animation, 31, 47, binding, false);
    Check(Audit::events.back().kind == 'U' && !Audit::events.back().keyed &&
        Audit::events.back().process == 0 && Audit::events.back().display == 2 &&
        Audit::totals.unkeyed == 1, "unkeyed work outside Process has incorrect attribution");
}

void ProcessAndInactiveRows()
{
    Reset(); Audit::Enable();
    Audit::BeginProcess(false, true, false);
    Audit::processStart -= 25ms; Audit::accountedUntil -= 25ms;
    Audit::ImportStarted(); Audit::EndProcess();
    Check(Audit::frames.size() == 1 && !Audit::frames[0].displayRow &&
        !Audit::frames[0].presented && Audit::frames[0].inactive &&
        Audit::frames[0].stats.imports == 1 && Audit::frames[0].frameUs > 20000,
        "slow unpresented process row is incorrect");
    Audit::BeginProcess(true, false, true);
    Audit::lastDisplay -= 30ms;
    Audit::Presented();
    const auto row = DisplayRow();
    Check(row.game && row.minimized && row.inactive && row.updates == 2,
        "display interval lost earlier minimized/inactive state");
    Check(Audit::peakDisplayUs > 20000 && Audit::peakGameDisplayUs == 0,
        "inactive/minimized interval contaminated active game peak");
    Audit::EndProcess();
}

std::vector<std::string> Columns(const std::string& line)
{
    std::vector<std::string> columns;
    std::istringstream input(line);
    for (std::string column; std::getline(input, column, ',');) columns.push_back(column);
    return columns;
}

std::string Hex(const Audit::Digest& digest)
{
    std::ostringstream out;
    for (auto byte : digest) out << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
    return out.str();
}

void Csv()
{
    Reset(); Audit::Enable();
    const auto model = Digest(3), animation = Digest(97);
    Audit::BeginProcess(true, false);
    Audit::ImportStarted(); Audit::ImportKey(&model, &animation, 17, 29, 123456789012345ULL);
    Audit::CacheLookup(nullptr, nullptr, 31, 47, 987654321, false);
    Audit::lastDisplay -= 30ms; Audit::Presented(); Audit::EndProcess();
    struct DirectoryRestore {
        std::filesystem::path path = std::filesystem::current_path();
        ~DirectoryRestore() { std::error_code error; std::filesystem::current_path(path, error); }
    } restore;
    const auto directory = restore.path / "animation-stall-audit-test";
    std::filesystem::create_directories(directory);
    std::filesystem::current_path(directory);
    Audit::Write();
    std::ifstream frames("animation-stalls.csv"), content("animation-stall-content.csv"),
        summary("animation-stall-summary.txt");
    Check(frames.good() && content.good() && summary.good(), "audit output files were not written");
    std::string header, line;
    std::getline(frames, header); std::getline(frames, line);
    const auto frameColumns = Columns(header);
    Check(frameColumns.size() == Columns(line).size() && frameColumns.size() == 34,
        "frame CSV column count differs from its header");
    Check(std::find(frameColumns.begin(), frameColumns.end(), "inactive") != frameColumns.end() &&
        std::find(frameColumns.begin(), frameColumns.end(), "present_wait_ms") != frameColumns.end() &&
        std::find(frameColumns.begin(), frameColumns.end(), "outside_process_ms") != frameColumns.end(),
        "frame CSV is missing attribution columns");
    std::getline(content, header); std::getline(content, line);
    auto columns = Columns(line);
    Check(Columns(header).size() == 10 && columns.size() == 10 && columns[4] == "1" &&
        columns[5] == Hex(model) && columns[6] == Hex(animation) &&
        columns[7] == "17" && columns[8] == "29" && columns[9] == "123456789012345",
        "content CSV lost full identities or decimal indexes/binding");
    std::getline(content, line); columns = Columns(line);
    Check(columns.size() == 10 && columns[0] == "U" && columns[1] == "1" && columns[4] == "0" &&
        columns[5].empty() && columns[6].empty() && columns[7] == "31" && columns[9] == "987654321",
        "unkeyed CSV row or numeric formatting after a digest is incorrect");
    std::getline(summary, line);
    Check(line.find("Processes=1 PresentedFrames=1") != std::string::npos &&
        line.find("Imports=1") != std::string::npos, "summary does not reflect captured counters");
}

void Bounds()
{
    Reset(); Audit::Enable();
    Audit::events.resize(Audit::eventLimit);
    const auto eventCapacity = Audit::events.capacity();
    Audit::CacheLookup(nullptr, nullptr, 0, 0, 0, false);
    Check(Audit::events.size() == Audit::eventLimit && Audit::events.capacity() == eventCapacity &&
        Audit::droppedEvents == 1 && Audit::totals.misses == 1,
        "full event capture grew or stopped aggregate counters");
    Audit::frames.resize(Audit::frameLimit);
    const auto frameCapacity = Audit::frames.capacity();
    Audit::Frame row; row.frameUs = 20000;
    Audit::Save(row);
    Check(Audit::droppedFrames == 0, "threshold is not strictly greater than 20 ms");
    row.frameUs = 20001; Audit::Save(row);
    Check(Audit::frames.size() == Audit::frameLimit && Audit::frames.capacity() == frameCapacity &&
        Audit::droppedFrames == 1, "full frame capture grew or failed to report a dropped row");
}
}

int main()
{
    try {
        Disabled(); AcrossUpdates(); ProcessAndInactiveRows(); Csv(); Bounds(); Reset();
        std::cout << "Animation stall audit contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
