#include "nvn_draw_state.h"
#include "nvn_texture_identity.h"
#include "nvn_astc.h"
#include <algorithm>
#include <cstring>

namespace nx::nvn {
using namespace nx::vk;
namespace {
struct BufferTextureCopy { ViewInfo view; int32_t region[6] = {}; };
void CopyBufferToTexture(CPU& cpu) {
    if (!cpu.x[4]) return;
    BufferTextureCopy info;
    info.view = read_view(cpu, cpu.x[3]);
    std::memcpy(info.region, reinterpret_cast<const void*>(cpu.x[4]), sizeof info.region);
    Cmd c{Cmd::CopyBufferToTexture};
    c.a[0] = cpu.x[1]; c.a[1] = texture_identity_address(cpu.x[2]); c.mask = uint32_t(cpu.x[5]);
    const auto strides = command_copy_strides(cpu.x[0]); c.a[2] = strides.first; c.a[3] = strides.second;
    record(cpu.x[0], c, &info, sizeof info);
}
template <unsigned Which> void SetCopyStride(CPU& cpu) {
    command_set_copy_stride(cpu.x[0], Which, cpu.x[1]);
}
template <unsigned Which> void GetCopyStride(CPU& cpu) {
    const auto strides = command_copy_strides(cpu.x[0]); cpu.x[0] = Which ? strides.second : strides.first;
}
void memory_barrier(VkCommandBuffer cb) {
    VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    p_vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          0, 1, &b, 0, nullptr, 0, nullptr);
}
}

void replay_buffer_texture(VkCommandBuffer cb, const Cmd& c, const CmdList& list) {
    if (c.blob >= list.blobs.size() || list.blobs[c.blob].size() != sizeof(BufferTextureCopy)) return;
    BufferTextureCopy info;
    std::memcpy(&info, list.blobs[c.blob].data(), sizeof info);
    auto* t = get<Texture>(c.a[1]);
    if (!t) return;
    for (int i = 0; i < 6; i++) if (info.region[i] < 0 || (i >= 3 && !info.region[i])) return;
    const auto x = uint32_t(info.region[0]), y = uint32_t(info.region[1]), z = uint32_t(info.region[2]);
    const auto w = uint32_t(info.region[3]), h = uint32_t(info.region[4]), d = uint32_t(info.region[5]);
    const uint32_t level = info.view.base_level;
    if (level >= t->levels || level >= 32) return;
    const uint32_t width = std::max(t->width >> level, 1u), height = std::max(t->height >> level, 1u);
    if (x >= width || y >= height || w > width - x || h > height - y) return;
    uint32_t bw = 1, bh = 1, bytes = 4;
    format_block(t->format, bw, bh, bytes);
    if (!bw || !bh || !bytes || x % bw || y % bh || (w % bw && x + w != width) || (h % bh && y + h != height)) return;
    uint64_t row = uint64_t((w + bw - 1) / bw) * bytes, rows = (h + bh - 1) / bh;
    uint64_t rs = c.a[2] ? c.a[2] : row;
    if (rs < row || rs > (256ull << 20) || rows > (256ull << 20) / rs) return;
    uint64_t is = c.a[3] ? c.a[3] : rs * rows;
    if (is < rs * rows || is > (256ull << 20) || d > (256ull << 20) / is) return;
    uint64_t size = (d - 1) * is + (rows - 1) * rs + row, packed_size = row * rows * d;
    const uint8_t* src = buffer_copy_bytes(c.a[0], size);
    if (!src) return;
    draw_end_rendering(cb, 5);
    GpuImage* img = ensure_contents(cb, c.a[1]);
    if (!img || img->aspect != VK_IMAGE_ASPECT_COLOR_BIT) return;
    const bool is3d = img->view_type == VK_IMAGE_VIEW_TYPE_3D;
    uint32_t slices = is3d ? std::max(t->depth >> level, 1u) : img->layers;
    uint64_t first = uint64_t(z) + (is3d ? 0 : info.view.min_layer);
    if (first >= slices || d > slices - first) return;
    bool converted = img->alpha_from_r || astc_decoded(t->format) || bc_host_decode(t->format) ||
        (t->format == 0x3f && img->format == VK_FORMAT_R16G16B16A16_SFLOAT);
    Upload packed;
    if (converted) {
        if (replay::written_overlaps(src, size)) {
            log("nvn: conversión CPU de copia con origen aún escrito por GPU no disponible (formato %#x)", t->format);
            return;
        }
        std::vector<uint8_t> input(packed_size);
        for (uint32_t slice = 0; slice < d; slice++) for (uint64_t r = 0; r < rows; r++)
            std::memcpy(input.data() + (slice * rows + r) * row, src + slice * is + r * rs, row);
        if (astc_decoded(t->format)) {
            packed = upload_alloc(uint64_t(w) * h * d * 4, 16);
            astc_decode_image(input.data(), w, h, d, bw, bh, t->format >= 0x87, packed.ptr);
        } else if (bc_host_decode(t->format)) {
            packed = upload_alloc(uint64_t(w) * h * d * bc_decoded_bytes(t->format), 16);
            bc_decode(t->format, input.data(), w, h, d, packed.ptr);
        } else if (img->alpha_from_r) {
            packed = upload_alloc(uint64_t(w) * h * d * 4, 16);
            for (uint64_t i = 0; i < uint64_t(w) * h * d; i++) {
                uint32_t value = 0xFFFFFFu | uint32_t(input[i]) << 24;
                std::memcpy(packed.ptr + i * 4, &value, 4);
            }
        } else {
            packed = upload_alloc(uint64_t(w) * h * d * 8, 16);
            for (uint64_t i = 0; i < uint64_t(w) * h * d; i++) {
                uint32_t v; std::memcpy(&v, input.data() + i * 4, 4);
                uint16_t pixel[] = {uint16_t((v & 0x7FF) << 4), uint16_t(((v >> 11) & 0x7FF) << 4),
                                   uint16_t(((v >> 22) & 0x3FF) << 5), 0x3C00};
                std::memcpy(packed.ptr + i * 8, pixel, 8);
            }
        }
    } else {
        auto source = replay::read_upload(cb, src, size, size, 16);
        packed = upload_alloc(packed_size, 16);
        memory_barrier(cb);
        std::vector<VkBufferCopy> copies; copies.reserve(1024);
        auto flush = [&] {
            if (copies.empty()) return;
            p_vkCmdCopyBuffer(cb, source.buffer, packed.buffer, uint32_t(copies.size()), copies.data()); copies.clear();
        };
        if (rs == row && is == row * rows) {
            copies.push_back({source.offset, packed.offset, packed_size});
        } else {
            for (uint32_t slice = 0; slice < d; slice++) for (uint64_t r = 0; r < rows; r++) {
                copies.push_back({source.offset + slice * is + r * rs, packed.offset + (slice * rows + r) * row, row});
                if (copies.size() == 1024) flush();
            }
        }
        flush();
        memory_barrier(cb);
    }
    transition(cb, *img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy{};
    copy.bufferOffset = packed.offset;
    copy.imageSubresource = {img->aspect, level, is3d ? 0u : uint32_t(first), is3d ? 1u : d};
    copy.imageOffset = {int32_t(x), int32_t(y), is3d ? int32_t(first) : 0};
    copy.imageExtent = {w, h, is3d ? d : 1};
    p_vkCmdCopyBufferToImage(cb, packed.buffer, img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    img->rendered = img->uploaded = true;
    memory_barrier(cb);
}
NX_HLE("nvnCommandBufferCopyBufferToTexture", CopyBufferToTexture);
NX_HLE("nvnCommandBufferSetCopyRowStride", SetCopyStride<0>);
NX_HLE("nvnCommandBufferSetCopyImageStride", SetCopyStride<1>);
NX_HLE("nvnCommandBufferGetCopyRowStride", GetCopyStride<0>);
NX_HLE("nvnCommandBufferGetCopyImageStride", GetCopyStride<1>);
}
