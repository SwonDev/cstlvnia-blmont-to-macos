#include "platform.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mutex>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

namespace nx::plat {

// ---- memory ---------------------------------------------------------------------------------------------------

namespace {

std::mutex reservation_lock;
// No usar malloc durante el arranque: sus zonas pueden ocupar un rango aún pendiente de reservar.
std::array<std::pair<uint64_t, uint64_t>, 64> reservations{};
size_t reservation_count = 0;

bool owned(uint64_t begin, uint64_t end) {
    return std::any_of(reservations.begin(), reservations.begin() + reservation_count, [&](const auto& range) {
        return begin >= range.first && end <= range.second;
    });
}

kern_return_t region_at(uint64_t point, mach_vm_address_t& address, mach_vm_size_t& size,
                        vm_region_submap_info_data_64_t& info) {
    address = point;
    size = 0;
    natural_t depth = 0;
    kern_return_t status;
    do {
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        status = mach_vm_region_recurse(mach_task_self(), &address, &size, &depth,
                                       reinterpret_cast<vm_region_recurse_info_t>(&info), &count);
        if (status != KERN_SUCCESS) break;
        if (info.is_submap) {
            ++depth;
            address = point;
        }
    } while (info.is_submap);
    return status;
}

bool empty_system_reservation(uint64_t begin, uint64_t end) {
    // Este intervalo queda por encima de la caché dyld y por debajo de sus guardas.
    // Se comprueba cada región: una reserva nuestra PROT_NONE conserva max_protection
    // y también debe rechazarse, igual que cualquier mapa legible o guarda de macOS.
    if (begin < 0x400000000ull || end > 0xFC0000000ull) return false;
    while (begin < end) {
        mach_vm_address_t address = begin;
        mach_vm_size_t size = 0;
        vm_region_submap_info_data_64_t info{};
        const kern_return_t status = region_at(begin, address, size, info);
        if (status == KERN_INVALID_ADDRESS) return true;
        if (status != KERN_SUCCESS || size == 0 || address + size <= begin) return false;
        if (address >= end) return true;
        if (info.protection != VM_PROT_NONE || info.max_protection != VM_PROT_NONE || info.user_tag != 0)
            return false;
        begin = address + size;
    }
    return true;
}

void clear_partial(uint64_t addr, uint64_t length, uint64_t page) {
    if (!length) return;
    const uint64_t base = addr & ~(page - 1);
    mach_vm_address_t address;
    mach_vm_size_t size;
    vm_region_submap_info_data_64_t info{};
    if (region_at(base, address, size, info) != KERN_SUCCESS || address > base)
        throw std::runtime_error("No se pudo consultar la página parcial del invitado");
    if (info.protection == VM_PROT_NONE) return;
    const bool writable = (info.protection & VM_PROT_WRITE) != 0;
    if (!writable && mprotect(reinterpret_cast<void*>(base), page, PROT_READ | PROT_WRITE))
        throw std::runtime_error("No se pudo liberar la página parcial del invitado");
    std::memset(reinterpret_cast<void*>(addr), 0, length);
    if (!writable && mprotect(reinterpret_cast<void*>(base), page, info.protection))
        throw std::runtime_error("No se pudo restaurar la protección de la página vecina");
}

}  // namespace

bool reserve(uint64_t addr, uint64_t size) {
    const uint64_t page = getpagesize();
    if (!size || addr + size < addr || addr % page || size % page) return false;
    std::lock_guard lock(reservation_lock);
    if (reservation_count == reservations.size() ||
        std::any_of(reservations.begin(), reservations.begin() + reservation_count, [&](const auto& range) {
            return addr < range.second && addr + size > range.first;
        })) return false;
    void* p = mmap(reinterpret_cast<void*>(addr), size, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    if (p != MAP_FAILED && uint64_t(p) == addr) {
        reservations[reservation_count++] = {addr, addr + size};
        return true;
    }
    if (p != MAP_FAILED) munmap(p, size);
    // Darwin ignora el hint dentro de su reserva vacía de direcciones bajas.
    // Solo sustituimos esa reserva tras inspeccionarla; nunca datos o guardas.
    if (!empty_system_reservation(addr, addr + size)) return false;
    p = mmap(reinterpret_cast<void*>(addr), size, PROT_NONE,
             MAP_FIXED | MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED) return false;
    reservations[reservation_count++] = {addr, addr + size};
    return true;
}

bool commit(uint64_t addr, uint64_t size) {
    if (!size) return true;
    uint64_t page = uint64_t(getpagesize()) - 1;
    if (addr + size < addr || addr + size + page < addr + size) return false;
    uint64_t a = addr & ~page, e = (addr + size + page) & ~page;
    std::lock_guard lock(reservation_lock);
    if (!owned(a, e)) return false;
    return mprotect(reinterpret_cast<void*>(a), e - a, PROT_READ | PROT_WRITE) == 0;
}

// El invitado libera páginas de 4 KiB sobre páginas Mac de 16 KiB. Los extremos
// preservan vecinos y guardas; las páginas completas se sustituyen por memoria
// anónima vacía. MADV_FREE por sí solo no garantiza ceros al volver a comprometerlas.
void decommit(uint64_t addr, uint64_t size) {
    if (!size) return;
    std::lock_guard lock(reservation_lock);
    if (addr + size < addr || !owned(addr, addr + size))
        throw std::runtime_error("Se intentó liberar memoria ajena al invitado");
    uint64_t page = uint64_t(getpagesize());
    uint64_t lo = (addr + page - 1) & ~(page - 1), hi = (addr + size) & ~(page - 1);
    clear_partial(addr, std::min(lo, addr + size) - addr, page);
    const uint64_t tail = std::max(lo, hi);
    if (tail < addr + size) clear_partial(tail, addr + size - tail, page);
    if (hi > lo && mmap(reinterpret_cast<void*>(lo), hi - lo, PROT_NONE,
                        MAP_FIXED | MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0) == MAP_FAILED)
        throw std::runtime_error("No se pudo liberar la memoria reservada del invitado");
}

}  // namespace nx::plat
