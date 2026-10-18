#include "platform.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <crt_externs.h>

namespace { bool reject_host_allocation = false; }

void* operator new(std::size_t size) {
    if (reject_host_allocation) throw std::bad_alloc();
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
int failure(int code, const char* condition) {
    std::fprintf(stderr, "Memoria Mac: condición %d (%s), página %ld bytes\n",
                 code, condition, static_cast<long>(getpagesize()));
    return code;
}

void describe_regions(uint64_t begin, uint64_t end) {
    for (unsigned n = 0; begin < end && n < 32; ++n) {
        mach_vm_address_t address = begin;
        mach_vm_size_t size = 0;
        natural_t depth = 0;
        vm_region_submap_info_data_64_t info{};
        kern_return_t status;
        do {
            mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
            status = mach_vm_region_recurse(mach_task_self(), &address, &size, &depth,
                reinterpret_cast<vm_region_recurse_info_t>(&info), &count);
            if (status != KERN_SUCCESS || !info.is_submap) break;
            ++depth;
            address = begin;
        } while (true);
        std::fprintf(stderr, "Región: estado=%d dirección=0x%llx tamaño=0x%llx protección=%d máxima=%d tag=%u\n",
                     status, static_cast<unsigned long long>(address), static_cast<unsigned long long>(size),
                     info.protection, info.max_protection, info.user_tag);
        if (status != KERN_SUCCESS || !size || address + size <= begin || address >= end) break;
        begin = address + size;
    }
}
}

int partial_pages() {
    constexpr uint64_t base = 0xC00000000ull;
    const uint64_t page = getpagesize();
    const uint64_t size = page * 4;
    if (!nx::plat::reserve(base, size)) {
        std::fprintf(stderr, "No se pudo reservar el rango de módulos.\n");
        return failure(1, "reserva inicial");
    }
    auto* bytes = reinterpret_cast<unsigned char*>(base);
    if (!nx::plat::commit(base, page)) return failure(2, "commit inicial");
    std::memset(bytes, 0xA5, page);
    nx::plat::decommit(base + page, page + 0x1000);
    if (nx::plat::reserve(base, size)) return failure(3, "rechazo de reserva solapada");
    for (uint64_t i = 0; i < page; ++i) if (bytes[i] != 0xA5) return failure(4, "vecino intacto tras liberar otra página");
    nx::plat::decommit(base + 0x1000, 0x1000);
    for (uint64_t i = 0; i < page; ++i)
        if (bytes[i] != (i >= 0x1000 && i < 0x2000 ? 0 : 0xA5)) return failure(5, "liberación parcial de 4 KiB");
    nx::plat::decommit(base, page);
    if (nx::plat::reserve(base, size)) return failure(6, "rechazo tras decommit de página completa");
    if (!nx::plat::commit(base, page)) return failure(7, "recommit de página completa");
    for (uint64_t i = 0; i < page; ++i) if (bytes[i] != 0) return failure(13, "ceros tras recommit");
    if (!nx::plat::commit(base, page * 2)) return failure(14, "commit de dos páginas");
    std::memset(bytes, 0xA5, page * 2);
    nx::plat::decommit(base + page - 0x1000, 0x2000);
    for (uint64_t i = 0; i < page * 2; ++i)
        if (bytes[i] != (i >= page - 0x1000 && i < page + 0x1000 ? 0 : 0xA5)) return failure(15, "liberación parcial entre páginas");
    alignas(16384) static unsigned char foreign[16384];
    std::memset(foreign, 0x5A, sizeof foreign);
    if (nx::plat::commit(reinterpret_cast<uint64_t>(foreign), sizeof foreign)) return failure(16, "rechazo de commit ajeno");
    try { nx::plat::decommit(reinterpret_cast<uint64_t>(foreign) + 0x1000, 0x1000); return failure(17, "rechazo de decommit ajeno"); }
    catch (const std::runtime_error&) {}
    for (unsigned char value : foreign) if (value != 0x5A) return failure(18, "memoria ajena intacta");
    if (munmap(bytes, size)) return failure(8, "munmap inicial");
    return 0;
}

int profile_ranges() {
    const uint64_t page = getpagesize();
    constexpr uint64_t ranges[][2] = {
        {0x400000000ull, 0x180000000ull},
        {0x600000000ull, 0x400000000ull},
        {0xA00000000ull, 0x100000000ull},
        {0xB00000000ull, 0x100000000ull},
        {0xC00000000ull, 0x30000000ull},
        {0xE00000000ull, 0x100000ull},
    };
    for (const auto& range : ranges) {
        bool reserved = false;
        reject_host_allocation = true;
        try { reserved = nx::plat::reserve(range[0], range[1]); }
        catch (const std::bad_alloc&) {
            reject_host_allocation = false;
            return failure(21, "la reserva de arranque pide memoria al allocator del host");
        }
        reject_host_allocation = false;
        if (!reserved) {
            std::fprintf(stderr, "Rango rechazado: 0x%llx + 0x%llx\n",
                         static_cast<unsigned long long>(range[0]), static_cast<unsigned long long>(range[1]));
            describe_regions(range[0], range[0] + range[1]);
            return failure(9, "reserva de rango del juego");
        }
        if (!nx::plat::commit(range[0], page)) return failure(10, "commit inicial del rango");
        if (!nx::plat::commit(range[0] + range[1] - page, page)) return failure(11, "commit final del rango");
    }
    for (const auto& range : ranges)
        if (munmap(reinterpret_cast<void*>(range[0]), range[1])) return failure(12, "munmap de rango");
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--profile-ranges") == 0) return profile_ranges();
    if (int code = partial_pages()) return code;
    // El arranque real reserva el perfil antes de provocar excepciones y asignaciones del test.
    char option[] = "--profile-ranges";
    char* arguments[] = {argv[0], option, nullptr};
    pid_t child = 0;
    const int error = posix_spawn(&child, argv[0], nullptr, nullptr, arguments, *_NSGetEnviron());
    if (error) return failure(19, "crear proceso limpio del perfil");
    int status = 0;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status))
        return failure(20, "terminar proceso del perfil");
    if (WEXITSTATUS(status)) return WEXITSTATUS(status);
    std::puts("Reserva exacta, rechazo de solapamientos y páginas parciales: PASS");
}
