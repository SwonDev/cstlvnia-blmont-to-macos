#include <nx/runtime.h>
#include "platform.h"
#include <sys/mman.h>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <string_view>
#undef NDEBUG
#include <cassert>
void nx_castlevania_video_text(nx::CPU&,void(*)(nx::CPU&));
void nx_castlevania_setting_active(nx::CPU&,void(*)(nx::CPU&));
void nx_castlevania_video_defaults(nx::CPU&,void(*)(nx::CPU&));
void nx_castlevania_supported_resolutions(nx::CPU&,void(*)(nx::CPU&));
void nx_castlevania_video_initialize(nx::CPU&,void(*)(nx::CPU&));
void nx_castlevania_resolution_changed(nx::CPU&,void(*)(nx::CPU&));
namespace {
std::vector<std::vector<uint8_t>> strings;
uint64_t seen_text=0;
void text_original(nx::CPU& c){seen_text=c.x[1];nx::st<uint64_t>(c.x[0]+0xd8,seen_text);}
int mode=3,calls=0;uint32_t width=0,height=0;
void original(nx::CPU& c) {++calls;c.x[0]=0;}
void console_load(nx::CPU& c) {
    const auto window=nx::ld<uint64_t>(c.x[0]+0x58),choices=nx::ld<uint64_t>(window+0x58);
    for(unsigned n=0;n<4;++n)nx::st<uint8_t>(nx::ld<uint64_t>(choices+32+n*8)+0x28,n==0 || n==3);
    nx::st<uint8_t>(window+0x50,0);
    nx::st<uint8_t>(nx::ld<uint64_t>(c.x[0]+0x68)+0x50,0);
    c.x[0]=0;
}
uint64_t address(void* p){return reinterpret_cast<uint64_t>(p);}
}
namespace nx {
void log(const char*,...){}
void call(CPU& c,uint64_t target) {
    if(target==NX_BASE+0x4e00a50) {assert(c.x[0]==2560 && c.x[1]==1440 && c.x[2]==3 && c.x[3]==0x100000000ull);++calls;}
    else if(target==NX_BASE+0x418f160) {
        const auto len=c.x[0];strings.emplace_back(22+len*2);c.x[0]=address(strings.back().data());st<int32_t>(c.x[0]+16,len);
    }
    else if(target==1){assert(c.x[1]==1);st<uint8_t>(c.x[0]+0x50,1);}
    else {assert(target==0);c.x[0]=ld<int32_t>(c.x[0]+0x18);}
}
namespace plat {void resize_video_window(uint32_t,uint32_t){} int video_window_mode(){return mode;}void set_video_window_mode(int m){mode=m;}}
namespace game {
void video_native_size(uint32_t& w,uint32_t& h){w=3024;h=1701;}
void video_render_size(uint32_t& w,uint32_t& h){w=3024;h=1701;}
bool select_render_size(uint32_t w,uint32_t h){width=w;height=h;return w==2560 && h==1440;}
}
}
int main(){
    auto* mapped=mmap(reinterpret_cast<void*>(NX_BASE),0x7000000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON|MAP_FIXED,-1,0);
    assert(mapped==reinterpret_cast<void*>(NX_BASE));
    std::vector<uint8_t> modes(48), general(32+12*8),klass(512),fields(64),list(32+12*8),object(256),setting(128),possible(32+12*8),window(128),enumklass(512);
    uint64_t klassptr=address(klass.data());
    nx::st<uint64_t>(NX_BASE+0x6be3ec8,address(&klassptr));nx::st<uint64_t>(klassptr+0xb8,address(fields.data()));
    nx::st<uint64_t>(address(fields.data())+8,address(list.data()));nx::st<uint32_t>(address(list.data())+24,12);
    nx::st<uint64_t>(address(fields.data()),address(general.data()));nx::st<uint32_t>(address(general.data())+24,12);
    nx::CPU c{};nx_castlevania_video_defaults(c,original);
    assert(nx::ld<uint32_t>(address(general.data())+32+11*8)==3024);
    assert(nx::ld<uint32_t>(address(list.data())+32+11*8)==3024);
    assert(nx::ld<uint32_t>(address(list.data())+36+11*8)==1701);
    std::vector<uint8_t> text(100);auto key=u"video/resolution";nx::st<int32_t>(address(text.data())+16,16);std::memcpy(text.data()+20,key,32);
    c.x[1]=address(text.data());nx_castlevania_setting_active(c,original);assert(c.x[0]==1);
    nx::st<int32_t>(address(text.data())+16,2);nx_castlevania_setting_active(c,original);assert(c.x[0]==0);
    nx::st<uint64_t>(address(object.data())+0x68,address(setting.data()));nx::st<uint64_t>(address(object.data())+0x58,address(window.data()));
    nx::st<uint64_t>(address(setting.data()),address(enumklass.data()));
    nx::st<uint32_t>(address(general.data())+32+8*8,2560);nx::st<uint32_t>(address(general.data())+36+8*8,1440);
    nx::st<int32_t>(address(setting.data())+0x18,8);
    c.x[0]=address(object.data());nx_castlevania_resolution_changed(c,original);assert(width==2560 && height==1440);
    nx::st<uint32_t>(address(list.data())+32+8*8,2560);nx::st<uint32_t>(address(list.data())+36+8*8,1440);
    nx::st<uint64_t>(address(fields.data())+16,address(modes.data()));nx::st<uint32_t>(address(modes.data())+24,4);
    for(unsigned n=0;n<4;++n)nx::st<uint32_t>(address(modes.data())+32+n*4,n);
    nx::st<uint64_t>(address(window.data()),address(enumklass.data()));
    nx::st<uint64_t>(address(enumklass.data())+0x188,1);
    std::vector<uint8_t> choices(32+4*8);
    std::vector<std::vector<uint8_t>> resolution_options(12,std::vector<uint8_t>(64)),window_options(4,std::vector<uint8_t>(64));
    nx::st<uint64_t>(address(setting.data())+0x58,address(possible.data()));nx::st<uint32_t>(address(possible.data())+24,12);
    nx::st<uint64_t>(address(window.data())+0x58,address(choices.data()));nx::st<uint32_t>(address(choices.data())+24,4);
    for(unsigned n=0;n<12;++n){nx::st<uint64_t>(address(possible.data())+32+n*8,address(resolution_options[n].data()));nx::st<uint32_t>(address(resolution_options[n].data())+0x20,n);}
    for(unsigned n=0;n<4;++n){nx::st<uint64_t>(address(choices.data())+32+n*8,address(window_options[n].data()));nx::st<uint32_t>(address(window_options[n].data())+0x20,n);}
    nx::st<uint32_t>(address(list.data())+32+9*8,2560);nx::st<uint32_t>(address(list.data())+36+9*8,1440);
    nx::st<uint32_t>(address(list.data())+32+10*8,3840);nx::st<uint32_t>(address(list.data())+36+10*8,2160);
    std::memcpy(general.data(),list.data(),list.size());
    c.x[0]=address(object.data());nx_castlevania_supported_resolutions(c,original);
    const auto native_label=nx::ld<uint64_t>(address(resolution_options[11].data())+0x10);
    assert(nx::ld<int32_t>(native_label+16)==9 && nx::ld<char16_t>(native_label+20)==u'3');
    assert(resolution_options[8][0x28] && !resolution_options[9][0x28] && !resolution_options[10][0x28] && resolution_options[11][0x28]);
    assert(!window_options[0][0x28] && window_options[1][0x28] && window_options[2][0x28] && window_options[3][0x28]);
    for(int wanted:{1,2}) {
        mode=wanted;c.x[0]=address(object.data());nx_castlevania_video_initialize(c,console_load);
        assert(nx::ld<int32_t>(address(window.data())+0x18)==wanted && nx::ld<int32_t>(address(window.data())+0x1c)==wanted);
    }
    mode=3;c.x[0]=address(object.data());nx_castlevania_video_initialize(c,console_load);
    assert(nx::ld<uint32_t>(address(window.data())+0x18)==3 && nx::ld<uint32_t>(address(window.data())+0x1c)==3);
    assert(nx::ld<uint8_t>(address(window.data())+0x50)==1);
    assert(nx::ld<uint8_t>(address(setting.data())+0x50)==1);
    assert(!window_options[0][0x28] && window_options[1][0x28] && window_options[2][0x28] && window_options[3][0x28]);
    auto old_label=u"3840 × 2160";nx::st<int32_t>(address(text.data())+16,11);std::memcpy(text.data()+20,old_label,22);
    c.x[0]=address(object.data());c.x[1]=address(text.data());nx_castlevania_video_text(c,text_original);
    assert(nx::ld<int32_t>(seen_text+16)==11 && nx::ld<char16_t>(seen_text+20+5*2)==u'×');
    auto spaced_label=u"3840\u00a0×\u00a02160";std::memcpy(text.data()+20,spaced_label,22);
    const auto allocated=strings.size();const auto previous=seen_text;
    c.x[1]=address(text.data());nx_castlevania_video_text(c,text_original);assert(strings.size()==allocated && seen_text==previous);
    for(std::u16string_view other:{u"3840 × 1600",u"3840x2160p",u"Resolución: 3840x2160"}) {
        nx::st<int32_t>(address(text.data())+16,other.size());std::memcpy(text.data()+20,other.data(),other.size()*2);
        c.x[0]=address(object.data());c.x[1]=address(text.data());nx_castlevania_video_text(c,text_original);
        assert(seen_text==address(text.data()) && strings.size()==allocated);
    }
    munmap(mapped,0x7000000);
}
