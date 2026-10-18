#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace nx::nvn {

// Offsets de c[2] contrastados con los comandos emitidos por el SDK 21 del juego.
struct TextureBankView {
    std::span<const uint64_t> combined, images, separate_textures, separate_samplers;

    uint32_t word(uint32_t offset) const {
        const uint64_t address = uint64_t(offset) * 4;
        auto read = [&](std::span<const uint64_t> values, uint64_t begin) -> uint32_t {
            if (address < begin || address - begin >= values.size() * 8) return 0;
            const uint64_t relative = address - begin;
            return uint32_t(values[relative / 8] >> ((relative & 4) * 8));
        };
        if (address >= 0x568) return read(separate_samplers, 0x568);
        if (address >= 0x168) return read(separate_textures, 0x168);
        if (address >= 0x120) return read(images.first(images.size() < 8 ? images.size() : 8), 0x120);
        return read(combined, 0x20);
    }

    std::array<uint8_t, 0x1000> bytes() const {
        std::array<uint8_t, 0x1000> result{};
        for (uint32_t offset = 0; offset < result.size() / 4; ++offset) {
            const uint32_t value = word(offset);
            for (uint32_t b = 0; b < 4; ++b) result[offset * 4 + b] = uint8_t(value >> (b * 8));
        }
        return result;
    }
};

// Ryubing TextureHandle: dos bancos y offsets empaquetados; los tipos 2 y 3
// proporcionan un ID de sampler, mientras que el tipo 1 ya contiene sus bits altos.
template <typename ReadWord>
uint64_t read_packed_texture_handle(int slots, uint32_t offsets, ReadWord read) {
    const int texture_bank = slots < 0 ? 2 : slots & 0xFFFF;
    const int sampler_bank = slots < 0 || (uint32_t(slots) >> 16) == 0
        ? texture_bank : int(uint32_t(slots) >> 16) - 1;
    const uint32_t texture_word = offsets & 0x3FFF, sampler_word = (offsets >> 14) & 0x3FFF;
    const uint32_t type = offsets >> 28;
    uint32_t handle = read(texture_bank, texture_word);
    if (type) {
        uint32_t sampler = type == 3 ? sampler_word : read(sampler_bank, sampler_word);
        if (type == 2 || type == 3) sampler <<= 20;
        handle |= sampler;
    }
    return handle;
}

}  // namespace nx::nvn
