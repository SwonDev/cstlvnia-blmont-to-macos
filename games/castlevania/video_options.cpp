#if defined(__APPLE__)
#include "video_options.h"
#include <nx/runtime.h>
#include "platform.h"
#include <string_view>
#include <set>
#include <string>
#include <atomic>

namespace {
using nx::CPU; using nx::ld; using nx::st;
constexpr uint64_t base=NX_BASE;
thread_local bool initializing=false;
uint64_t static_fields() {
    const auto usage=ld<uint64_t>(base+0x6be3ec8);
    const auto klass=ld<uint64_t>(usage);
    return klass ? ld<uint64_t>(klass+0xb8) : 0;
}
bool key_is(uint64_t text,std::u16string_view key) {
    if (!text || ld<int32_t>(text+16)!=int(key.size())) return false;
    for (size_t n=0;n<key.size();++n) if(ld<char16_t>(text+20+n*2)!=key[n]) return false;
    return true;
}
bool legacy_native_label(uint64_t text) {
    if(!text)return false;
    const auto length=ld<int32_t>(text+16);if(length<9 || length>15)return false;
    constexpr std::u16string_view expected=u"3840x2160";size_t position=0;
    for(int n=0;n<length;++n) {
        auto ch=ld<char16_t>(text+20+n*2);
        if(ch==u' ' || ch==u'\u00a0' || ch==u'\u202f')continue;
        if(ch==u'×' || ch==u'X')ch=u'x';
        if(position>=expected.size() || ch!=expected[position++])return false;
    }
    return position==expected.size();
}
uint64_t managed_label(CPU& caller,uint64_t current,std::u16string_view label) {
    if(key_is(current,label))return current;
    CPU tmp=caller;tmp.x[0]=label.size();tmp.x[1]=0;
    nx::call(tmp,base+0x418f160); // System.String.FastAllocateString usa el asignador y el GC originales.
    if(!tmp.x[0] || ld<int32_t>(tmp.x[0]+16)!=int(label.size()))return current;
    for(size_t n=0;n<label.size();++n)st<char16_t>(tmp.x[0]+20+n*2,label[n]);
    return tmp.x[0];
}
int selected(CPU& caller,uint64_t setting) {
    if(!setting) return -1;
    CPU tmp=caller; const auto klass=ld<uint64_t>(setting);
    tmp.x[0]=setting;tmp.x[1]=ld<uint64_t>(klass+0x1a0);
    nx::call(tmp,ld<uint64_t>(klass+0x198));return int(tmp.x[0]);
}
void valid(CPU& caller,uint64_t setting) {
    CPU tmp=caller;const auto klass=ld<uint64_t>(setting);
    tmp.x[0]=setting;tmp.x[1]=1;tmp.x[2]=ld<uint64_t>(klass+0x190);
    nx::call(tmp,ld<uint64_t>(klass+0x188));
}
void window_choices(uint64_t window) {
    const auto fields=static_fields();if(!fields)return;
    const auto fullmodes=ld<uint64_t>(fields+0x10);
    const auto choices=window ? ld<uint64_t>(window+0x58) : 0;
    nx::log("video/opciones: ventana opciones=%u modos=%u", choices ? ld<uint32_t>(choices+24) : 0, fullmodes ? ld<uint32_t>(fullmodes+24) : 0);
    if(window && fullmodes && choices) {
        const auto count=ld<uint32_t>(choices+24);
        if(count<=4 && count==ld<uint32_t>(fullmodes+24)) for(uint32_t n=0;n<count;++n) {
            const auto value=ld<uint64_t>(choices+32+n*8);if(!value)continue;
            const auto index=ld<uint32_t>(value+0x20);if(index>=count)continue;
            const auto mode=ld<uint32_t>(fullmodes+32+index*4);
            st<uint8_t>(value+0x28,mode>=1 && mode<=3);
            nx::log("video/opciones: ventana índice=%u valor=%u modo=%u activo=%u",n,index,mode,unsigned(ld<uint8_t>(value+0x28)));
            if(mode==uint32_t(nx::plat::video_window_mode()))st<uint32_t>(window+0x1c,index);
        }
    }
}
void resolution_choices(CPU& caller,uint64_t setting) {
    const auto fields=static_fields();if(!fields)return;
    const auto modes=ld<uint64_t>(fields);
    const auto possible=setting ? ld<uint64_t>(setting+0x58) : 0;
    if(!modes || !possible)return;
    const auto count=ld<uint32_t>(possible+24);if(count>32 || ld<uint32_t>(modes+24)!=count)return;
    uint32_t nw,nh,w,h;nx::game::video_native_size(nw,nh);nx::game::video_render_size(w,h);
    std::set<std::pair<uint32_t,uint32_t>> unique;
    for(uint32_t n=0;n<count;++n) {
        const auto value=ld<uint64_t>(possible+32+n*8);if(!value)continue;
        const auto index=ld<uint32_t>(value+0x20);if(index>=count)continue;
        const auto tuple=modes+32+index*8;
        const auto x=ld<uint32_t>(tuple),y=ld<uint32_t>(tuple+4);
        if(index+1==count && x==nw && y==nh) {
            const auto text=std::to_string(x)+"x"+std::to_string(y);
            const std::u16string label(text.begin(),text.end());
            st<uint64_t>(value+0x10,managed_label(caller,ld<uint64_t>(value+0x10),label));
        }
        st<uint8_t>(value+0x28,x>=640 && y>=360 && x<=nw && y<=nh && x*9==y*16 && unique.emplace(x,y).second);
        if(x==w && y==h)st<uint32_t>(setting+0x1c,index);
        nx::log("video/opciones: modo %u %u×%u activo=%u",index,x,y,unsigned(ld<uint8_t>(value+0x28)));
    }
}
void resolution_options(CPU& caller,uint64_t self) {
    resolution_choices(caller,ld<uint64_t>(self+0x68));
    window_choices(ld<uint64_t>(self+0x58));
}
}
void nx_castlevania_setting_active(CPU& c,void (*original)(CPU&)) {
    // IsSettingActive es de instancia: x0=this, x1=clave.
    if(key_is(c.x[1],u"video/windowmode") || key_is(c.x[1],u"video/resolution")) c.x[0]=1;
    else original(c);
}
void nx_castlevania_video_text(CPU& c,void (*original)(CPU&)) {
    if(legacy_native_label(c.x[1])) {
        uint32_t w,h;nx::game::video_native_size(w,h);
        if(w!=3840 || h!=2160) {
            const auto text=std::to_string(w)+" x "+std::to_string(h);
            std::u16string label(text.begin(),text.end());label[text.find('x')]=u'×';
            // TMP_Text.m_text conserva la etiqueta ya asignada; no se crea otra cadena en cada refresco.
            c.x[1]=managed_label(c,ld<uint64_t>(c.x[0]+0xd8),label);
            static std::atomic<unsigned> logged{0};if(logged.fetch_add(1)<4)nx::log("video/opciones: etiqueta localizada %u×%u",w,h);
        }
    }
    original(c);
}
void nx_castlevania_video_defaults(CPU& c,void (*original)(CPU&)) {
    original(c);
    const auto fields=static_fields();if(!fields)return;
    uint32_t w,h;nx::game::video_native_size(w,h);
    // Los valores de EnumSetting indexan la tabla general; la tabla de 16:9 también debe conservar la entrada nativa.
    for(unsigned offset : {0u,8u}) {
        const auto resolutions=ld<uint64_t>(fields+offset);if(!resolutions)continue;
        const auto count=ld<uint32_t>(resolutions+24);if(count!=12)continue;
        const auto tuple=resolutions+32+(count-1)*8;
        st<uint32_t>(tuple,w);st<uint32_t>(tuple+4,h);
    }
    nx::log("video/opciones: resolución nativa añadida %u×%u",w,h);
    const auto fm=ld<uint64_t>(fields+16);
    if(fm && ld<uint32_t>(fm+24)==4) nx::log("video/opciones: orden de modos %u,%u,%u,%u",ld<uint32_t>(fm+32),ld<uint32_t>(fm+36),ld<uint32_t>(fm+40),ld<uint32_t>(fm+44));
}
void nx_castlevania_supported_resolutions(CPU& c,void (*original)(CPU&)) {
    const auto self=c.x[0];original(c);resolution_options(c,self);
}
void nx_castlevania_resolution_changed(CPU& c,void (*original)(CPU&)) {
    const auto self=c.x[0];original(c);
    const auto index=selected(c,ld<uint64_t>(self+0x68));
    const auto fields=static_fields();if(!fields)return;
    const auto list=ld<uint64_t>(fields);if(!list || index<0 || uint32_t(index)>=ld<uint32_t>(list+24))return;
    const auto tuple=list+32+index*8;
    const auto w=ld<uint32_t>(tuple),h=ld<uint32_t>(tuple+4);
    if(!nx::game::select_render_size(w,h))return;
    CPU tmp=c;tmp.x[0]=w;tmp.x[1]=h;tmp.x[2]=nx::plat::video_window_mode();tmp.x[3]=0x100000000ull;tmp.x[4]=0;
    nx::call(tmp,base+0x4e00a50);
    if(!initializing)nx::plat::resize_video_window(w,h);
    nx::log("video/opciones: aplicada resolución %u×%u",w,h);
}
void nx_castlevania_fullscreen_mode(CPU& c,void (*)(CPU&)) {c.x[0]=nx::plat::video_window_mode();}
void nx_castlevania_set_fullscreen_mode(CPU& c,void (*original)(CPU&)) {
    const int mode=int(c.x[0]);
    if(initializing)return;
    // Unity reserva el modo exclusivo para Windows; macOS utiliza pantalla completa de escritorio.
    nx::plat::set_video_window_mode(mode==0?1:mode);
    original(c);
    nx::log("video/opciones: modo de pantalla %d (solicitud %d)",nx::plat::video_window_mode(),mode);
}
void nx_castlevania_video_initialize(CPU& c,void (*original)(CPU&)) {
    const auto self=c.x[0];
    const bool previous=initializing;initializing=true;
    try {
        original(c);
        resolution_options(c,self);
        const auto resolution=ld<uint64_t>(self+0x68);
        if(resolution)valid(c,resolution);
        const auto fields=static_fields();
        const auto window=ld<uint64_t>(self+0x58);
        const auto modes=fields ? ld<uint64_t>(fields+16) : 0;
        if(window && modes && ld<uint32_t>(modes+24)==4 && selected(c,window)==ld<int32_t>(window+0x18)) {
            for(unsigned index=0;index<4;++index) if(ld<uint32_t>(modes+32+index*4)==unsigned(nx::plat::video_window_mode())) {
                st<uint32_t>(window+0x18,index);st<uint32_t>(window+0x1c,index);break;
            }
            valid(c,window);
        }
    } catch(...) {initializing=previous;throw;}
    initializing=previous;
}

void nx_castlevania_fullscreen(CPU& c,void (*)(CPU&)) {c.x[0]=nx::plat::video_window_mode()==1;}
#endif
