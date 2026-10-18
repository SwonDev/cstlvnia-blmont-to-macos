#include <nx/game.h>
#include <nx/runtime.h>
#include "kernel.h"

#include <cstdio>

namespace nx {
const game::Info game::kInfo = {"fixture", "Fixture", 1, "fixture", ".", 0x1000000};
uint64_t heap_size() { return 0x10000; }
uint64_t available_threads = 27;
uint64_t free_thread_count() { return available_threads; }
uint64_t kernel_alloc(uint64_t, uint64_t) { return kKernelBase; }
void log(const char*, ...) {}
}

int main() {
    uint32_t result = 0;
    const auto extra = nx::get_info(28, 0xFFFF8001, 0, result);
    if (result != 0 || extra != 0) return 1;
    if (nx::get_info(2, 0xFFFF8001, 0, result) != nx::kAliasBase || result) return 2;
    if (nx::get_info(3, 0xFFFF8001, 0, result) != nx::kAliasSize || result) return 3;
    nx::get_info(28, 0xFFFF8001, 1, result);
    if (result != 0xE801) return 4;
    nx::get_info(0xFFFF, 0xFFFF8001, 0, result);
    if (result != 0xF001) return 5;
    if (nx::get_info(24, 0xFFFF8001, 0, result) != 27 || result) return 6;
    nx::available_threads = 0;
    if (nx::get_info(24, 0xFFFF8001, 0, result) != 0 || result) return 7;
    nx::get_info(24, 0xFFFF8001, 1, result);
    if (result != 0xE801) return 8;
    std::puts("GetInfo: reserva alias real, subíndice inválido y consulta desconocida: PASS");
}
