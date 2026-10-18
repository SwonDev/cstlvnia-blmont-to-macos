#include "nvn/nvn.h"
#include <array>
#include <map>
#include <string>
#include <cstring>
#undef NDEBUG
#include <cassert>
namespace {
auto& functions(){static std::map<std::string,nx::HleFn> f;return f;}
nx::nvn::ObjectsLock lock;
std::unordered_map<uint64_t,std::unique_ptr<nx::nvn::Object>> registry;
nx::nvn::ViewInfo view;
}
namespace nx {
HleRegistrar::HleRegistrar(const char* name,HleFn fn){functions()[name]=fn;}
void log(const char*,...){}
VmInfo vm_query(uint64_t address){return {address,0x1000,0,0};}
}
namespace nx::nvn {
Texture::~Texture()=default;
ObjectsLock& objects_lock(){return lock;}
std::unordered_map<uint64_t,std::unique_ptr<Object>>& objects(){return registry;}
void defer_object(std::unique_ptr<Object>){}
bool resolve_cache_on(){return false;}
Object* find_object(uint64_t key,uint64_t){auto it=registry.find(key);return it==registry.end()?nullptr:it->second.get();}
ViewInfo read_view(CPU&,uint64_t){return view;}
void format_block(uint32_t,uint32_t& w,uint32_t& h,uint32_t& bytes){w=h=1;bytes=4;}
}
int main(){
    using namespace nx::nvn;
    auto& t=make<Texture>(0x1234);t.target=4;t.width=4;t.height=4;t.depth=16;t.levels=2;
    std::array<int32_t,6> region{0,0,0,2,2,1};std::array<uint8_t,24> data{};
    for(unsigned n=0;n<data.size();++n)data[n]=uint8_t(n+1);
    nx::CPU c{};c.x[0]=0x1234;c.x[1]=1;c.x[2]=reinterpret_cast<uint64_t>(region.data());c.x[3]=reinterpret_cast<uint64_t>(data.data());
    view.min_layer=7;view.layers=1;view.base_level=1;
    functions().at("nvnTextureWriteTexels")(c);
    assert(t.writes.size()==1 && t.writes[0].z==7 && t.writes[0].level==1);
    assert(t.writes[0].data.size()==16 && t.writes[0].data[15]==16);
    view.min_layer=15;view.base_level=0;c.x[4]=12;c.x[5]=24;
    functions().at("nvnTextureWriteTexelsStrided")(c);
    assert(t.writes.size()==2 && t.writes[1].z==15 && t.writes[1].data[8]==13);
    view.min_layer=16;functions().at("nvnTextureWriteTexels")(c);assert(t.writes.size()==2);
    view.min_layer=4;view.layers=4;region={0,0,1,1,1,2};
    functions().at("nvnTextureWriteTexels")(c);
    assert(t.writes.size()==3 && t.writes.back().z==5 && t.writes.back().d==2 && t.writes.back().data.size()==8);
    view.layers=2;functions().at("nvnTextureWriteTexels")(c);assert(t.writes.size()==3);
    view.min_layer=14;view.layers=0;region[2]=0;
    functions().at("nvnTextureWriteTexels")(c);assert(t.writes.size()==4 && t.writes.back().z==14);
    t.target=2;t.depth=4;view.base_level=1;view.min_layer=7;region={0,0,1,1,1,1};
    functions().at("nvnTextureWriteTexels")(c);assert(t.writes.size()==5 && t.writes.back().z==1);
    view.base_level=2;functions().at("nvnTextureWriteTexels")(c);assert(t.writes.size()==5);
}
