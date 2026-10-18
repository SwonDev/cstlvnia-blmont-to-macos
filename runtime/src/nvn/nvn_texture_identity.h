#pragma once
#include <nx/cpu.h>

namespace nx::nvn {
inline constexpr uint32_t kTextureCopyMagic = 0x4E585458;
inline void stamp_texture_identity(uint64_t guest, uint32_t generation) {
    // NVNtexture ocupa 0x80 bytes opacos; Unity copia el descriptor completo.
    st<uint64_t>(guest + 0x70, uint64_t(generation) << 32 | kTextureCopyMagic);
    st<uint64_t>(guest + 0x78, guest);
}
}
