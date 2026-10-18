#include "ipc.h"
#undef NDEBUG
#include <cassert>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace {
auto& factories() { static std::map<std::string, nx::ServiceFactory> value; return value; }
std::array<uint8_t, 0x1000> memory{};
}
namespace nx {
void register_service(const std::string& name, ServiceFactory factory) { factories()[name] = std::move(factory); }
uint32_t Service::handle(IpcCtx& c) { return cmds.at(c.cmd)(c); }
uint64_t system_tick() { return 123ull * 19200000; }
uint32_t handle_add(std::shared_ptr<KObject>) { return 42; }
std::shared_ptr<KSharedMemory> make_shared_memory(const std::string&, uint64_t size) {
    assert(size == memory.size());
    auto value = std::make_shared<KSharedMemory>();
    value->host = reinterpret_cast<uint64_t>(memory.data());
    return value;
}
}
int main() {
    setenv("TZ", "UTC", 1); tzset();
    auto time = factories().at("time:u")();
    nx::IpcCtx c; c.cmd = 3; assert(time->handle(c) == 0);
    auto zone = c.out_objects.at(0);
    if (!zone->cmds.contains(202)) {
        std::fputs("ToPosixTimeWithMyRule (202) ausente: FAIL\n", stderr); return 1;
    }
    for (auto [year, month, day, expected] : {
             std::array<int64_t, 4>{2020, 1, 1, 1577836800},
             std::array<int64_t, 4>{2024, 2, 29, 1709164800}}) {
        std::array<uint8_t, 8> calendar{};
        int16_t y = year; std::memcpy(calendar.data(), &y, 2);
        calendar[2] = month; calendar[3] = day;
        int64_t result = -1;
        nx::IpcCtx conversion; conversion.cmd = 202;
        conversion.in = calendar.data(); conversion.in_size = calendar.size();
        conversion.c.push_back({reinterpret_cast<uint64_t>(&result), sizeof(result)});
        assert(zone->handle(conversion) == 0 && conversion.out.size() == 4);
        uint32_t count; std::memcpy(&count, conversion.out.data(), 4);
        assert(count == 1 && result == expected);
        nx::IpcCtx reverse; reverse.cmd = 101;
        reverse.in = reinterpret_cast<const uint8_t*>(&result); reverse.in_size = sizeof(result);
        assert(zone->handle(reverse) == 0 && reverse.out.size() == 32);
        assert(std::memcmp(reverse.out.data(), calendar.data(), calendar.size()) == 0);
    }
    nx::IpcCtx shared; shared.cmd = 20; assert(time->handle(shared) == 0);
    for (size_t base : {0x38, 0x80}) {
        auto stamp = nx::ld<int64_t>(reinterpret_cast<uint64_t>(memory.data()) + base + 8) + 123;
        assert(std::llabs(stamp - int64_t(std::time(nullptr))) <= 2);
    }
    // El SDK 21 usa el ajuste continuo de 56 bytes; rechaza un UUID distinto con 0xcc74.
    const uint64_t address = reinterpret_cast<uint64_t>(memory.data());
    for (size_t slot : {0, 1}) {
        const auto adjustment = address + 0xd8 + slot * 0x38;
        assert(nx::ld<int64_t>(adjustment + 8) == 1);
        assert(nx::ld<uint64_t>(adjustment + 40) == nx::ld<uint64_t>(address + 0x10));
        assert(nx::ld<uint64_t>(adjustment + 48) == nx::ld<uint64_t>(address + 0x18));
        const int64_t now_ns = 123000000000;
        auto delta = ((now_ns - nx::ld<int64_t>(adjustment)) * nx::ld<int64_t>(adjustment + 8)) >> nx::ld<int64_t>(adjustment + 16);
        delta += nx::ld<int64_t>(adjustment + 24);
        delta = std::min(delta, nx::ld<int64_t>(adjustment + 32));
        auto stamp = (now_ns + delta) / 1000000000 + nx::ld<int64_t>(address + 0x40);
        assert(std::llabs(stamp - int64_t(std::time(nullptr))) <= 2);
    }
    setenv("TZ", "America/New_York", 1); tzset();
    // Enero y julio tienen reglas distintas; no sirve sumar el offset de la fecha actual.
    for (auto [month, expected] : {std::array<int64_t, 2>{1, 1704085200},
                                  std::array<int64_t, 2>{7, 1719806400}}) {
        std::array<uint8_t, 8> calendar{};
        int16_t year = 2024; std::memcpy(calendar.data(), &year, 2);
        calendar[2] = month; calendar[3] = 1;
        int64_t result = -1;
        nx::IpcCtx conversion; conversion.cmd = 202;
        conversion.in = calendar.data(); conversion.in_size = calendar.size();
        conversion.c.push_back({reinterpret_cast<uint64_t>(&result), sizeof(result)});
        assert(zone->handle(conversion) == 0 && result == expected);
        nx::IpcCtx reverse; reverse.cmd = 101;
        reverse.in = reinterpret_cast<const uint8_t*>(&result); reverse.in_size = sizeof(result);
        assert(zone->handle(reverse) == 0 && std::memcmp(reverse.out.data(), calendar.data(), 8) == 0);
    }
    for (auto [month, day, hour, expected_count] : {std::array<int, 4>{3, 10, 2, 0},
                                                   std::array<int, 4>{11, 3, 1, 2}}) {
        std::array<uint8_t, 8> calendar{};
        int16_t year = 2024; std::memcpy(calendar.data(), &year, 2);
        calendar[2] = month; calendar[3] = day; calendar[4] = hour; calendar[5] = 30;
        std::array<int64_t, 3> results{-1, -1, 0x12345678};
        nx::IpcCtx conversion; conversion.cmd = 202;
        conversion.in = calendar.data(); conversion.in_size = calendar.size();
        conversion.c.push_back({reinterpret_cast<uint64_t>(results.data()), 16});
        assert(zone->handle(conversion) == 0);
        uint32_t count; std::memcpy(&count, conversion.out.data(), 4);
        assert(count == uint32_t(expected_count) && results[2] == 0x12345678);
        if (count) assert(results[0] == 1730611800 && results[1] == 1730615400);
        else assert(results[0] == -1 && results[1] == -1);
    }
    std::puts("Reloj compartido y conversión calendario/Unix por comando 202: PASS");
}
