#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace nx::nvn {
inline bool quad_indices(uint32_t count, const uint8_t* indices, uint32_t type, std::vector<uint32_t>& out) {
    if (count > (1u << 24) || (indices && type > 2)) return false;
    out.clear(); out.reserve(uint64_t(count / 4) * 6);
    auto at = [&](uint32_t i) {
        if (!indices) return i;
        uint32_t value = 0, bytes = 1u << type;
        std::memcpy(&value, indices + uint64_t(i) * bytes, bytes);
        return value;
    };
    for (uint32_t base = 0; base + 3 < count; base += 4) {
        const uint32_t a = at(base), b = at(base + 1), c = at(base + 2), d = at(base + 3);
        out.insert(out.end(), {a, b, c, a, c, d});
    }
    return true;
}
}
