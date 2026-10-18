#include "switch2bridge.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "Fallo en línea %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main() {
    using namespace nx::plat;
    std::array<uint8_t, 20> frame{0x31, 0x50, 0x32, 0x53};
    frame[18] = 1;
    const uint32_t raw[] = {8, 4, 2, 1, 0x800, 0x400, 0x400000, 0x40, 0x800000, 0x80,
                            0x200, 0x100, 0x80000, 0x20000, 0x40000, 0x10000};
    const uint64_t expected[] = {kA, kB, kX, kY, kStickL, kStickR, kL, kR, kZL, kZR,
                                 kPlus, kMinus, kLeft, kUp, kRight, kDown};
    PadState pad;
    bool connected = false;
    for (int k = 0; k < 16; ++k) {
        for (int b = 0; b < 4; ++b) frame[4 + b] = uint8_t(raw[k] >> (b * 8));
        CHECK(switch2bridge::decode(frame, pad, connected));
        CHECK(connected && pad.buttons == expected[k]);
    }
    std::memset(frame.data() + 4, 0, 4);
    frame[8] = 0; frame[9] = 0x80;
    frame[10] = 0xFF; frame[11] = 0x7F;
    frame[12] = 1; frame[16] = 255; frame[17] = 255;
    CHECK(switch2bridge::decode(frame, pad, connected));
    CHECK(pad.lx == -32767 && pad.ly == -32767 && pad.rx == 1 && pad.ry == 0);
    CHECK(pad.buttons == (kZL | kZR));
    frame[18] = 0;
    CHECK(switch2bridge::decode(frame, pad, connected) && !connected && !pad.buttons && !pad.lx);
    frame[18] = 2;
    CHECK(!switch2bridge::decode(frame, pad, connected));
    frame[18] = 1; frame[0] = 0;
    CHECK(!switch2bridge::decode(frame, pad, connected));
    frame[0] = 0x31;

    char temporary[] = "/tmp/castlevania-mando-XXXXXX";
    CHECK(mkdtemp(temporary));
    const std::string path = std::string(temporary) + "/estado.sock";
    const int server = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(server >= 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    CHECK(bind(server, reinterpret_cast<sockaddr*>(&address), sizeof address) == 0);
    CHECK(listen(server, 2) == 0);
    switch2bridge::Client client(path);
    CHECK(!client.sample(pad, true, 0));
    int peer = accept(server, nullptr, nullptr);
    CHECK(peer >= 0);
    CHECK(write(peer, frame.data(), 7) == 7);
    CHECK(!client.sample(pad, true, 1));
    CHECK(write(peer, frame.data() + 7, 13) == 13);
    CHECK(client.sample(pad, true, 2) && pad.lx == -32767);
    CHECK(client.sample(pad, false, 3) && !pad.lx && !pad.buttons);
    CHECK(client.sample(pad, true, 4) && pad.lx == -32767);
    frame[4] = 8; frame[8] = 13; frame[9] = 0;
    CHECK(write(peer, frame.data(), 20) == 20);
    frame[4] = 4;
    CHECK(write(peer, frame.data(), 20) == 20);
    CHECK(client.sample(pad, true, 5) && pad.lx == 13 && (pad.buttons & kB) && !(pad.buttons & kA));
    CHECK(!client.sample(pad, true, 300000006) && !pad.buttons && !pad.lx);
    close(peer);
    CHECK(!client.sample(pad, true, 1400000000));
    peer = accept(server, nullptr, nullptr);
    CHECK(peer >= 0);
    CHECK(write(peer, frame.data(), 20) == 20);
    CHECK(client.sample(pad, true, 1400000001) && (pad.buttons & kB));
    close(peer);
    CHECK(!client.sample(pad, true, 1400000002) && !pad.buttons);
    close(server);
    unlink(path.c_str());
    rmdir(temporary);
    std::puts("Botones Nintendo, ejes, tramas parciales, foco, caducidad y reconexión: PASS");
}
