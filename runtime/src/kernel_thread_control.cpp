// Suspensión cooperativa y contexto AArch64 para el recolector y los servicios del SDK.
#include <cstring>

#include "kernel_state.h"

namespace nx {
namespace kstate {

void checkpoint_locked(KThread* thread, std::unique_lock<std::recursive_mutex>& lock) {
    if (!thread->suspend_requested.load(std::memory_order_acquire)) return;
    thread->suspended = true;
    thread->suspend_cv.notify_all();
    thread->suspend_cv.wait(lock, [&] { return !thread->suspend_requested.load(std::memory_order_acquire); });
    thread->suspended = false;
    thread->suspend_cv.notify_all();
}

}  // namespace kstate

void thread_checkpoint(uint64_t pc) {
    auto* thread = kstate::t_self;
    if (!thread) return;
    thread->guest_pc = pc;
    if (!thread->suspend_requested.load(std::memory_order_acquire)) return;
    std::unique_lock lock(kstate::g_k);
    kstate::checkpoint_locked(thread, lock);
}

uint32_t set_thread_activity(uint32_t handle, uint32_t activity) {
    auto thread = std::dynamic_pointer_cast<KThread>(handle_get(handle));
    if (!thread) return 0xE401;
    if (activity > 1) return 0xF001;
    if (thread.get() == kstate::t_self) return 0xFA01;
    std::unique_lock lock(kstate::g_k);
    if (thread->exited) return 0xFA01;
    thread->suspend_requested.store(activity == 1, std::memory_order_release);
    thread->cv.notify_all();
    thread->suspend_cv.notify_all();
    if (activity == 1 && thread->started)
        thread->suspend_cv.wait(lock, [&] {
            return thread->suspended || thread->exited || !thread->suspend_requested.load(std::memory_order_acquire);
        });
    return thread->exited ? 0xFA01 : 0;
}

uint32_t get_thread_context(uint64_t out, uint32_t handle) {
    auto thread = std::dynamic_pointer_cast<KThread>(handle_get(handle));
    if (!thread) return 0xE401;
    std::lock_guard lock(kstate::g_k);
    if (thread->exited || !thread->suspended || !thread->suspend_requested.load(std::memory_order_acquire)) return 0xFA01;
    const CPU& cpu = thread->ctx;
    // ThreadContext de Horizon: x0..x28, FP/LR/SP, PC/PSTATE, Q0..Q31, FPCR/FPSR, TPIDR_EL0.
    std::memset(reinterpret_cast<void*>(out), 0, 0x320);
    std::memcpy(reinterpret_cast<void*>(out), cpu.x, sizeof cpu.x);
    st<uint64_t>(out + 0x100, thread->guest_pc);
    st<uint32_t>(out + 0x108, uint32_t(cpu.nf) << 31 | uint32_t(cpu.zf) << 30 |
                                uint32_t(cpu.cf) << 29 | uint32_t(cpu.vf) << 28);
    std::memcpy(reinterpret_cast<void*>(out + 0x110), cpu.v, sizeof cpu.v);
    st<uint32_t>(out + 0x310, uint32_t(cpu.fpcr));
    st<uint32_t>(out + 0x314, uint32_t(cpu.fpsr));
    st<uint64_t>(out + 0x318, cpu.tpidr);
    return 0;
}

}  // namespace nx
