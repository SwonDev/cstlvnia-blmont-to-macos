#include "nvn/nvn.h"
#include <nx/game.h>
#include <array>
#include <map>
#include <string>
#include <cstdlib>
#undef NDEBUG
#include <cassert>
namespace {
auto& functions(){static std::map<std::string,nx::HleFn> v;return v;}
auto recorded=std::make_shared<nx::nvn::CmdList>();
}
namespace nx {
HleRegistrar::HleRegistrar(const char* name,HleFn fn){functions()[name]=fn;}
void log(const char*,...){}
namespace game {const Info kInfo={"fixture","fixture",0,"fixture",".",0,false,false,false,true};}
}
namespace nx::nvn {
void record(uint64_t,Cmd cmd,const void*,size_t){recorded->cmds.push_back(cmd);}
std::shared_ptr<CmdList> command_list(uint64_t){return {};}
}
int main(int argc,char**){
    const bool deferred=argc==1;
    if(deferred)unsetenv("NX_DEFERRED_BINDS");else setenv("NX_DEFERRED_BINDS","0",1);
    std::array<uint64_t,4> ranges{100,16,200,32};
    nx::CPU c{};c.x[0]=42;c.x[1]=1;c.x[2]=0;c.x[3]=2;c.x[4]=reinterpret_cast<uint64_t>(ranges.data());
    functions().at("nvnCommandBufferBindUniformBuffersDeferred")(c);
    ranges={900,64,1000,128};
    const auto snapshot=nx::nvn::deferred_snapshot({recorded});
    if(deferred){
        assert(recorded->cmds.size()==1 && recorded->cmds[0].op==nx::nvn::Cmd::BindDeferred);
        const auto& bytes=snapshot.at(reinterpret_cast<uint64_t>(ranges.data()));
        assert(bytes.size()==4 && bytes[0]==900 && bytes[1]==64 && bytes[2]==1000 && bytes[3]==128);
        ranges[0]=0;assert(bytes[0]==900);
    }else{
        assert(recorded->cmds.size()==2 && snapshot.empty());
        assert(recorded->cmds[0].a[0]==100 && recorded->cmds[1].a[0]==200);
    }
}
