#include <nx/runtime.h>
#include "../games/castlevania/movie.h"
#include "../games/castlevania/yuv.h"
#undef NDEBUG
#include <cassert>
#include <map>
#include <string>
#include <vector>
namespace {
auto& registry() { static std::map<std::string, nx::HleFn> r; return r; }
constexpr auto symbol = "_ZN7android8OMXCodec18findMatchingCodecsEPKcbS2_jPNS_6VectorINS_7String8EEE";
constexpr auto ctor = "_ZN7android7String8C1EPKc";
constexpr auto dtor = "_ZN7android7String8D1Ev";
constexpr auto add = "_ZN7android10VectorImpl3addEPKv";
struct GuestVector { uint64_t vptr=0, storage=0, size=0; };
GuestVector result;
uint64_t flags, component;
bool original_has_candidate=false;
std::vector<std::string> added;
unsigned destroyed=0, converter_fallbacks=0;
constexpr auto converter = "_ZN5movie11DecoderImpl26ConvertDecoderOutputFormatEPhiiiiS1_PiNS_19DecoderOutputFormatEm";
uint64_t frames=0;
}
namespace nx {
HleRegistrar::HleRegistrar(const char* name, HleFn fn) { registry()[name] = fn; }
const char* hle_current_name() { return symbol; }
const LoadedModule* module_at(uint64_t) { return nullptr; }
uint64_t find_symbol(const char* name) {
    if (std::string(name) == converter) return 47;
    if (std::string(name) == symbol) return 42;
    if (std::string(name) == ctor) return 43;
    if (std::string(name) == dtor) return 44;
    assert(std::string(name) == add); return 45;
}
void call(CPU& c, uint64_t target) {
    if (target==47) { converter_fallbacks++; c.x[0]=0xCAFE; }
    else if (target==46) c.x[0]=frames;
    else if (target==42) {
        flags=c.x[3]; component=c.x[2];
        assert(c.x[4]==reinterpret_cast<uint64_t>(&result));
        result.size=original_has_candidate ? 1 : 0;
        c.x[0]=0x1234; c.x[8]=0x5678;
    } else if (target==43) st<uint64_t>(c.x[0], c.x[1]);
    else if (target==44) { assert(ld<uint64_t>(c.x[0])); destroyed++; }
    else {
        assert(target==45 && c.x[0]==reinterpret_cast<uint64_t>(&result));
        added.emplace_back(reinterpret_cast<const char*>(ld<uint64_t>(c.x[1])));
        result.size++; c.x[0]=0;
    }
}
void log(const char*, ...) {}
[[noreturn]] void fatal(const char*, ...) { std::abort(); }
}
int main() {
    uint64_t vtable[45]{}; vtable[0x160/8]=46;
    uint64_t object=reinterpret_cast<uint64_t>(vtable);
    auto ready = [&] {
        nx::CPU c{}; c.x[0]=reinterpret_cast<uint64_t>(&object);
        nx::game::movie_readiness(c); return c.x[0]!=0;
    };
    for (frames=0;frames<5;frames++) assert(ready()==(frames>=3));
    for (const char* mime : {"video/avc", "video/hevc", "audio/mp4a-latm"})
        for (uint64_t encoder : {0,1,256})
            for (bool existing : {false,true}) {
                original_has_candidate=existing; added.clear(); destroyed=0;
                nx::CPU c{}; c.x[0]=reinterpret_cast<uint64_t>(mime); c.x[1]=encoder;
                c.x[3]=0x91; c.x[4]=reinterpret_cast<uint64_t>(&result);
                registry().at(symbol)(c);
                bool video=!(encoder&1) && mime[0]=='v';
                bool fallback=video && std::string(mime)=="video/avc" && !existing;
                assert(flags==(video ? 0x89 : 0x91));
                assert(added.size()==unsigned(fallback) && destroyed==unsigned(fallback));
                if (fallback) assert(added[0]=="OMX.google.h264.decoder");
                assert(c.x[0]==0x1234 && c.x[8]==0x5678);
            }
    for (const char* match : {"OMX.Nvidia.h264.decode","OMX.google.h264.decoder","OMX.other.decoder"}) {
        original_has_candidate=false; added.clear(); destroyed=0;
        nx::CPU c{}; c.x[0]=reinterpret_cast<uint64_t>("video/avc");
        c.x[2]=reinterpret_cast<uint64_t>(match); c.x[4]=reinterpret_cast<uint64_t>(&result);
        registry().at(symbol)(c);
        assert(flags==8);
        bool nvidia=std::string(match).starts_with("OMX.Nvidia.");
        assert(component==(nvidia ? 0 : reinterpret_cast<uint64_t>(match)));
        assert(added.size()==unsigned(nvidia || std::string(match)=="OMX.google.h264.decoder"));
    }
    for (frames=0;frames<5;frames++) assert(ready()==(frames>=1));
    std::vector<uint8_t> planar(48), nv12(48);
    for (unsigned i = 0; i < 32; i++) planar[i] = uint8_t(i);
    for (unsigned i = 0; i < 8; i++) { planar[32+i] = uint8_t(80+i); planar[40+i] = uint8_t(160+i); }
    assert(nx::game::i420_to_nv12(planar, nv12, 4, 4, 4, 8));
    for (unsigned i = 0; i < 16; i++) assert(nv12[i] == i);
    for (unsigned i = 0; i < 4; i++) assert(nv12[16+i*2] == 80+i && nv12[17+i*2] == 160+i);
    auto in_place = planar; assert(nx::game::i420_to_nv12(in_place, in_place, 4, 4, 4, 8) && std::equal(in_place.begin(), in_place.begin() + 24, nv12.begin()));
    assert(!nx::game::i420_to_nv12(planar, std::span<uint8_t>(nv12.data(), 23), 4, 4, 4, 8));
    assert(!nx::game::i420_to_nv12(planar, nv12, 5, 4, 4, 8));
    std::vector<uint8_t> source(3133440), target(source.size());
    std::fill(source.begin(), source.begin() + 2088960, 16);
    std::fill(source.begin() + 2088960, source.begin() + 2611200, 120);
    std::fill(source.begin() + 2611200, source.end(), 140);
    uint64_t stack[] = {4, source.size()}; uint32_t length = 0;
    nx::CPU convert{}; convert.x[1] = reinterpret_cast<uint64_t>(source.data());
    convert.x[2] = convert.x[4] = 1920; convert.x[3] = convert.x[5] = 1080;
    convert.x[6] = reinterpret_cast<uint64_t>(target.data()); convert.x[7] = reinterpret_cast<uint64_t>(&length);
    convert.x[31] = reinterpret_cast<uint64_t>(stack);
    registry().at("_ZN5movie11DecoderImpl26ConvertDecoderOutputFormatEPhiiiiS1_PiNS_19DecoderOutputFormatEm")(convert);
    assert(convert.x[0] == 0 && length == 3110400 && target[2073600] == 120 && target[2073601] == 140 &&
           target[3110398] == 120 && target[3110399] == 140);
    convert.x[2] = 1280; convert.x[8] = 123;
    registry().at(converter)(convert);
    assert(converter_fallbacks == 1 && convert.x[0] == 0xCAFE && convert.x[8] == 123);
    std::puts("AVC por software conserva su propiedad y prepara con imágenes reales sin esperar tres búferes: PASS");
}
