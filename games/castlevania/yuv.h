#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace nx::game {
inline bool i420_to_nv12(std::span<const uint8_t> src, std::span<uint8_t> dst,
                         uint32_t width, uint32_t height, uint32_t stride, uint32_t coded_height) {
    if (!width || !height || ((width | height | stride | coded_height) & 1) ||
        stride < width || coded_height < height) return false;
    uint64_t y = uint64_t(stride) * coded_height, chroma = y / 4;
    const uint64_t output_y = uint64_t(stride) * height, output_chroma = output_y / 4;
    if (y > src.size() || chroma * 2 > src.size() - y || output_y > dst.size() || output_chroma * 2 > dst.size() - output_y) return false;
    std::vector<uint8_t> snapshot;
    auto a = reinterpret_cast<uintptr_t>(src.data()), b = reinterpret_cast<uintptr_t>(dst.data());
    const uint64_t size = y + chroma * 2;
    if (a <= b ? b - a < size : a - b < size) {
        snapshot.assign(src.begin(), src.begin() + size); src = snapshot;
    }
    std::memcpy(dst.data(), src.data(), output_y);
    for (uint64_t i = 0; i < output_chroma; i++) {
        dst[output_y + i * 2] = src[y + i];
        dst[output_y + i * 2 + 1] = src[y + chroma + i];
    }
    return true;
}
}
