#include "nvn_quads.h"
#include <array>
#include <cstdio>
int main() {
    std::vector<uint32_t> indices;
    const std::vector<uint32_t> expected = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    if (!nx::nvn::quad_indices(9, nullptr, 0, indices) || indices != expected) return 1;
    const uint8_t u8[] = {10, 11, 12, 13};
    if (!nx::nvn::quad_indices(4, u8, 0, indices) || indices != std::vector<uint32_t>{10, 11, 12, 10, 12, 13}) return 2;
    const uint16_t u16[] = {1000, 1001, 1002, 1003};
    if (!nx::nvn::quad_indices(4, reinterpret_cast<const uint8_t*>(u16), 1, indices) || indices[5] != 1003) return 3;
    const uint32_t u32[] = {100000, 100001, 100002, 100003};
    if (!nx::nvn::quad_indices(4, reinterpret_cast<const uint8_t*>(u32), 2, indices) || indices[5] != 100003) return 4;
    if (nx::nvn::quad_indices(UINT32_MAX, nullptr, 0, indices)) return 5;
    if (!nx::nvn::quad_indices(3, nullptr, 0, indices) || !indices.empty()) return 6;
    std::puts("Cuadriláteros NVN: ambos triángulos, orden, múltiples grupos e índices de 8/16/32 bits: PASS");
}
