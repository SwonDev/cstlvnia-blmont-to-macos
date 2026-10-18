#include "nvn.h"
#include "nvn_depth_mode.h"
#include <cstdlib>

namespace nx::nvn {
namespace {
std::atomic<unsigned> depth_mode{0};
void DeviceSetDepthMode(CPU& c) {
    const auto mode=unsigned(c.x[1]);
    if(mode>1) {log("nvn: modo de profundidad no válido %u",mode);return;}
    // El driver conserva su propio estado para las consultas originales del SDK.
    driver(c,"nvnDeviceSetDepthMode",{c.x[0],mode});
    depth_mode=mode;
    log("nvn: profundidad solicitada %s",mode==1 ? "0..w" : "-w..w");
}
}
bool device_depth_zero_to_one() {
    static const int forced=[] {
        const char* value=std::getenv("NX_DEPTH_MODE_FORCE");
        return value && value[0]=='0' && !value[1] ? 0 : value && value[0]=='1' && !value[1] ? 1 : -1;
    }();
    return forced>=0 ? forced==1 : depth_mode.load()==1;
}
NX_HLE("nvnDeviceSetDepthMode",DeviceSetDepthMode);
}
