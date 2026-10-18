#include "nvn_texture_handles.h"

#include <array>
#include <cstdio>

int main() {
    using namespace nx::nvn;
    std::array<uint64_t, 32> combined{}, samplers{};
    std::array<uint64_t, 16> images{};
    std::array<uint64_t, 128> textures{};
    combined[0] = 0x321ABCDE;
    images[0] = 0x111;
    textures[0] = 0x1000ABCDE;
    samplers[0] = 0x12300000;
    TextureBankView view{combined, images, textures, samplers};
    if (view.word(8) != 0x321ABCDE || view.word(72) != 0x111) return 1;
    if (view.word(90) != 0xABCDE || view.word(91) != 1 || view.word(346) != 0x12300000) return 2;
    if (view.word(0xFFFFFFFF) != 0) return 3;
    const auto bytes = view.bytes();
    if (bytes[0x168] != 0xDE || bytes[0x169] != 0xBC || bytes[0x568 + 2] != 0x30) return 4;
    auto read = [&](int bank, uint32_t word) { return bank == 2 ? view.word(word) : 0; };
    if (read_packed_texture_handle(2, 8, read) != 0x321ABCDE) return 5;
    if (read_packed_texture_handle(196610, 0x1056805A, read) != 0x123ABCDE) return 6;
    if (read_packed_texture_handle(196610, 90 | (7 << 14) | (3 << 28), read) != 0x7ABCDE) return 7;
    samplers[0] = 8;
    if (read_packed_texture_handle(196610, 90 | (346 << 14) | (2 << 28), read) != 0x8ABCDE) return 8;
    if (read_packed_texture_handle(-1, 8, read) != 0x321ABCDE) return 9;
    auto different_banks = [](int bank, uint32_t word) -> uint32_t {
        if (bank == 2 && word == 1) return 0xABC;
        if (bank == 3 && word == 2) return 0x32100000;
        return 0;
    };
    if (read_packed_texture_handle(2 | (4 << 16), 1 | (2 << 14) | (1 << 28), different_banks) != 0x32100ABC) return 10;
    std::puts("Bancos NVN y textura/sampler separados: PASS");
}
