#include <nx/runtime.h>
#include "../games/castlevania/resolution.h"
#include "platform.h"
#undef NDEBUG
#include <cassert>
#include <cstdlib>
#include <vector>
#include <cstring>
void nx_castlevania_render_resolution(nx::CPU&);
void nx_castlevania_framebuffer_resolution(nx::CPU&);
namespace {
std::vector<uint8_t> image(0xd50060);
std::vector<nx::Fn> table(image.size()/4);
nx::ModuleInfo info{};
nx::LoadedModule module{};
void noop(nx::CPU&) {}
}
namespace nx {
void log(const char*,...) {}
[[noreturn]] void fatal(const char*,...) { std::abort(); }
const LoadedModule* module_at(uint64_t) { return &module; }
namespace plat { void desktop_size(uint32_t& w,uint32_t& h) { w=3024;h=1964; } }
}
int main() {
    info.name="main";info.text_size=image.size();info.instruction_patch_count=9;
    module={&info,reinterpret_cast<uint64_t>(image.data()),image.size(),table.data()};
    table[0xca9db4/4]=noop;
    nx::st<uint32_t>(module.base+0xca9ea4,0x2a1503e8);
    nx::st<uint32_t>(module.base+0xca9eb0,0x2a1403e8);
    unsetenv("NX_RENDER_RES");nx::game::install_resolution_policy();
    nx::CPU c{};c.x[0]=123;c.x[1]=1920;c.x[2]=1080;
    nx_castlevania_render_resolution(c);assert(c.x[0]==123 && c.x[1]==3024 && c.x[2]==1701);
    nx_castlevania_framebuffer_resolution(c);assert(c.x[0]==3024 && c.x[1]==1701);
    c.x[1]=512;c.x[2]=512;nx_castlevania_render_resolution(c);assert(c.x[1]==512 && c.x[2]==512);
    setenv("NX_RENDER_RES","2560x1440",1);nx::game::install_resolution_policy();
    c.x[1]=1280;c.x[2]=720;nx_castlevania_render_resolution(c);assert(c.x[1]==2560 && c.x[2]==1440);
    nx_castlevania_framebuffer_resolution(c);assert(c.x[0]==3024 && c.x[1]==1701);
    setenv("NX_RENDER_RES","3840x2160",1);nx::game::install_resolution_policy();
    c.x[1]=1920;c.x[2]=1080;nx_castlevania_render_resolution(c);assert(c.x[1]==3024 && c.x[2]==1701);
}
