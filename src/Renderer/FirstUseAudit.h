#pragma once
#include "EterBase/MapLoadTrace.h"
#include "Diagnostics.h"
#include "ShaderLoadAuditContext.h"
#include <chrono>

namespace Renderer
{
inline bool loadingPrewarm{};
inline bool worldPrewarmPending{};
inline bool awaitingWorldPresent{};
// Creation-only diagnostics. Nested FX/IBL totals include their shader/PSO
// events; they must not be summed with those child events.
class FirstUseAudit
{
    ShaderLoadAudit::Domain domain_;
    MapLoadTrace::Scope p0lScope_;
    const char* category_;
    const char* name_;
    bool enabled_;
    std::chrono::steady_clock::time_point start_;
public:
    FirstUseAudit(const char* category,const char* name,bool enabled=true):
        domain_(std::string_view(category)=="fx-total"?name:ShaderLoadAudit::domain),
        p0lScope_("Shaders / PSOs",name,category,enabled), category_(category),name_(name),enabled_(enabled&&verboseDiagnostics),
        start_(enabled_?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{}) {}
    ~FirstUseAudit() noexcept {
        if(!enabled_)return;
        try {
            const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start_).count();
            std::ofstream log("gdxc-first-use.log",std::ios::app);
            const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            log<<"timeMs="<<now<<" phase="<<(loadingPrewarm?"loading":"runtime")<<" category="<<category_<<" name="<<name_<<" ms="<<elapsed<<'\n';
        }catch(...){}
    }
};
inline void LogClientLifecycle(const char* phase) noexcept {
    if(!verboseDiagnostics)return;
    try {
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        std::ofstream log("gdxc-lifecycle.log",std::ios::app);
        log<<"timeMs="<<now<<" phase="<<phase<<'\n';
    }catch(...){}
}
}
