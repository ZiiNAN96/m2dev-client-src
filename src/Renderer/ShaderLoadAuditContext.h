#pragma once
// No backend types: lifecycle callers can name the active load subsystem.
#include "EterBase/MapLoadTrace.h"
namespace ShaderLoadAudit {
inline thread_local const char* domain="unclassified";
class Domain {
    const char* previous_;
public:
    explicit Domain(const char* value):previous_(domain){domain=value;}
    ~Domain(){domain=previous_;}
};
inline std::string Name(const char* value) { return MapLoadTrace::state.active?std::string(domain)+" | "+(value?value:""):std::string{}; }
}
