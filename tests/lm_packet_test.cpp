#include "lm_packet.h"

#include <cstring>
#include <cstdio>
#include <vector>

int main() {
    const std::string message = "Error de inicialización";
    std::vector<uint8_t> packet(26 + message.size());
    packet[8] = 42; packet[16] = 3; packet[18] = 4;
    const uint32_t size = 2 + message.size();
    std::memcpy(packet.data() + 20, &size, sizeof size);
    packet[24] = 2; packet[25] = uint8_t(message.size());
    std::memcpy(packet.data() + 26, message.data(), message.size());
    nx::LmFragment fragment;
    if (!nx::parse_lm_packet(packet, fragment) || fragment.thread != 42 || fragment.flags != 3 ||
        fragment.severity != 4 || fragment.message != message) return 1;
    packet.pop_back();
    if (nx::parse_lm_packet(packet, fragment)) return 2;
    packet.resize(26); packet[20] = 2; packet[25] = 0x80;
    if (nx::parse_lm_packet(packet, fragment)) return 3;
    packet.resize(24); packet[20] = 0;
    if (!nx::parse_lm_packet(packet, fragment) || !fragment.message.empty()) return 4;
    std::puts("Paquetes LM: mensaje UTF-8, cabecera, longitud y ULEB truncado: PASS");
}
