#include "nvn/nvn.h"
#include "nvn/nvn_depth_mode.h"
#include <map>
#include <string>
#include <cstdlib>
#undef NDEBUG
#include <cassert>
namespace {
auto& functions(){static std::map<std::string,nx::HleFn> f;return f;}
unsigned driver_mode=0,calls=0;
}
namespace nx {
HleRegistrar::HleRegistrar(const char* name,HleFn fn){functions()[name]=fn;}
void log(const char*,...){}
}
namespace nx::nvn {
uint64_t real_proc(CPU&,const char* name){assert(std::string(name)=="nvnDeviceSetDepthMode");return 123;}
uint64_t guest_call(CPU&,uint64_t fn,std::initializer_list<uint64_t> args){
    assert(fn==123 && args.size()==2 && *args.begin()==42);driver_mode=*(args.begin()+1);++calls;return 0;
}
}
int main(){
    unsetenv("NX_DEPTH_MODE_FORCE");
    nx::CPU c{};c.x[0]=42;c.x[1]=1;c.x[19]=97;
    assert(!nx::nvn::device_depth_zero_to_one());
    functions().at("nvnDeviceSetDepthMode")(c);
    assert(nx::nvn::device_depth_zero_to_one() && driver_mode==1 && calls==1 && c.x[0]==42 && c.x[19]==97);
    c.x[1]=0;functions().at("nvnDeviceSetDepthMode")(c);
    assert(!nx::nvn::device_depth_zero_to_one() && driver_mode==0 && calls==2);
    c.x[1]=55;functions().at("nvnDeviceSetDepthMode")(c);assert(!nx::nvn::device_depth_zero_to_one() && calls==2);
}
