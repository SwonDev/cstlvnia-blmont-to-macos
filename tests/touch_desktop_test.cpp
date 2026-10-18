#include <nx/runtime.h>
#include "platform.h"
#include <array>
#include <map>
#include <string>

namespace {
auto& functions() { static std::map<std::string, nx::HleFn> r; return r; }
}
namespace nx {
std::atomic<uint64_t> g_frame{0};
HleRegistrar::HleRegistrar(const char* name, HleFn fn) { functions()[name] = fn; }
void log(const char*, ...) {}
const char* hle_current_name() { return "fixture"; }
std::string describe(uint64_t) { return "fixture"; }
uint64_t find_symbol(const char*) { return 0; }
void call(CPU&, uint64_t) {}
namespace plat { void read_pad(PadState&) {} }
}
int main() {
    nx::CPU c{};
    functions().at("_ZN2nn3hid21InitializeTouchScreenEv")(c);
    auto read = functions().at("_ZN2nn3hid20GetTouchScreenStatesILm16EEEiPNS0_16TouchScreenStateIXT_EEEi");
    alignas(8) std::array<uint8_t, 0x2a0> out;
    out.fill(0xAB);
    c.x[0] = reinterpret_cast<uint64_t>(out.data()); c.x[1] = 0; read(c);
    if (c.x[0] || out[0] != 0xAB) return 1;
    c.x[0] = reinterpret_cast<uint64_t>(out.data()); c.x[1] = 16; read(c);
    if (c.x[0] != 1 || nx::ld<int64_t>(reinterpret_cast<uint64_t>(out.data())) <= 0) return 2;
    for (size_t i = 8; i < 0x290; ++i) if (out[i]) return 3;
    for (size_t i = 0x290; i < out.size(); ++i) if (out[i] != 0xAB) return 4;
    c.x[0] = 123;
    functions().at("_ZN2nn3hid20IsVibrationPermittedEv")(c);
    if (c.x[0] != 0) return 5;
    std::puts("Pantalla de sobremesa: muestra sin contactos y buffer delimitado: PASS");
}
