#include "ipc.h"
#include "platform.h"
#include <array>
#include <map>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace { auto& functions() { static std::map<std::string, nx::HleFn> result; return result; } }
namespace nx {
HleRegistrar::HleRegistrar(const char* name, HleFn fn) { functions()[name] = fn; }
uint32_t handle_add(std::shared_ptr<KObject>) { return 42; }
void notify_all() {}
uint64_t find_symbol(const char*) { return 1; }
const LoadedModule* module_at(uint64_t) { return nullptr; }
void call(CPU&, uint64_t) {}
void log(const char*, ...) {}
[[noreturn]] void fatal(const char*, ...) { std::abort(); }
std::string describe(uint64_t) { return "fixture"; }
namespace plat {
bool audio_open(AudioDone) { return false; }
uint32_t audio_queued() { return 0; }
bool audio_submit(const void*, size_t, void*) { return false; }
FILE* fopen_utf8(const std::string&, const char*) { return nullptr; }
std::string writable_root() { return "/tmp"; }
uint64_t monotonic_ns() { return 0; }
void sleep_ns(int64_t) {}
}
}
int main() {
    alignas(8) std::array<uint8_t, 64> buffer{}, out{}, event{};
    std::array<int16_t, 32> samples{};
    const auto addr = [](const auto& value) { return reinterpret_cast<uint64_t>(value.data()); };
    nx::CPU c{}; c.x[0] = addr(buffer); c.x[1] = addr(samples); c.x[2] = sizeof(samples); c.x[3] = sizeof(samples) / 2;
    functions().at("_ZN2nn5audio21SetAudioOutBufferInfoEPNS0_14AudioOutBufferEPvmm")(c);
    if (nx::ld<uint64_t>(addr(buffer) + 8) != addr(samples) ||
        nx::ld<uint64_t>(addr(buffer) + 16) != sizeof(samples) ||
        nx::ld<uint64_t>(addr(buffer) + 24) != sizeof(samples) / 2) {
        std::fputs("AudioOutBuffer no coincide con los getters originales del SDK: FAIL\n", stderr); return 1;
    }
    c.x[0] = addr(buffer);
    functions().at("_ZN2nn5audio28GetAudioOutBufferDataPointerEPKNS0_14AudioOutBufferE")(c);
    if (c.x[0] != addr(samples)) return 2;
    std::array<uint32_t, 2> parameters{48000, 2};
    c.x[0] = addr(out); c.x[1] = addr(event); c.x[2] = addr(parameters);
    functions().at("_ZN2nn5audio19OpenDefaultAudioOutEPNS0_8AudioOutEPNS_2os11SystemEventERKNS0_17AudioOutParameterE")(c);
    if (c.x[0] || nx::ld<uint32_t>(addr(out)) != 48000 || nx::ld<uint32_t>(addr(out) + 4) != 2 ||
        nx::ld<uint32_t>(addr(out) + 8) != 2) {
        std::fputs("AudioOut no declara 48000 Hz, estéreo, PCM16 al SDK: FAIL\n", stderr); std::_Exit(3);
    }
    std::puts("ABI AudioOut y búferes originales: 48000 Hz, estéreo PCM16: PASS");
    std::fflush(stdout);
    // El reproductor del runtime vive hasta salir del proceso; no destruir sus colas con el hilo aún esperando.
    std::_Exit(0);
}
