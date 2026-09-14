// ZiiNAN: Modern asset pipeline
#include <charconv>
#include <cstring>
#include <limits>

namespace
{
template<class T> T SafeInteger(const char* text)
{
    T value{};
    const auto end=text+std::strlen(text);
    const auto result=std::from_chars(text,end,value);
    return result.ec==std::errc{} && result.ptr==end ? value : T(-1);
}
double SafeNumber(const char* text)
{
    double value{};
    const auto end=text+std::strlen(text);
    const auto result=std::from_chars(text,end,value);
    return result.ec==std::errc{} && result.ptr==end ? value : std::numeric_limits<double>::quiet_NaN();
}
}
#define CGLTF_ATOI(text) SafeInteger<int>(text)
#define CGLTF_ATOLL(text) SafeInteger<long long>(text)
#define CGLTF_ATOF(text) SafeNumber(text)
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
