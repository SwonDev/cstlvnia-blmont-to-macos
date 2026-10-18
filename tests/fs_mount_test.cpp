#include <nx/runtime.h>
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <unistd.h>

namespace {
auto& registry() { static std::map<std::string, nx::HleFn> r; return r; }
std::string root;
uint64_t ptr(const void* p) { return reinterpret_cast<uint64_t>(p); }
void invoke(const char* name, nx::CPU& c) {
    auto it = registry().find(name);
    if (it == registry().end()) { std::fprintf(stderr, "Falta contrato: %s\n", name); std::exit(1); }
    it->second(c);
}
}
namespace nx {
bool g_trace = false;
HleRegistrar::HleRegistrar(const char* name, HleFn fn) { registry()[name] = fn; }
void log(const char*, ...) {}
namespace plat {
std::string data_root() { return root; }
std::string writable_root() { return root + "/perfil"; }
int64_t current_thread_id() { return 1; }
uint64_t monotonic_ns() { return 0; }
FILE* fopen_utf8(const std::string& path, const char* mode) { return std::fopen(path.c_str(), mode); }
int64_t ftell64(FILE* f) { return ftello(f); }
int fseek64(FILE* f, int64_t off, int origin) { return fseeko(f, off, origin); }
uint64_t read_at(FILE* f, int64_t off, void* out, uint64_t count) { return pread(fileno(f), out, count, off); }
}
}
int main() {
    char temporary[] = "/tmp/nx-fs-XXXXXX";
    root = mkdtemp(temporary);
    std::filesystem::create_directories(root + "/extract/romfs/Data");
    { std::ofstream f(root + "/extract/romfs/Data/fixture"); f << "metadatos"; }
    nx::CPU c{};
    c.x[0] = ptr("rom"); invoke("_ZN2nn2fs8MountRomEPKc", c); assert(c.x[0] == 0);
    c.x[0] = ptr("rom"); invoke("_ZN2nn2fs9IsMountedEPKc", c); assert(c.x[0] == 1);
    c.x[0] = ptr("rom:/"); invoke("_ZN2nn2fs9IsMountedEPKc", c); assert(c.x[0] == 0);
    for (const char* path : {"rom:/Data/fixture", "rom:/no-existe"}) {
        c.x[0] = ptr(path); invoke("_ZN2nn2fs13IsPathMountedEPKc", c); assert(c.x[0] == 1);
    }
    for (const char* symbol : {"_ZN2nn2fs16GetPathLengthMaxEPmPKc", "_ZN2nn2fs21GetEntryNameLengthMaxEPmPKc"}) {
        uint64_t maximum = 0;
        c.x[0] = ptr(&maximum); c.x[1] = ptr("rom:/Data/fixture"); invoke(symbol, c);
        assert(c.x[0] == 0 && maximum >= 255 && maximum <= 0x300);
        maximum = 42; c.x[0] = ptr(&maximum); c.x[1] = ptr("desconocido:/fixture"); invoke(symbol, c);
        assert(c.x[0] != 0 && maximum == 42);
    }
    for (const char* symbol : {"_ZN2nn2fs21GetUtf16PathLengthMaxEPbPmPKc", "_ZN2nn2fs26GetUtf16EntryNameLengthMaxEPbPmPKc"}) {
        bool constrained = true; uint64_t maximum = 42;
        c.x[0] = ptr(&constrained); c.x[1] = ptr(&maximum); c.x[2] = ptr("rom:/Data/fixture"); invoke(symbol, c);
        assert(c.x[0] == 0 && !constrained);
    }
    uint64_t handle = 0;
    c.x[0] = ptr(&handle); c.x[1] = ptr("rom:/Data/fixture"); c.x[2] = 1;
    invoke("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci", c); assert(c.x[0] == 0 && handle);
    char data[10]{}; uint64_t count = 0;
    c.x[0] = ptr(&count); c.x[1] = handle; c.x[2] = 0; c.x[3] = ptr(data); c.x[4] = 9;
    invoke("_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm", c); assert(c.x[0] == 0 && count == 9 && std::string(data) == "metadatos");
    c.x[0] = handle; invoke("_ZN2nn2fs9CloseFileENS0_10FileHandleE", c);
    c.x[0] = ptr("save"); invoke("_ZN2nn2fs13MountSaveDataEPKcRKNS_7account3UidE", c);
    assert(c.x[0] == 0);
    for (const char* symbol : {"_ZN2nn2fs16GetFreeSpaceSizeEPlPKc", "_ZN2nn2fs17GetTotalSpaceSizeEPlPKc"}) {
      for (const char* path : {"save", "save:/", "save:/archivo-aun-inexistente"}) {
        int64_t bytes = -1;
        c.x[0] = ptr(&bytes); c.x[1] = ptr(path); invoke(symbol, c);
        auto space = std::filesystem::space(root + "/perfil/user/save");
        assert(c.x[0] == 0 && bytes >= 0 && uint64_t(bytes) <= space.capacity);
        if (std::string(symbol).find("Total") != std::string::npos) assert(uint64_t(bytes) == space.capacity);
        bytes = -1;
        c.x[0] = ptr(&bytes); c.x[1] = ptr("ausente:/"); invoke(symbol, c);
        assert(c.x[0] != 0 && bytes == -1);
      }
    }
    c.x[0] = ptr("save:/persistencia.bin"); c.x[1] = 4;
    invoke("_ZN2nn2fs10CreateFileEPKcl", c);
    assert(c.x[0] == 0 && std::filesystem::file_size(root + "/perfil/user/save/persistencia.bin") == 4);
    assert(!std::filesystem::exists(root + "/extract/romfs/user"));
    c.x[0] = ptr("rom"); invoke("_ZN2nn2fs7UnmountEPKc", c);
    c.x[0] = ptr("rom:/Data/fixture"); invoke("_ZN2nn2fs13IsPathMountedEPKc", c); assert(c.x[0] == 0);
    c.x[0] = ptr("rom"); invoke("_ZN2nn2fs9IsMountedEPKc", c); assert(c.x[0] == 0);
    std::filesystem::remove_all(root);
    std::puts("Montaje, consultas previas del SDK y lectura: PASS");
}
