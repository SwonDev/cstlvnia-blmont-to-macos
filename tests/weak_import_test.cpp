#include <nx/runtime.h>
#include <nx/game.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace nx {
const game::Info game::kInfo = {"fixture", "Fixture", 1, "fixture", ".", 0, true};
bool commit(uint64_t, uint64_t) { return true; }
void vm_set(uint64_t, uint64_t, uint32_t, uint32_t) {}
void backtrace(CPU&) {}
void thread_checkpoint(uint64_t) {}
namespace plat {
void log_line(const char*) {}
std::string data_root() { return "."; }
std::string writable_root() { return "."; }
[[noreturn]] void exit_process(int status) { std::exit(status); }
}
}

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) return 2;
    const bool strong = argc == 3;
    std::filesystem::path directory = argv[1];
    alignas(4096) unsigned char image[4096]{};
    {
        std::ofstream file(directory / "fixture.bin", std::ios::binary);
        file.write(reinterpret_cast<const char*>(image), 256);
        if (!file) return 3;
    }
    const nx::Symbol symbols[] = {
        {"", 0, 0, 0, 0, 0},
        {strong ? "dependencia_necesaria" : "funcion_opcional", 0, 0, 2, uint8_t(strong ? 1 : 2), 1},
        {"dato_opcional", 0, 0, 1, 2, 1},
    };
    const nx::Reloc relocs[] = {
        {8, nx::R_AARCH64_GLOB_DAT, 1, 0},
        {16, nx::R_AARCH64_ABS64, 2, 8},
    };
    const nx::ModuleInfo module = {
        "fixture", reinterpret_cast<uint64_t>(image),
        0, 4, 4, 4, 0, 256, 256, 0,
        nullptr, 0, relocs, 2, symbols, 3,
        nullptr, 0, nullptr, 0,
    };
    const nx::ModuleInfo* modules[] = {&module};
    nx::load_modules(modules, 1, directory.string());
    if (strong) return 4;
    const auto function = nx::ld<uint64_t>(reinterpret_cast<uint64_t>(image + 8));
    const auto data = nx::ld<uint64_t>(reinterpret_cast<uint64_t>(image + 16));
    if (function != 0 || data != 8) {
        std::cerr << "Los imports débiles ausentes deben resolver a cero más su addend\n";
        return 1;
    }
    return 0;
}
