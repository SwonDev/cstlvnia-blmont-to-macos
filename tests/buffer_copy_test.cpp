#include "nvn/nvn_draw_state.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <stdexcept>

namespace {
auto& functions() { static std::unordered_map<std::string, nx::HleFn> f; return f; }
nx::nvn::CmdList recorded;
std::array<uint8_t, 1024> guest{};
std::array<uint8_t, 65536> device{};
nx::nvn::MemoryPool pool;
nx::nvn::ObjectsLock mutex;
uint64_t used = 0;
unsigned copies = 0, barriers = 0, ends = 0, image_copies = 0;
nx::nvn::GpuImage image;
std::array<uint8_t, 256> pixels{};
void VKAPI_PTR copy_image(VkCommandBuffer, VkBuffer, VkImage, VkImageLayout, uint32_t n, const VkBufferImageCopy* r) {
    for (uint32_t k = 0; k < n; k++) {
        std::memcpy(pixels.data(), device.data() + r[k].bufferOffset, r[k].imageExtent.width * r[k].imageExtent.height);
        image_copies++;
    }
}
const uint8_t* gpu_written = nullptr;
nx::nvn::Upload written{};
void VKAPI_PTR copy(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t n, const VkBufferCopy* r) {
    for (uint32_t k = 0; k < n; k++) {
        if (r[k].srcOffset < r[k].dstOffset + r[k].size && r[k].dstOffset < r[k].srcOffset + r[k].size)
            throw std::runtime_error("Vulkan no permite regiones solapadas");
        std::memcpy(device.data() + r[k].dstOffset, device.data() + r[k].srcOffset, r[k].size);
        copies++;
    }
}
void VKAPI_PTR barrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags,
                      uint32_t, const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*,
                      uint32_t, const VkImageMemoryBarrier*) { barriers++; }
}
namespace nx {
HleRegistrar::HleRegistrar(const char* n, HleFn f) { functions()[n] = f; }
void log(const char*, ...) {}
[[noreturn]] void fatal(const char*, ...) { throw std::runtime_error("Copia rechazada"); }
}
namespace nx::vk {
PFN_vkCmdCopyBuffer p_vkCmdCopyBuffer = copy;
PFN_vkCmdCopyBufferToImage p_vkCmdCopyBufferToImage = copy_image;
PFN_vkCmdPipelineBarrier p_vkCmdPipelineBarrier = ::barrier;
}
namespace nx::nvn {
Texture::~Texture() = default;
auto& strides() { static std::unordered_map<uint64_t, std::pair<uint64_t, uint64_t>> s; return s; }
void command_set_copy_stride(uint64_t guest, bool image, uint64_t stride) { (image ? strides()[guest].second : strides()[guest].first) = stride; }
std::pair<uint64_t, uint64_t> command_copy_strides(uint64_t guest) { return strides()[guest]; }
bool resolve_cache_on() { return false; }
Object* find_object(uint64_t address, uint64_t) { auto it = objects().find(address); return it == objects().end() ? nullptr : it->second.get(); }
uint64_t texture_identity_address(uint64_t address) { return address; }
ViewInfo read_view(CPU&, uint64_t) { return {}; }
void format_block(uint32_t, uint32_t& bw, uint32_t& bh, uint32_t& bytes) { bw = bh = bytes = 1; }
bool astc_decoded(uint32_t) { return false; }
bool bc_host_decode(uint32_t) { return false; }
uint32_t bc_decoded_bytes(uint32_t) { return 4; }
void bc_decode(uint32_t, const uint8_t*, uint32_t, uint32_t, uint32_t, uint8_t*) { throw std::runtime_error("Fixture BC no esperado"); }
uint64_t astc_decode_image(const uint8_t*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool, uint8_t*) { throw std::runtime_error("Fixture ASTC no esperado"); }
GpuImage* ensure_contents(VkCommandBuffer, uint64_t) { return &image; }
void transition(VkCommandBuffer, GpuImage& img, VkImageLayout layout) { img.layout = layout; }
ObjectsLock& objects_lock() { return mutex; }
std::unordered_map<uint64_t, std::unique_ptr<Object>>& objects() { static std::unordered_map<uint64_t, std::unique_ptr<Object>> o; return o; }
MemoryPool* pool_at_gpu(uint64_t gpu, uint64_t* off) {
    if (gpu < 0x10000 || gpu - 0x10000 >= guest.size()) return nullptr;
    *off = gpu - 0x10000; return &pool;
}
uint64_t pool_host(const MemoryPool&, uint64_t offset) { return reinterpret_cast<uint64_t>(guest.data() + offset); }
void record(uint64_t, Cmd command, const void* blob, size_t bytes) {
    if (bytes) { command.blob = recorded.blobs.size(); recorded.blobs.emplace_back(static_cast<const uint8_t*>(blob), static_cast<const uint8_t*>(blob) + bytes); }
    recorded.cmds.push_back(command);
}
void draw_end_rendering(VkCommandBuffer, int) { ends++; }
Upload upload_alloc(uint64_t size, uint64_t alignment) {
    used = (used + alignment - 1) / alignment * alignment;
    Upload u{reinterpret_cast<VkBuffer>(1), used, device.data() + used};
    used += size;
    if (used > device.size()) throw std::runtime_error("Fixture sin espacio");
    return u;
}
Upload upload(const void* p, uint64_t size, uint64_t alignment) {
    auto u = upload_alloc(size, alignment); std::memcpy(u.ptr, p, size); return u;
}
}
namespace nx::nvn::replay {
bool written_overlaps(const uint8_t*, uint64_t) { return false; }
Upload read_upload(VkCommandBuffer, const uint8_t* p, uint64_t size, uint64_t, uint64_t alignment) {
    return p == gpu_written ? written : upload(p, size, alignment);
}
Upload written_bind(VkCommandBuffer, const uint8_t*, uint64_t size, uint64_t) {
    written = upload_alloc(size, 256); return written;
}
}
int main() {
    using namespace nx::nvn;
    pool.memory = reinterpret_cast<uint64_t>(guest.data()); pool.size = guest.size();
    if (!functions().count("nvnCommandBufferCopyBufferToBuffer")) {
        std::puts("FALLO: CopyBufferToBuffer no implementada"); return 1;
    }
    nx::CPU c{};
    c.x[0] = 123; c.x[1] = 0x10000; c.x[2] = 0x10100; c.x[3] = 24; c.x[4] = 2;
    functions().at("nvnCommandBufferCopyBufferToBuffer")(c);
    if (recorded.cmds.size() != 1 || copies) return 2;
    for (unsigned i = 0; i < 24; i++) guest[i] = uint8_t(i + 1);
    replay_buffer_copy(nullptr, recorded.cmds.back());
    if (std::memcmp(written.ptr, guest.data(), 24) || guest[256] != 0 || !barriers || !ends) return 3;
    // La GPU tiene valores distintos de la CPU; la copia debe leer los de la GPU.
    gpu_written = guest.data();
    for (unsigned i = 0; i < 24; i++) written.ptr[i] = uint8_t(200 + i);
    replay_buffer_copy(nullptr, recorded.cmds.back());
    if (written.ptr[0] != 200 || written.ptr[23] != 223) return 4;
    // El origen y destino invitados se solapan; las regiones Vulkan deben seguir siendo disjuntas.
    Cmd overlap = recorded.cmds.back(); overlap.a[1] = 0x10004;
    replay_buffer_copy(nullptr, overlap);
    if (written.ptr[0] != 200 || written.ptr[23] != 223) return 5;
    auto old = copies;
    Cmd bad = overlap; bad.a[0] = 0x103F8; bad.a[2] = 24; replay_buffer_copy(nullptr, bad);
    bad = overlap; bad.a[1] = UINT64_MAX; replay_buffer_copy(nullptr, bad);
    bad = overlap; bad.a[2] = 0; replay_buffer_copy(nullptr, bad);
    if (copies != old) return 6;
    gpu_written = nullptr;
    auto texture = std::make_unique<Texture>(); texture->width = 4; texture->height = 2; texture->depth = 1; texture->levels = 1;
    texture->guest_address = 0x55; objects()[0x55] = std::move(texture);
    image.width = 4; image.height = 2; image.layers = 1; image.levels = 1; image.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    const int32_t region[] = {0, 0, 0, 4, 2, 1};
    c.x[0] = 123; c.x[1] = 6; functions().at("nvnCommandBufferSetCopyRowStride")(c);
    c.x[1] = 0x10000; c.x[2] = 0x55; c.x[3] = 0; c.x[4] = reinterpret_cast<uint64_t>(region); c.x[5] = 2;
    functions().at("nvnCommandBufferCopyBufferToTexture")(c);
    for (unsigned i = 0; i < 10; i++) guest[i] = uint8_t(i);
    replay_buffer_texture(nullptr, recorded.cmds.back(), recorded);
    const uint8_t expected[] = {0, 1, 2, 3, 6, 7, 8, 9};
    if (image_copies != 1 || std::memcmp(pixels.data(), expected, sizeof expected) || !image.rendered) return 7;
    const Cmd valid_texture_copy = recorded.cmds.back();
    gpu_written = guest.data(); written = upload_alloc(10, 16);
    for (unsigned i = 0; i < 10; i++) written.ptr[i] = uint8_t(20 + i);
    replay_buffer_texture(nullptr, valid_texture_copy, recorded);
    const uint8_t gpu_expected[] = {20, 21, 22, 23, 26, 27, 28, 29};
    if (image_copies != 2 || std::memcmp(pixels.data(), gpu_expected, sizeof gpu_expected)) return 8;
    gpu_written = nullptr; c.x[0] = 456; c.x[4] = reinterpret_cast<uint64_t>(region);
    functions().at("nvnCommandBufferCopyBufferToTexture")(c);
    if (recorded.cmds.back().a[2] || recorded.cmds.back().a[3]) return 12;
    replay_buffer_texture(nullptr, recorded.cmds.back(), recorded);
    const uint8_t tight_expected[] = {0, 1, 2, 3, 4, 5, 6, 7};
    if (image_copies != 3 || std::memcmp(pixels.data(), tight_expected, 8)) return 13;
    const int32_t bad_region[] = {3, 0, 0, 4, 2, 1}; c.x[4] = reinterpret_cast<uint64_t>(bad_region);
    functions().at("nvnCommandBufferCopyBufferToTexture")(c);
    replay_buffer_texture(nullptr, recorded.cmds.back(), recorded);
    if (image_copies != 3) return 9;
    pool.memory = 0;
    auto physical = std::make_unique<MemoryPool>(); physical->memory = reinterpret_cast<uint64_t>(guest.data()); physical->size = 32;
    objects()[0x66] = std::move(physical); pool.mappings[0] = {0x66, 2, 16};
    if (buffer_copy_bytes(0x10000, 12) != guest.data() + 2 || buffer_copy_bytes(0x10008, 12) || buffer_copy_bytes(0x10020, 4)) return 10;
    pool.mappings[0].physical_offset = 30;
    if (buffer_copy_bytes(0x10000, 4)) return 11;
    std::puts("Copias NVN: grabación diferida, datos GPU, textura con stride, solapamiento y límites físicos/virtuales: PASS");
}
