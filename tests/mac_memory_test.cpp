#include "platform.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    constexpr uint64_t base = 0xC00000000ull;
    const uint64_t page = getpagesize();
    const uint64_t size = page * 4;
    if (!nx::plat::reserve(base, size)) {
        std::fprintf(stderr, "No se pudo reservar el rango de módulos.\n");
        return 1;
    }
    auto* bytes = reinterpret_cast<unsigned char*>(base);
    if (!nx::plat::commit(base, page)) return 2;
    std::memset(bytes, 0xA5, page);
    nx::plat::decommit(base + page, page + 0x1000);
    if (nx::plat::reserve(base, size)) return 3;
    for (uint64_t i = 0; i < page; ++i) if (bytes[i] != 0xA5) return 4;
    nx::plat::decommit(base + 0x1000, 0x1000);
    for (uint64_t i = 0; i < page; ++i)
        if (bytes[i] != (i >= 0x1000 && i < 0x2000 ? 0 : 0xA5)) return 5;
    nx::plat::decommit(base, page);
    if (nx::plat::reserve(base, size)) return 6;
    if (!nx::plat::commit(base, page)) return 7;
    for (uint64_t i = 0; i < page; ++i) if (bytes[i] != 0) return 13;
    if (!nx::plat::commit(base, page * 2)) return 14;
    std::memset(bytes, 0xA5, page * 2);
    nx::plat::decommit(base + page - 0x1000, 0x2000);
    for (uint64_t i = 0; i < page * 2; ++i)
        if (bytes[i] != (i >= page - 0x1000 && i < page + 0x1000 ? 0 : 0xA5)) return 15;
    alignas(16384) static unsigned char foreign[16384];
    std::memset(foreign, 0x5A, sizeof foreign);
    if (nx::plat::commit(reinterpret_cast<uint64_t>(foreign), sizeof foreign)) return 16;
    try { nx::plat::decommit(reinterpret_cast<uint64_t>(foreign) + 0x1000, 0x1000); return 17; }
    catch (const std::runtime_error&) {}
    for (unsigned char value : foreign) if (value != 0x5A) return 18;
    if (munmap(bytes, size)) return 8;
    constexpr uint64_t ranges[][2] = {
        {0x400000000ull, 0x180000000ull},
        {0x600000000ull, 0x400000000ull},
        {0xA00000000ull, 0x100000000ull},
        {0xB00000000ull, 0x100000000ull},
        {0xC00000000ull, 0x30000000ull},
        {0xE00000000ull, 0x100000ull},
    };
    for (const auto& range : ranges) {
        if (!nx::plat::reserve(range[0], range[1])) return 9;
        if (!nx::plat::commit(range[0], page)) return 10;
        if (!nx::plat::commit(range[0] + range[1] - page, page)) return 11;
    }
    for (const auto& range : ranges)
        if (munmap(reinterpret_cast<void*>(range[0]), range[1])) return 12;
    std::puts("Reserva exacta, rechazo de solapamientos y páginas parciales: PASS");
}
