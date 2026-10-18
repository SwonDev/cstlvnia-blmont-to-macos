#include "nvn.h"
#include <algorithm>
#include <cstring>
#include <map>

namespace nx::nvn {
namespace {
// nvnTextureWriteTexels(const NVNtexture*, const NVNtextureView*, const NVNcopyRegion*, const void* data) and the
// Strided form (+ ptrdiff_t rowStride, ptrdiff_t imageStride). NVNcopyRegion: {int x, y, z, width, height, depth}.
// The driver would swizzle the texels into the texture's memory; we keep them for the GPU image instead.
void write_texels(CPU& c, uint64_t row_stride, uint64_t image_stride) {
    auto* t = get<Texture>(c.x[0]);
    uint64_t region = c.x[2], data = c.x[3];
    if (!t || !region || !data) return;
    ViewInfo v = read_view(c, c.x[1]);
    Texture::TexelWrite w;
    w.level = v.base_level;
    w.x = ld<uint32_t>(region), w.y = ld<uint32_t>(region + 4), w.z = ld<uint32_t>(region + 8);
    w.w = ld<uint32_t>(region + 12), w.h = ld<uint32_t>(region + 16), w.d = std::max(1u, ld<uint32_t>(region + 20));
    if(w.level>=t->levels || w.level>=32)return;
    const bool layered=t->target==3 || t->target==4 || t->target==6 || t->target==8 || t->target==9;
    uint32_t slices=layered ? std::max(1u,t->target==3 && t->depth<=1?t->height:t->depth) : t->target==2 ? std::max(1u,t->depth>>w.level) : 1u;
    if(t->target==8 || t->target==9)slices=std::max(6u,slices);
    const uint64_t first=uint64_t(w.z)+(layered?v.min_layer:0);
    if(first>=slices || w.d>slices-first || (layered && v.layers && (w.z>=v.layers || w.d>v.layers-w.z)))return;
    // La región es relativa a la vista; Unity carga cada fotograma de la animación en una vista de una capa.
    w.z=uint32_t(first);
    uint32_t bw = 1, bh = 1, bytes = 4;
    format_block(t->format, bw, bh, bytes);
    uint64_t row = uint64_t((w.w + bw - 1) / bw) * bytes, rows = (w.h + bh - 1) / bh;
    if (!row_stride) row_stride = row;
    if (!image_stride) image_stride = row_stride * rows;
    if (row * rows * w.d > (256ull << 20)) return;
    w.data.resize(row * rows * w.d);
    for (uint32_t z = 0; z < w.d; z++)
        for (uint64_t r = 0; r < rows; r++)
            std::memcpy(w.data.data() + (z * rows + r) * row,
                        reinterpret_cast<const void*>(data + z * image_stride + r * row_stride), row);
    static std::map<uint64_t, uint64_t> counts;
    std::lock_guard lk(objects_lock());
    uint64_t n = ++counts[c.x[0]];
    if (n == 1 || n == 100 || n == 1000)
        log("nvn: WriteTexels #%llu into %llx (%ux%u fmt %#x): %ux%ux%u at %u,%u,%u level %u", (unsigned long long)n,
            (unsigned long long)c.x[0], t->width, t->height, t->format, w.w, w.h, w.d, w.x, w.y, w.z, w.level);
    t->writes.push_back(std::move(w));
    t->has_writes.store(true, std::memory_order_release);
}

void TextureWriteTexels(CPU& c) { write_texels(c, 0, 0); }
void TextureWriteTexelsStrided(CPU& c) { write_texels(c, c.x[4], c.x[5]); }
}
NX_HLE("nvnTextureWriteTexels", TextureWriteTexels);
NX_HLE("nvnTextureWriteTexelsStrided", TextureWriteTexelsStrided);
}
