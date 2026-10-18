#include <nx/game.h>
#include <nx/runtime.h>

#include "kernel.h"

namespace nx {

// ---- svcGetInfo -------------------------------------------------------------------------------------------------

uint64_t get_info(uint32_t type, uint32_t handle, uint64_t sub, uint32_t& rc) {
    (void)handle;
    rc = 0;
    switch (type) {
        case 0: return 0xF;                         // AllowedCpuIdBitmask
        case 1: return 0xFFFFFFFFFFFFFFFFull >> 0;  // AllowedThreadPriorityMask
        case 2: return kAliasBase;
        case 3: return kAliasSize;
        case 4: return kHeapBase;
        case 5: return kHeapMax;
        case 6: return 0xCD500000;  // TotalMemoryAvailable (~3.2 GiB, like an application)
        case 7: return heap_size() + 0x4000000;
        case 8: return 0;  // IsCurrentProcessBeingDebugged
        case 9: return 0;  // ResourceLimit handle
        case 10: return 0;  // IdleTickCount
        case 11: return 0x1234567890ABCDEFull ^ sub;  // RandomEntropy
        case 12: return kAddressSpaceBase;
        case 13: return kAddressSpaceSize;
        case 14: return kStackBase;
        case 15: return kStackSize;
        case 16: return game::kInfo.system_resource_size;  // SystemResourceSizeTotal
        case 17: return 0;
        case 18: return game::kInfo.program_id;  // ProgramId
        case 20: {  // UserExceptionContextAddress: a page of ours (SDK 16 keeps its dying-message region at +0x1C0)
            static uint64_t ctx = kernel_alloc(0x1000);
            return ctx;
        }
        case 21: return 0xCD500000;
        case 22: return heap_size() + 0x4000000;
        case 23: return 1;  // IsApplication
        case 24:  // FreeThreadCount: presupuesto real que aplica CreateThread.
            if (sub != 0) { rc = 0xE801; return 0; }
            return free_thread_count();
        case 28:  // AliasRegionExtraSize: el host reserva únicamente kAliasSize, sin extensión adicional.
            if (sub != 0) rc = 0xE801;  // InvalidCombination
            return 0;
        case 0xF0000002: return 0;  // ThreadTickCount
        default:
            log("svcGetInfo: unknown type %u sub %llu", type, (unsigned long long)sub);
            rc = 0xF001;
            return 0;
    }
}

}  // namespace nx
