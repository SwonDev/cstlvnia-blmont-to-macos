#include <nx/runtime.h>
#include <array>
#include <vector>
#undef NDEBUG
#include <cassert>
void nx_castlevania_loading_material(nx::CPU&,void(*)(nx::CPU&));
namespace {std::vector<char> events;float wanted=0,material_frame=48;uint64_t expected_self=0,expected_material=0;}
namespace nx {
void log(const char*,...){}
void call(CPU& c,uint64_t target) {
    if(target==NX_BASE+0x1dcc470){assert(c.x[0]==expected_self && c.x[1]==0);events.push_back('I');c.v[0].f32[0]=wanted;}
    else {assert(target==NX_BASE+0x4e1aba0 && c.x[0]==expected_material && c.x[1]==99 && c.x[2]==0);events.push_back('M');material_frame=c.v[0].f32[0];}
}
}
void attach(nx::CPU& c){assert(c.x[0]==expected_self && c.x[1]==123 && c.v[0].f32[0]==456);assert(material_frame==wanted);events.push_back('A');}
void no_material(nx::CPU&){events.push_back('A');}
int main(){
    std::array<uint8_t,128> self{},settings{},material{};
    expected_self=reinterpret_cast<uint64_t>(self.data());expected_material=reinterpret_cast<uint64_t>(material.data());
    nx::st<uint64_t>(expected_self+0x38,reinterpret_cast<uint64_t>(settings.data()));
    nx::st<uint64_t>(reinterpret_cast<uint64_t>(settings.data())+0x18,expected_material);
    nx::st<uint32_t>(expected_self+0x48,99);
    nx::CPU c{};c.x[0]=expected_self;c.x[1]=123;c.v[0].f32[0]=456;
    for(float initial:{0.0f,47.0f,12.5f}) {
        wanted=initial;material_frame=48;events.clear();nx_castlevania_loading_material(c,attach);
        assert((events==std::vector<char>{'I','M','A'}));
    }
    events.clear();nx::st<uint64_t>(expected_self+0x38,0);nx_castlevania_loading_material(c,no_material);
    assert((events==std::vector<char>{'A'}));
}
