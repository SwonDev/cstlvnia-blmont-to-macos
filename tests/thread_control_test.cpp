#include "kernel_state.h"

#include <array>
#include <chrono>
#include <future>
#include <thread>

namespace {
std::shared_ptr<nx::KThread> target = std::make_shared<nx::KThread>();
}
namespace nx::kstate {
std::recursive_mutex g_k;
thread_local KThread* t_self = nullptr;
}
namespace nx {
std::shared_ptr<KObject> handle_get(uint32_t handle) { return handle == 7 ? target : nullptr; }
}

int main() {
    using namespace std::chrono_literals;
    alignas(16) std::array<uint8_t, 0x330> data;
    data.fill(0xAB);
    const auto out = reinterpret_cast<uint64_t>(data.data());
    if (nx::get_thread_context(out, 99) != 0xE401 || nx::set_thread_activity(99, 1) != 0xE401) return 1;
    if (nx::get_thread_context(out, 7) != 0xFA01 || data[0] != 0xAB) return 2;
    if (nx::set_thread_activity(7, 2) != 0xF001) return 3;
    target->started = true;
    target->ctx.nf = target->ctx.cf = 1;
    target->ctx.fpsr = 1 << 27; target->ctx.fpcr = 0x400000;
    target->ctx.tpidr = 0x12345678;
    for (int i = 0; i < 32; ++i) {
        target->ctx.x[i] = 0x1000 + i;
        target->ctx.v[i].u64[0] = i * 3;
        target->ctx.v[i].u64[1] = i * 7;
    }
    std::atomic<bool> stop{false}, resumed{false};
    std::atomic<uint64_t> progress{0};
    std::promise<void> changed;
    auto changed_future = changed.get_future();
    std::thread worker([&] {
        nx::kstate::t_self = target.get();
        while (!stop.load()) {
            nx::thread_checkpoint(0x4560);
            ++target->ctx.x[0];
            ++progress;
            if (resumed.exchange(false)) changed.set_value();
        }
    });
    int result = 0;
    if (nx::set_thread_activity(7, 1) != 0) result = 4;
    auto count = progress.load();
    if (nx::get_thread_context(out, 7) != 0 || std::memcmp(data.data(), target->ctx.x, 0x100) != 0) result = 5;
    if (nx::ld<uint64_t>(out + 0x100) != 0x4560 || nx::ld<uint32_t>(out + 0x108) != 0xA0000000) result = 6;
    if (std::memcmp(data.data() + 0x110, target->ctx.v, 0x200) != 0 ||
        nx::ld<uint32_t>(out + 0x310) != 0x400000 || nx::ld<uint32_t>(out + 0x314) != (1 << 27) ||
        nx::ld<uint64_t>(out + 0x318) != 0x12345678) result = 7;
    for (size_t i = 0x320; i < data.size(); ++i) if (data[i] != 0xAB) result = 8;
    std::this_thread::sleep_for(10ms);
    if (count != progress.load()) result = 9;
    resumed = true;
    if (nx::set_thread_activity(7, 0) != 0 || changed_future.wait_for(1s) != std::future_status::ready) result = 10;
    stop = true;
    worker.join();
    if (nx::get_thread_context(out, 7) != 0xFA01) result = 11;
    if (!result) std::puts("Hilos: pausa confirmada, contexto AArch64 completo, límites y reanudación: PASS");
    return result;
}
