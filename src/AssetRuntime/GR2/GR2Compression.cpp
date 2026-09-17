// Oodle1 arithmetic/LZ model adapted from Arbos/nwn2mdk gr2_decompress.cpp,
// derived from berenm/xoreos-tools. Boost Software License 1.0; see
// LICENSE-Boost.txt. ZiiNAN changes: checked spans, explicit little-endian
// parameters, offset-based output, bounded models and malformed-stream errors.
#include "GR2File.h"
#include "EterBase/MapLoadTrace.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include <algorithm>
#include <numeric>
#if defined(_M_X64) || defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace AssetRuntime::GR2
{
namespace
{
// The coarse CDF gives short sorted intervals. Compare four probabilities at
// once on SSE2 (baseline on x64), preserving exact upper_bound semantics.
// Never load past last, including empty intervals and the 16384 sentinel.
auto SymbolUpperBound(std::vector<std::uint32_t>::const_iterator first,
    std::vector<std::uint32_t>::const_iterator last, std::uint32_t value)
{
#if defined(_M_X64) || defined(__SSE2__)
    // Keep logarithmic work for unusually wide intervals as well as malformed
    // inputs. The measured production intervals are almost all <=16 entries.
    if(last-first>16)return std::upper_bound(first,last,value);
    // All CDF entries are validated in [0,16384], so signed comparison is exact.
    const auto limit=_mm_set1_epi32(static_cast<int>(value+1));
    while(last-first>=4) {
        const auto cdf=_mm_loadu_si128(reinterpret_cast<const __m128i*>(&*first));
        const auto mask=static_cast<unsigned>(_mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(limit,cdf))));
        if(mask!=15)return first+std::countr_one(mask);
        first+=4;
    }
    while(first!=last && *first<=value)++first;
    return first;
#else
    return std::upper_bound(first,last,value);
#endif
}
struct Decoder
{
    std::span<const std::byte> input;
    std::size_t position{};
    std::uint32_t numerator{}, denominator=128, next{};
    explicit Decoder(std::span<const std::byte> bytes) : input(bytes)
    {
        Require(!bytes.empty(), "missing arithmetic stream"); numerator=Byte(0)>>1;
    }
    std::uint32_t Byte(std::size_t index) const
    {
        // Arithmetic decoding has at most one zero-filled terminal lookahead
        // word. It is local padding, never an out-of-range access to the file.
        Require(index < input.size()+4, "truncated arithmetic stream");
        return index<input.size()?std::to_integer<std::uint32_t>(input[index]):0;
    }
    std::uint32_t Decode(std::uint32_t maximum)
    {
        Require(maximum>0 && maximum<=16384 && denominator>0, "invalid arithmetic interval");
        while(denominator<=0x800000) {
            numerator=(numerator<<8)|((Byte(position)<<7)&128)|(Byte(position+1)>>1);
            ++position; denominator<<=8;
        }
        next=denominator/maximum;
        return std::min(numerator/next, maximum-1);
    }
    std::uint32_t Commit(std::uint32_t maximum, std::uint32_t value, std::uint32_t width)
    {
        Require(width>0 && value<maximum && width<=maximum-value, "empty arithmetic interval");
        numerator-=next*value;
        denominator=value+width<maximum?next*width:denominator-next*value;
        return value;
    }
    std::uint32_t Value(std::uint32_t maximum) { const auto v=Decode(maximum); return Commit(maximum,v,1); }
};
struct Window
{
    std::vector<std::uint32_t> ranges{0,16384}, values{0}, weights{4};
    // Coarse inverse CDF, refreshed only with ranges. Small models keep their
    // original search; larger models search only the exact enclosing interval.
    std::array<std::uint16_t,65> search{};
    std::uint32_t total=4, increase=4, rebuild=8, weightLimit{}, increaseLimit{}, capacity{};
    Window(std::uint32_t maximum, std::uint32_t count) : capacity(count+1)
    {
        Require(count<=8192 && maximum<=8192, "Oodle1 model allocation limit");
        weightLimit=std::max(256u,std::min(32*maximum,15160u));
        increaseLimit=maximum>64?std::min(2*maximum,weightLimit/2-32):128;
    }
    void Rescale()
    {
        total=0; for(auto& weight:weights) { weight/=2; total+=weight; }
        for(std::size_t i=1;i<weights.size();++i) while(i<weights.size() && weights[i]==0) {
            weights[i]=weights.back(); values[i]=values.back(); weights.pop_back(); values.pop_back();
        }
        const auto best=std::max_element(weights.begin()+1,weights.end());
        if(best!=weights.end()) { const auto i=static_cast<std::size_t>(best-weights.begin()); std::swap(weights[i],weights.back()); std::swap(values[i],values.back()); }
        if(weights.size()<capacity && !weights[0]) { weights[0]=1; ++total; }
    }
    void Rebuild()
    {
        Require(total>0, "empty Oodle1 model");
        ranges.resize(weights.size()+1);
        const auto factor=8*16384/total;
        std::uint32_t start=0;
        for(std::size_t i=0;i<weights.size();++i) { ranges[i]=start; start+=weights[i]*factor/8; }
        Require(start<=16384, "Oodle1 model range overflow"); ranges.back()=16384;
        if(ranges.size()>=16) {
            std::size_t cursor=0;
            for(unsigned bucket=0;bucket<search.size();++bucket) {
                while(cursor<ranges.size() && ranges[cursor]<=bucket*256u) ++cursor;
                search[bucket]=static_cast<std::uint16_t>(cursor);
            }
        }
        if(increase>increaseLimit/2) rebuild=total+increaseLimit;
        else { increase*=2; rebuild=total+increase; }
    }
    std::uint32_t Read(Decoder& decoder, std::uint32_t maximum)
    {
        if(total>=rebuild) { if(rebuild>=weightLimit) Rescale(); Rebuild(); }
        const auto value=decoder.Decode(16384);
        const auto bucket=value>>8;
        const auto first=ranges.size()>=16?ranges.begin()+search[bucket]:ranges.begin();
        const auto last=ranges.size()>=16?ranges.begin()+search[bucket+1]:ranges.end();
        const auto upper=SymbolUpperBound(first,last,value);
        Require(upper!=ranges.begin() && upper!=ranges.end(), "invalid Oodle1 model range");
        const auto index=static_cast<std::size_t>(upper-ranges.begin()-1);
        Require(index<weights.size(), "invalid Oodle1 symbol index");
        decoder.Commit(16384,ranges[index],ranges[index+1]-ranges[index]); ++weights[index]; ++total;
        if(index) return values[index];
        if(weights.size()>=ranges.size() && decoder.Value(2)==1) {
            const auto at=ranges.size()+decoder.Value(static_cast<std::uint32_t>(weights.size()-ranges.size()+1))-1;
            Require(at<weights.size(), "invalid escaped Oodle1 symbol"); weights[at]+=2; total+=2; return values[at];
        }
        Require(weights.size()<capacity, "Oodle1 model capacity exceeded");
        const auto result=decoder.Value(maximum);
        values.push_back(result); weights.push_back(2); total+=2;
        if(weights.size()==capacity) { total-=weights[0]; weights[0]=0; }
        return result;
    }
};
void DecodeBlock(std::span<const std::byte> header, Decoder& decoder,
    std::vector<std::byte>& output, std::size_t& position, std::size_t stop)
{
    const auto word0=U32(header,0), word1=U32(header,4), word2=U32(header,8);
    const auto byteMaximum=word0&511, offsetMaximum=word0>>9, byteCount=word1&511, highCount=word1>>19;
    // Bits 9..18 of word1 are encoder padding, not flags. Real exporters leave
    // these bits set; they do not participate in the coding model.
    if(!(byteMaximum>0 && byteMaximum<=256 && byteCount<=byteMaximum))
        Bad("invalid Oodle1 parameters max="+std::to_string(byteMaximum)+" count="+std::to_string(byteCount)+" reserved="+std::to_string(word1&0x7fe00));
    const auto lowMaximum=std::min(offsetMaximum+1,4u), midMaximum=std::min(offsetMaximum/4+1,256u), highMaximum=offsetMaximum/1024+1;
    Require(highCount<highMaximum, "invalid Oodle1 offset model");
    MapLoadTrace::Scope setupTrace("Assets","GR2 decoder model setup");
    Window low(lowMaximum-1,lowMaximum), high(highMaximum-1,highCount+1);
    std::vector<Window> mid, literal, length;
    mid.reserve(highMaximum); for(std::uint32_t i=0;i<highMaximum;++i) mid.emplace_back(midMaximum-1,midMaximum);
    for(unsigned i=0;i<4;++i) literal.emplace_back(byteMaximum-1,byteCount);
    for(unsigned i=0;i<65;++i) {
        const auto count=(word2 >> ((3-std::min(i/16,3u))*8))&255;
        Require(count<=65,"invalid Oodle1 length model"); length.emplace_back(64,count);
    }
    setupTrace.Stop();
    MapLoadTrace::Scope loopTrace("Assets","GR2 decoder loop");
    const auto start=position;
    std::uint32_t previous=0;
    while(position<stop) {
        previous=length[previous].Read(decoder,65); Require(previous<=64,"invalid Oodle1 length");
        if(!previous) {
            const auto v=literal[position%4].Read(decoder,byteMaximum); Require(v<256,"invalid Oodle1 literal");
            output[position++]=std::byte(v);
        } else {
            constexpr std::array<std::size_t,4> longLengths{128,192,256,512};
            const auto count=previous<61?previous+1:longLengths[previous-61];
            const auto available=std::min<std::size_t>(offsetMaximum,position-start);
            const auto lo=low.Read(decoder,lowMaximum), hi=high.Read(decoder,static_cast<std::uint32_t>(available/1024+1));
            Require(hi<mid.size(),"invalid Oodle1 high distance");
            const auto mi=mid[hi].Read(decoder,static_cast<std::uint32_t>(std::min<std::size_t>(available/4+1,256)));
            const auto distance=std::size_t(hi)*1024+mi*4+lo+1;
            Require(distance<=position-start && count<=stop-position,"invalid Oodle1 backreference");
            for(std::size_t i=0;i<count;++i) { output[position]=output[position-distance]; ++position; }
        }
    }
}
}
std::vector<std::byte> Decompress(const Section& section, std::span<const std::byte> bytes)
{
    MapLoadTrace::Scope p0lScope("Assets","GR2 section decompression","cpu");

    AnimationStallAudit::WorkScope audit(AnimationStallAudit::Work::Decompress,section.compression!=0 && section.expanded!=0);
    if(section.compression!=0 && section.compression!=2)
        throw Error(Failure::UnsupportedCompression,"unsupported compression "+std::to_string(section.compression));
    Require(section.expanded<=MaximumExpandedBytes && bytes.size()==section.compressed,"section allocation/size limit");
    if(!section.compression) { Require(bytes.size()==section.expanded,"raw size mismatch"); return {bytes.begin(),bytes.end()}; }
    if(!section.expanded) { Require(bytes.empty(),"empty compressed section mismatch"); return {}; }
    Require(bytes.size()>=37,"truncated Oodle1 header");
    Require(section.stop0<=section.stop1 && section.stop1<=section.expanded,"invalid Oodle1 stops");
    MapLoadTrace::Scope allocateTrace("Assets","GR2 output allocation");
    std::vector<std::byte> result(section.expanded);
    allocateTrace.Stop();
    Decoder decoder(bytes.subspan(36));
    const std::array<std::size_t,3> stops{section.stop0,section.stop1,section.expanded};
    std::size_t position=0;
    for(unsigned i=0;i<3;++i) if(position<stops[i]) DecodeBlock(bytes.subspan(i*12,12),decoder,result,position,stops[i]);
    return result;
}
}
