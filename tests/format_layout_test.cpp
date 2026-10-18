#include "nvn/nvn.h"
#include <stdexcept>

namespace nx {
HleRegistrar::HleRegistrar(const char*, HleFn) {}
[[noreturn]] void fatal(const char*, ...) { throw std::invalid_argument("Formato NVN rechazado"); }
}
namespace nx::nvn {
uint64_t real_proc(CPU&, const char*) { throw std::logic_error("La geometría no debe consultar el SDK"); }
uint64_t guest_call(CPU&, uint64_t, std::initializer_list<uint64_t>) { return 0; }
}
int main() {
    struct Case { uint32_t format, width, height, bytes; };
    const Case cases[] = {
        {1, 1, 1, 1}, {5, 1, 1, 2}, {0x25, 1, 1, 4}, {0x2e, 1, 1, 16},
        {0x35, 1, 1, 4}, {0x36, 1, 1, 8}, {0x3f, 1, 1, 4},
        {0x41, 4, 4, 8}, {0x43, 4, 4, 16}, {0x79, 4, 4, 16},
        {0x7a, 5, 4, 16}, {0x86, 12, 12, 16}, {0x94, 12, 12, 16}, {0x9c, 1, 1, 4},
    };
    for (const auto& expected : cases) {
        uint32_t width = 0, height = 0, bytes = 0;
        nx::nvn::format_block(expected.format, width, height, bytes);
        if (width != expected.width || height != expected.height || bytes != expected.bytes) return 1;
    }
    for (uint32_t format : {0x9dU, 0xd0U, 0xffffffffU}) {
        uint32_t width, height, bytes;
        try { nx::nvn::format_block(format, width, height, bytes); return 2; }
        catch (const std::invalid_argument&) {}
    }
    std::puts("Bloques NVN: tamaños originales, BC/ASTC, conversiones del host y límites: PASS");
}
