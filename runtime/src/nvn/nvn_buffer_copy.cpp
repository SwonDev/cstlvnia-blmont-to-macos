#include "nvn_draw_state.h"

namespace nx::nvn {
using namespace nx::vk;
namespace {
void CopyBufferToBuffer(CPU& cpu) {
    Cmd c{Cmd::CopyBufferToBuffer};
    c.a[0] = cpu.x[1]; c.a[1] = cpu.x[2]; c.a[2] = cpu.x[3]; c.mask = uint32_t(cpu.x[4]);
    record(cpu.x[0], c);
}
void transfer_barrier(VkCommandBuffer cb) {
    VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    p_vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          0, 1, &b, 0, nullptr, 0, nullptr);
}
}

const uint8_t* buffer_copy_bytes(uint64_t address, uint64_t size) {
    if (!size) return nullptr;
    uint64_t offset = 0;
    auto* pool = pool_at_gpu(address, &offset);
    if (!pool || offset >= pool->size || size > pool->size - offset) return nullptr;
    if (pool->memory) return reinterpret_cast<const uint8_t*>(pool->memory + offset);
    // Una copia no puede atravesar un hueco o una discontinuidad física del pool virtual.
    std::shared_lock lock(objects_lock());
    auto it = pool->mappings.upper_bound(offset);
    if (it == pool->mappings.begin()) return nullptr;
    --it;
    const auto& map = it->second;
    uint64_t inside = offset - it->first;
    if (inside >= map.size || size > map.size - inside) return nullptr;
    auto physical = objects().find(map.physical);
    auto* p = physical == objects().end() ? nullptr : dynamic_cast<MemoryPool*>(physical->second.get());
    if (!p || !p->memory || map.physical_offset > p->size || inside > p->size - map.physical_offset ||
        size > p->size - map.physical_offset - inside) return nullptr;
    return reinterpret_cast<const uint8_t*>(p->memory + map.physical_offset + inside);
}

void replay_buffer_copy(VkCommandBuffer cb, const Cmd& c) {
    const uint64_t size = c.a[2];
    const auto* src = buffer_copy_bytes(c.a[0], size);
    const auto* dst = buffer_copy_bytes(c.a[1], size);
    if (!src || !dst) {
        static unsigned reports = 0;
        if (size && reports++ < 8) log("nvn: copia de búfer fuera de un tramo físico contiguo: %llx -> %llx, %llu bytes",
            (unsigned long long)c.a[0], (unsigned long long)c.a[1], (unsigned long long)size);
        return;
    }
    draw_end_rendering(cb, 5);
    auto source = replay::read_upload(cb, src, size, size, 16);
    // El destino puede compartir la región escrita del origen. La instantánea GPU evita el solapamiento
    // prohibido por vkCmdCopyBuffer y conserva resultados de cómputo que aún no han vuelto a la CPU.
    auto snapshot = upload_alloc(size, 16);
    transfer_barrier(cb);
    VkBufferCopy first{source.offset, snapshot.offset, size};
    p_vkCmdCopyBuffer(cb, source.buffer, snapshot.buffer, 1, &first);
    transfer_barrier(cb);
    auto target = replay::written_bind(cb, dst, size, size);
    transfer_barrier(cb);
    VkBufferCopy second{snapshot.offset, target.offset, size};
    p_vkCmdCopyBuffer(cb, snapshot.buffer, target.buffer, 1, &second);
    transfer_barrier(cb);
}
NX_HLE("nvnCommandBufferCopyBufferToBuffer", CopyBufferToBuffer);
}
