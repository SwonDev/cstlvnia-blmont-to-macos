#if defined(__APPLE__)
#include "resolution.h"
#include "video_options.h"
#include <nx/runtime.h>
#include "platform.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace nx::game {
namespace {
uint32_t native_width=1920,native_height=1080;
std::atomic<uint64_t> selected_size{uint64_t(1920)<<32 | 1080};
void set_resolution(CPU& cpu) {
    const auto width=uint32_t(cpu.x[1]),height=uint32_t(cpu.x[2]);
    if((width==1920 && height==1080) || (width==1280 && height==720)) {
        const auto size=selected_size.load();
        cpu.x[1]=uint32_t(size>>32);cpu.x[2]=uint32_t(size);
        log("video: resolución interna %u×%u (solicitud original %u×%u)",uint32_t(cpu.x[1]),uint32_t(cpu.x[2]),width,height);
    }
}
}
void video_native_size(uint32_t& w,uint32_t& h) {w=native_width;h=native_height;}
void video_render_size(uint32_t& w,uint32_t& h) {const auto size=selected_size.load();w=uint32_t(size>>32);h=uint32_t(size);}
bool select_render_size(uint32_t w,uint32_t h) {
    if(w<640 || h<360 || w>native_width || h>native_height || w*9!=h*16)return false;
    selected_size=uint64_t(w)<<32 | h;return true;
}
void install_resolution_policy() {
    constexpr uint64_t setter=0xca9db4;
    const auto* module=module_at(NX_BASE);
    if(!module || std::string_view(module->info->name)!="main" || module->info->text_size<setter+0x180 ||
       !module->table[setter/4] || module->info->instruction_patch_count!=9 ||
       ld<uint32_t>(module->base+0xca9ea4)!=0x2a1503e8 || ld<uint32_t>(module->base+0xca9eb0)!=0x2a1403e8)
        fatal("Falta el perfil AOT de resolución de Castlevania 1.1.0");
    uint32_t width=0,height=0;
    plat::desktop_size(width,height);
    // Esta revisión compone en 16:9; el almacenamiento se reserva una vez al máximo del panel.
    const auto units=std::min({width/16,height/9,240u});
    native_width=units>=80 ? units*16 : 1920;
    native_height=units>=80 ? units*9 : 1080;
    selected_size=uint64_t(native_width)<<32 | native_height;
    if(const char* requested=std::getenv("NX_RENDER_RES")) {
        unsigned w=0,h=0;char tail;
        if(std::sscanf(requested,"%ux%u%c",&w,&h,&tail)==2)select_render_size(w,h);
    }
    uint32_t w,h;video_render_size(w,h);
    log("video: perfil Retina preparado %u×%u, lienzo máximo %u×%u",w,h,native_width,native_height);
}
}
void nx_castlevania_render_resolution(nx::CPU& cpu) {nx::game::set_resolution(cpu);}
void nx_castlevania_framebuffer_resolution(nx::CPU& cpu) {cpu.x[0]=nx::game::native_width;cpu.x[1]=nx::game::native_height;}
void nx_castlevania_depth_resolution(nx::CPU& cpu) {cpu.x[1]=nx::game::native_width;cpu.x[2]=nx::game::native_height;}
#endif
