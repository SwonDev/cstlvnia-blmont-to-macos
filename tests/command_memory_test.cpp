#include "nvn/nvn.h"
#include "nvn/nvn_shaders.h"
#include <array>
#include <cstdio>
#include <stdexcept>
namespace {
auto& functions() { static std::unordered_map<std::string, nx::HleFn> f; return f; }
nx::CPU cpu;
nx::nvn::ObjectsLock mutex;
unsigned events[2] = {}, serial = 0;
bool replace_recording = false;
void invoke(const char* name) { functions().at(name)(cpu); }
void memory(unsigned kind) {
    cpu.x[0] = 0x100; cpu.x[1] = kind ? 0x9000 : 0x200; cpu.x[2] = ++serial * 1024;
    if (kind) { cpu.x[2] = 256; invoke("nvnCommandBufferAddControlMemory"); }
    else { cpu.x[3] = 256; invoke("nvnCommandBufferAddCommandMemory"); }
}
}
namespace nx {
HleRegistrar::HleRegistrar(const char* n, HleFn f) { functions()[n] = f; }
CPU& current_cpu() { return cpu; }
void log(const char*, ...) {}
[[noreturn]] void fatal(const char*, ...) { throw std::runtime_error("Memoria agotada"); }
std::string describe(uint64_t) { return "fixture"; }
}
namespace nx::nvn {
bool g_trace = false;
Texture::~Texture() = default;
ObjectsLock& objects_lock() { return mutex; }
std::unordered_map<uint64_t, std::unique_ptr<Object>>& objects() { static std::unordered_map<uint64_t, std::unique_ptr<Object>> o; return o; }
void defer_object(std::unique_ptr<Object>) {}
bool resolve_cache_on() { return false; }
Object* find_object(uint64_t a, uint64_t) { auto it = objects().find(a); return it == objects().end() ? nullptr : it->second.get(); }
uint64_t current_epoch() { return 1; }
uint64_t texture_identity_address(uint64_t a) { return a; }
ViewInfo read_view(CPU&, uint64_t) { return {}; }
const std::vector<const ShaderInfo*>* program_shaders(uint64_t) { return nullptr; }
uint64_t guest_call(CPU&, uint64_t fn, std::initializer_list<uint64_t> args) {
    const auto saved = cpu; std::array<uint64_t, 4> a{}; std::copy(args.begin(), args.end(), a.begin());
    if (fn != 0xABC || a[0] != 0x100 || a[1] > 1 || a[3] != 0xDEF) throw std::runtime_error("ABI callback inválida");
    if (replace_recording) { cpu.x[0] = 0x100; invoke("nvnCommandBufferEndRecording"); cpu.x[0] = 0x100; invoke("nvnCommandBufferBeginRecording"); }
    events[a[1]]++; memory(a[1]); cpu = saved; return 0;
}
}
int main() {
    using namespace nx::nvn;
    if (!functions().count("nvnCommandBufferSetMemoryCallback")) { std::puts("FALLO: falta callback de agotamiento"); return 1; }
    cpu.x[0] = 0x100; invoke("nvnCommandBufferInitialize");
    auto pool = std::make_unique<MemoryPool>(); pool->gpu = 0x100000; pool->size = 1 << 20; pool->guest_address = 0x200;
    objects()[0x200] = std::move(pool);
    cpu.x[0] = 0x100; cpu.x[1] = 0xABC; invoke("nvnCommandBufferSetMemoryCallback");
    cpu.x[1] = 0xDEF; invoke("nvnCommandBufferSetMemoryCallbackData");
    command_set_copy_stride(0x100, false, 1920); command_set_copy_stride(0x100, true, 1088 * 1920);
    cpu.x[0] = 0x300; invoke("nvnCommandBufferInitialize");
    if (command_copy_strides(0x300) != std::pair<uint64_t,uint64_t>{0,0} || command_copy_strides(0x100).first != 1920) return 6;
    memory(0); memory(1);
    std::vector<uint64_t> handles;
    for (unsigned list = 0; list < 30; list++) {
        cpu.x[0] = 0x100; invoke("nvnCommandBufferBeginRecording");
        for (unsigned i = 0; i < 10; i++) record(0x100, Cmd{Cmd::DrawArrays});
        cpu.x[0] = 0x100; invoke("nvnCommandBufferEndRecording"); handles.push_back(cpu.x[0]);
    }
    if (!events[0] || !events[1]) return 2;
    for (auto h : handles) { auto list = command_list(h); if (!list || list->cmds.size() != 10) return 3; }
    memory(0); const uint64_t first_segment = 0x100000 + serial * 1024;
    cpu.x[0] = 0x100; invoke("nvnCommandBufferBeginRecording");
    for (unsigned i = 0; i < 30; i++) record(0x100, Cmd{Cmd::DrawArrays});
    cpu.x[0] = 0x100; invoke("nvnCommandBufferEndRecording"); const auto spanning = cpu.x[0];
    if (!command_list(spanning)) return 4;
    command_memory_released(first_segment, 256);
    if (command_list(spanning)) { std::puts("FALLO: se conserva una lista cuyo primer bloque fue liberado"); return 5; }
    replace_recording = true; bool rejected = false;
    try { for (unsigned i = 0; i < 100; i++) record(0x100, Cmd{Cmd::DrawArrays}); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) return 7;
    std::puts("Memoria NVN: callbacks reentrantes de ambos tipos, rotación y listas antes del primer envío: PASS");
}
