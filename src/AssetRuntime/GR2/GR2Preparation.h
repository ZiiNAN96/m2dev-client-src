#pragma once
#include "GR2Reader.h"
#include <functional>
#include <future>
#include <memory>

namespace AssetRuntime::GR2 {
// Synchronous owner-thread batch. Only File/Read run in the supplied existing
// executor; payload acquisition and publication remain on the caller.
class Preparation final {
public:
    using Executor = std::function<std::future<void>(std::function<void()>)>;
    using Loader = std::function<std::vector<std::byte>()>;
    struct Result { Contents contents; std::exception_ptr error; };
    Preparation();
    ~Preparation();
    Preparation(const Preparation&) = delete;
    Preparation& operator=(const Preparation&) = delete;
    bool Enabled() const;
    void Add(std::string id, const Loader& load);
    void Run(unsigned workers, const Executor& execute, const std::function<std::uint64_t()>& cpuClock = {});
    static std::span<const std::byte> Payload(std::string_view id);
    static std::unique_ptr<Result> Take(std::string_view id);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
