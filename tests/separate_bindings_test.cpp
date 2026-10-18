#include "nvn/nvn.h"

#include <stdexcept>

namespace {
auto& functions() { static std::unordered_map<std::string, nx::HleFn> map; return map; }
auto& commands() { static std::vector<nx::nvn::Cmd> list; return list; }
}

namespace nx {
HleRegistrar::HleRegistrar(const char* name, HleFn function) { functions()[name] = function; }
[[noreturn]] void fatal(const char*, ...) { throw std::invalid_argument("Entrada NVN rechazada"); }
}
namespace nx::nvn {
void record(uint64_t, Cmd command, const void*, size_t) { commands().push_back(command); }
}

int main() {
    using nx::nvn::Cmd;
    nx::CPU cpu{};
    auto call = [&](const char* name) { functions().at(name)(cpu); };
    cpu.x[1] = 0xABCDE;
    call("nvnDeviceGetSeparateTextureHandle");
    if (cpu.x[0] != 0x1000ABCDE) return 1;
    cpu.x[1] = 0x123;
    call("nvnDeviceGetSeparateSamplerHandle");
    if (cpu.x[0] != 0x12300000) return 2;
    cpu.x[1] = 1; cpu.x[2] = 3; cpu.x[3] = 0xABC;
    call("nvnCommandBufferBindSeparateTexture");
    if (commands().size() != 1 || commands()[0].op != Cmd::BindSeparateTexture ||
        commands()[0].i[0] != 1 || commands()[0].i[1] != 3 || commands()[0].a[0] != 0xABC) return 3;
    const uint64_t handles[] = {111, 222};
    cpu.x[3] = 2; cpu.x[4] = reinterpret_cast<uint64_t>(handles);
    call("nvnCommandBufferBindSeparateSamplers");
    if (commands().size() != 3 || commands()[2].op != Cmd::BindSeparateSampler ||
        commands()[2].i[1] != 4 || commands()[2].a[0] != 222) return 4;
    call("nvnCommandBufferBindSeparateTexturesDeferred");
    if (commands().size() != 4 || commands()[3].op != Cmd::BindDeferred || commands()[3].mask != 4 ||
        commands()[3].n != 2 || commands()[3].a[0] != cpu.x[4]) return 5;
    cpu.x[2] = 127; cpu.x[3] = 2;
    try { call("nvnCommandBufferBindSeparateTextures"); return 6; }
    catch (const std::invalid_argument&) {}
    if (commands().size() != 4) return 7;
    std::puts("ABI NVN: handles, comandos individuales, arrays, diferidos y límites: PASS");
}
