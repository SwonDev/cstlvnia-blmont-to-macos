// NVN command-buffer binds and state setters beyond the single ones in nvn_state.cpp (docs/NVN.md, N6): the plural
// forms (and their "Deferred" variants, which bind the same way here) record one single bind per element, so the
// replay needs nothing new. Signatures from Wonder's call sites (nvn_trace.txt):
//   BindTextures[Deferred](cmd, stage, first, count, const NVNtextureHandle*)
//   BindUniformBuffers[Deferred] / BindStorageBuffers[Deferred](cmd, stage, first, count, const NVNbufferRange*)
//     with NVNbufferRange {u64 address; u64 size}
//   BindVertexBuffers(cmd, first, count, const NVNbufferRange*)
//   SetViewports(cmd, first, count, const float* {x, y, w, h}), SetScissors(cmd, first, count, const int* {x, y, w, h})
//   SetDepthRanges(cmd, first, count, const float* {near, far})
//   SetRasterizerDiscard(cmd, NVNboolean), SetRenderEnable(cmd, NVNboolean)
// Only viewport/scissor/depth range 0 are replayed (one viewport).
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "nvn.h"
#include <nx/game.h>

namespace nx::nvn {

namespace {

void bind_ranges(CPU& c, Cmd::Op op) {
    int32_t stage = int32_t(c.x[1]), first = int32_t(c.x[2]), count = int32_t(c.x[3]);
    for (int32_t k = 0; k < count && c.x[4]; k++) {
        Cmd cmd{op};
        cmd.i[0] = stage, cmd.i[1] = first + k;
        cmd.a[0] = ld<uint64_t>(c.x[4] + 16ull * k), cmd.a[1] = ld<uint64_t>(c.x[4] + 16ull * k + 8);
        record(c.x[0], cmd);
    }
}

// The Deferred forms keep the array's address: the driver reads it when the commands are submitted, and Wonder fills
// its (double-buffered) handle arrays after recording. Read at recording, the scene sampled the AO, ambient-light and
// shadow textures of two frames earlier: dark actors, a white sky (docs/nvn/DEBUG.md #41).
void BindTextures(CPU& c);
void BindImages(CPU& c);
void BindUniformBuffers(CPU& c);
void BindStorageBuffers(CPU& c);

// Castlevania rellena estas tablas después de grabar. El perfil de cada juego determina el contrato;
// NX_DEFERRED_BINDS permite comparar ambas rutas sin modificar los demás perfiles.
template <uint32_t Kind>
void bind_deferred(CPU& c) {
    static const bool on = [] {
        const char* option=std::getenv("NX_DEFERRED_BINDS");
        return option ? option[0]=='1' : game::kInfo.deferred_bindings;
    }();
    if (!on) {
        if (Kind == 0) BindTextures(c);
        if (Kind == 1) BindImages(c);
        if (Kind == 2) BindUniformBuffers(c);
        if (Kind == 3) BindStorageBuffers(c);
        return;
    }
    if (!c.x[4] || int32_t(c.x[3]) <= 0) return;
    Cmd cmd{Cmd::BindDeferred};
    cmd.mask = Kind;
    cmd.i[0] = int32_t(c.x[1]), cmd.i[1] = int32_t(c.x[2]);
    cmd.n = uint32_t(c.x[3]);
    cmd.a[0] = c.x[4];
    record(c.x[0], cmd);
}

void BindUniformBuffers(CPU& c) { bind_ranges(c, Cmd::BindUniformBuffer); }
void BindStorageBuffers(CPU& c) { bind_ranges(c, Cmd::BindStorageBuffer); }

void BindTextures(CPU& c) {
    int32_t stage = int32_t(c.x[1]), first = int32_t(c.x[2]), count = int32_t(c.x[3]);
    for (int32_t k = 0; k < count && c.x[4]; k++) {
        Cmd cmd{Cmd::BindTexture};
        cmd.i[0] = stage, cmd.i[1] = first + k;
        cmd.a[0] = ld<uint64_t>(c.x[4] + 8ull * k);
        record(c.x[0], cmd);
    }
}

void BindVertexBuffers(CPU& c) {
    int32_t first = int32_t(c.x[1]), count = int32_t(c.x[2]);
    for (int32_t k = 0; k < count && c.x[3]; k++) {
        Cmd cmd{Cmd::BindVertexBuffer};
        cmd.i[0] = first + k;
        cmd.a[0] = ld<uint64_t>(c.x[3] + 16ull * k), cmd.a[1] = ld<uint64_t>(c.x[3] + 16ull * k + 8);
        record(c.x[0], cmd);
    }
}

void SetViewports(CPU& c) {
    if (c.x[1] != 0 || int32_t(c.x[2]) < 1 || !c.x[3]) return;  // viewport 0 only
    Cmd cmd{Cmd::SetViewport};
    for (int k = 0; k < 4; k++) cmd.i[k] = int32_t(std::lround(ld<float>(c.x[3] + 4ull * k)));
    record(c.x[0], cmd);
    static std::atomic<int> logged{0};
    if ((cmd.i[3] < 0 || cmd.i[2] < 0) && logged++ < 10)
        log("nvn: SetViewports with a negative size: %d %d %d %d", cmd.i[0], cmd.i[1], cmd.i[2], cmd.i[3]);
}

void SetScissors(CPU& c) {
    if (c.x[1] != 0 || int32_t(c.x[2]) < 1 || !c.x[3]) return;
    Cmd cmd{Cmd::SetScissor};
    for (int k = 0; k < 4; k++) cmd.i[k] = ld<int32_t>(c.x[3] + 4ull * k);
    record(c.x[0], cmd);
}

void SetDepthRanges(CPU& c) {
    if (c.x[1] != 0 || int32_t(c.x[2]) < 1 || !c.x[3]) return;
    Cmd cmd{Cmd::SetDepthRange};
    cmd.f[0] = ld<float>(c.x[3]), cmd.f[1] = ld<float>(c.x[3] + 4);
    record(c.x[0], cmd);
}

void SetRasterizerDiscard(CPU& c) {
    Cmd cmd{Cmd::SetRasterizerDiscard};
    cmd.n = uint32_t(c.x[1] & 1);
    record(c.x[0], cmd);
}

void SetRenderEnable(CPU& c) {
    Cmd cmd{Cmd::SetRenderEnable};
    cmd.n = uint32_t(c.x[1] & 1);
    record(c.x[0], cmd);
    static std::atomic<uint64_t> counts[2];
    uint64_t n = ++counts[cmd.n];
    if (n == 1 || n % 5000 == 0)
        log("nvn: SetRenderEnable(%u) x%llu", cmd.n, (unsigned long long)n);
}

// SetRenderEnableConditional(cmd, NVNconditionalRenderMode, NVNbufferAddress): rendering on or off from values the
// GPU wrote (occlusion results). Treated as "on": drawing what might have been culled is only extra work, while
// staying off after a SetRenderEnable(false) dropped whole passes.
void SetRenderEnableConditional(CPU& c) {
    Cmd cmd{Cmd::SetRenderEnable};
    cmd.n = 1;
    record(c.x[0], cmd);
}

// BindImage(cmd, stage, index, NVNimageHandle), BindImages[Deferred](cmd, stage, first, count, const NVNimageHandle*)
void BindImage(CPU& c) {
    Cmd cmd{Cmd::BindImage};
    cmd.i[0] = int32_t(c.x[1]), cmd.i[1] = int32_t(c.x[2]);
    cmd.a[0] = c.x[3];
    record(c.x[0], cmd);
}

void BindImages(CPU& c) {
    int32_t stage = int32_t(c.x[1]), first = int32_t(c.x[2]), count = int32_t(c.x[3]);
    for (int32_t k = 0; k < count && c.x[4]; k++) {
        Cmd cmd{Cmd::BindImage};
        cmd.i[0] = stage, cmd.i[1] = first + k;
        cmd.a[0] = ld<uint64_t>(c.x[4] + 8ull * k);
        record(c.x[0], cmd);
    }
}

// DispatchCompute(cmd, int x, int y, int z); DispatchComputeIndirect(cmd, NVNbufferAddress of {u32 x, y, z}),
// read at replay (the GPU may have written it).
void DispatchCompute(CPU& c) {
    Cmd cmd{Cmd::DispatchCompute};
    cmd.i[0] = int32_t(c.x[1]), cmd.i[1] = int32_t(c.x[2]), cmd.i[2] = int32_t(c.x[3]);
    record(c.x[0], cmd);
}

void DispatchComputeIndirect(CPU& c) {
    Cmd cmd{Cmd::DispatchCompute};
    cmd.n = 1;
    cmd.a[0] = c.x[1];
    record(c.x[0], cmd);
}

}  // namespace

NX_HLE("nvnCommandBufferBindImage", BindImage);
NX_HLE("nvnCommandBufferBindImages", BindImages);
NX_HLE("nvnCommandBufferBindImagesDeferred", bind_deferred<1>);
NX_HLE("nvnCommandBufferDispatchCompute", DispatchCompute);
NX_HLE("nvnCommandBufferDispatchComputeIndirect", DispatchComputeIndirect);
NX_HLE("nvnCommandBufferBindUniformBuffers", BindUniformBuffers);
NX_HLE("nvnCommandBufferBindUniformBuffersDeferred", bind_deferred<2>);
NX_HLE("nvnCommandBufferBindStorageBuffers", BindStorageBuffers);
NX_HLE("nvnCommandBufferBindStorageBuffersDeferred", bind_deferred<3>);
NX_HLE("nvnCommandBufferBindTextures", BindTextures);
NX_HLE("nvnCommandBufferBindTexturesDeferred", bind_deferred<0>);
NX_HLE("nvnCommandBufferBindVertexBuffers", BindVertexBuffers);
NX_HLE("nvnCommandBufferSetViewports", SetViewports);
NX_HLE("nvnCommandBufferSetScissors", SetScissors);
NX_HLE("nvnCommandBufferSetDepthRanges", SetDepthRanges);
NX_HLE("nvnCommandBufferSetRasterizerDiscard", SetRasterizerDiscard);
NX_HLE("nvnCommandBufferSetRenderEnable", SetRenderEnable);
NX_HLE("nvnCommandBufferSetRenderEnableConditional", SetRenderEnableConditional);

}  // namespace nx::nvn
