#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace nx {

struct LmFragment {
    uint64_t thread = 0;
    uint8_t flags = 0, severity = 0;
    std::string message;
};

inline bool parse_lm_packet(std::span<const uint8_t> packet, LmFragment& output) {
    output = {};
    if (packet.size() < 24) return false;
    uint32_t payload = 0;
    for (int b = 0; b < 4; ++b) payload |= uint32_t(packet[20 + b]) << (b * 8);
    if (payload != packet.size() - 24) return false;
    for (int b = 0; b < 8; ++b) output.thread |= uint64_t(packet[8 + b]) << (b * 8);
    output.flags = packet[16];
    output.severity = packet[18];
    size_t cursor = 24;
    auto read_uleb = [&](uint32_t& value) {
        value = 0;
        for (int shift = 0; shift <= 28; shift += 7) {
            if (cursor == packet.size()) return false;
            const uint8_t byte = packet[cursor++];
            if (shift == 28 && byte > 15) return false;
            value |= uint32_t(byte & 0x7F) << shift;
            if (!(byte & 0x80)) return true;
        }
        return false;
    };
    while (cursor < packet.size()) {
        uint32_t key, length;
        if (!read_uleb(key) || !read_uleb(length) || length > packet.size() - cursor) return false;
        if (key == 2) output.message.append(reinterpret_cast<const char*>(packet.data() + cursor), length);
        cursor += length;
    }
    return true;
}

}  // namespace nx
