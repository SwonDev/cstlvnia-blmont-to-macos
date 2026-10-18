#include "nvn.h"

namespace nx::nvn {
namespace {

template <bool Sampler, bool Bulk, bool Deferred = false>
void bind_separate(CPU& c) {
    const int stage = int32_t(c.x[1]), first = int32_t(c.x[2]);
    const int count = Bulk ? int32_t(c.x[3]) : 1;
    constexpr int limit = Sampler ? 32 : 128;
    if (stage < 0 || stage >= 6 || count < 0 || count > limit || first < 0 || first > limit - count)
        fatal("Binding NVN separado fuera de rango");
    if constexpr (Deferred) {
        if (!count) return;
        if (!c.x[4]) fatal("Tabla NVN diferida ausente");
        Cmd command{Cmd::BindDeferred};
        command.i[0] = stage, command.i[1] = first;
        command.n = count, command.mask = Sampler ? 5 : 4;
        command.a[0] = c.x[4];
        record(c.x[0], command);
    } else {
        if (Bulk && count && !c.x[4]) fatal("Tabla NVN separada ausente");
        for (int n = 0; n < count; ++n) {
            Cmd command{Sampler ? Cmd::BindSeparateSampler : Cmd::BindSeparateTexture};
            command.i[0] = stage, command.i[1] = first + n;
            command.a[0] = Bulk ? ld<uint64_t>(c.x[4] + uint64_t(n) * 8) : c.x[3];
            record(c.x[0], command);
        }
    }
}

void separate_texture(CPU& c) { c.x[0] = (c.x[1] & 0xFFFFF) | 0x100000000ull; }
void separate_sampler(CPU& c) { c.x[0] = (c.x[1] & 0xFFF) << 20; }

}  // namespace

NX_HLE("nvnDeviceGetSeparateTextureHandle", separate_texture);
NX_HLE("nvnDeviceGetSeparateSamplerHandle", separate_sampler);
NX_HLE("nvnCommandBufferBindSeparateTexture", (bind_separate<false, false>));
NX_HLE("nvnCommandBufferBindSeparateSampler", (bind_separate<true, false>));
NX_HLE("nvnCommandBufferBindSeparateTextures", (bind_separate<false, true>));
NX_HLE("nvnCommandBufferBindSeparateSamplers", (bind_separate<true, true>));
NX_HLE("nvnCommandBufferBindSeparateTexturesDeferred", (bind_separate<false, true, true>));
NX_HLE("nvnCommandBufferBindSeparateSamplersDeferred", (bind_separate<true, true, true>));

}  // namespace nx::nvn
