#include "nvn/nvn.h"
#include "nvn/nvn_texture_identity.h"

#include <array>
#include <cstring>

namespace {
nx::nvn::ObjectsLock lock;
std::unordered_map<uint64_t, std::unique_ptr<nx::nvn::Object>> registry;
std::vector<std::unique_ptr<nx::nvn::Object>> deferred;
alignas(16) std::array<uint8_t, 0x180> memory{};
}
namespace nx {
void log(const char*, ...) {}
VmInfo vm_query(uint64_t address) {
    auto base = reinterpret_cast<uint64_t>(memory.data());
    return address - base < memory.size() ? VmInfo{base, memory.size(), 1, 3} : VmInfo{address, 0x1000, 0, 0};
}
}
namespace nx::nvn {
Texture::~Texture() = default;
ObjectsLock& objects_lock() { return lock; }
std::unordered_map<uint64_t, std::unique_ptr<Object>>& objects() { return registry; }
void defer_object(std::unique_ptr<Object> object) { deferred.push_back(std::move(object)); }
void drain_objects() {}
}
int main() {
    using namespace nx::nvn;
    uint64_t first = reinterpret_cast<uint64_t>(memory.data()), second = first + 0x80, alias = first + 0x100;
    auto& a = make<Texture>(first); a.copy_generation = 1; stamp_texture_identity(first, 1);
    auto& b = make<Texture>(second); b.copy_generation = 2; stamp_texture_identity(second, 2);
    std::memcpy(reinterpret_cast<void*>(alias), reinterpret_cast<void*>(first), 0x80);
    if (get<Texture>(alias) != &a || texture_identity_address(alias) != first) return 1;
    // Cambia la copia sin tocar el registro del host: no debe reutilizar una entrada cacheada.
    std::memcpy(reinterpret_cast<void*>(alias), reinterpret_cast<void*>(second), 0x80);
    if (get<Texture>(alias) != &b || texture_identity_address(alias) != second) return 2;
    t_replay_serial = 42; t_replay_mark = current_epoch();
    if (get<Texture>(alias) != &b) return 3;
    std::memcpy(reinterpret_cast<void*>(alias), reinterpret_cast<void*>(first), 0x80);
    if (get<Texture>(alias) != &a) return 4;
    auto before = current_epoch();
    { std::lock_guard guard(objects_lock()); note_replaced(first); }
    auto& replaced = make<Texture>(first); replaced.copy_generation = 3; stamp_texture_identity(first, 3);
    t_replay_serial = 43; t_replay_mark = before;
    if (get<Texture>(alias) != &a) return 5;
    t_replay_serial = 0; t_replay_mark = 0;
    if (get<Texture>(alias) != nullptr) return 6;
    std::memset(reinterpret_cast<void*>(alias), 0, 0x80);
    if (get<Texture>(alias) != nullptr || get<Texture>(0xDEADBEEF) != nullptr || get<Texture>(UINT64_MAX) != nullptr) return 7;
    std::puts("Texturas copiadas: identidad, recopia, replay, generaciones y memoria inválida: PASS");
}
