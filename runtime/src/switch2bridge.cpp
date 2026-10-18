#include "switch2bridge.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

namespace nx::plat::switch2bridge {

bool decode(std::span<const uint8_t> frame, PadState& pad, bool& connected) {
    pad = {};
    connected = false;
    if (frame.size() != 20 || frame[0] != 0x31 || frame[1] != 0x50 ||
        frame[2] != 0x32 || frame[3] != 0x53 || frame[18] > 1) return false;
    connected = frame[18] == 1;
    if (!connected) return true;
    uint32_t raw = 0;
    for (int b = 0; b < 4; ++b) raw |= uint32_t(frame[4 + b]) << (b * 8);
    constexpr uint32_t source[] = {8, 4, 2, 1, 0x800, 0x400, 0x400000, 0x40,
                                    0x800000, 0x80, 0x200, 0x100, 0x80000, 0x20000, 0x40000, 0x10000};
    constexpr uint64_t target[] = {kA, kB, kX, kY, kStickL, kStickR, kL, kR,
                                    kZL, kZR, kPlus, kMinus, kLeft, kUp, kRight, kDown};
    for (int k = 0; k < 16; ++k) if (raw & source[k]) pad.buttons |= target[k];
    if (frame[16] > 127) pad.buttons |= kZL;
    if (frame[17] > 127) pad.buttons |= kZR;
    auto axis = [&](size_t offset, bool invert) {
        int value = int16_t(uint16_t(frame[offset]) | (uint16_t(frame[offset + 1]) << 8));
        value = std::max(value, -kStickMax);
        return invert ? -value : value;
    };
    pad.lx = axis(8, false);
    pad.ly = axis(10, true);
    pad.rx = axis(12, false);
    pad.ry = axis(14, true);
    return true;
}

std::string default_path() {
    if (const char* path = std::getenv("NX_SWITCH2BRIDGE_SOCKET")) return path;
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/Library/Application Support/Switch2Bridge/estado.sock" : "";
}

Client::Client(std::string path) : path_(std::move(path)) {}
Client::~Client() { if (socket_ >= 0) close(socket_); }

void Client::disconnect(uint64_t now_ns) {
    if (socket_ >= 0) close(socket_);
    socket_ = -1;
    pending_ = 0;
    connected_ = false;
    latest_ = {};
    retry_at_ = now_ns + 1000000000;
}

bool Client::sample(PadState& pad, bool focused, uint64_t now_ns) {
    pad = {};
    if (socket_ < 0) {
        if (now_ns < retry_at_) return false;
        sockaddr_un address{};
        if (path_.empty() || path_.size() >= sizeof address.sun_path) return false;
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, path_.c_str(), path_.size() + 1);
        socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (socket_ < 0) { disconnect(now_ns); return false; }
        if (fcntl(socket_, F_SETFL, O_NONBLOCK) < 0 || fcntl(socket_, F_SETFD, FD_CLOEXEC) < 0 ||
            connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof address) < 0) {
            disconnect(now_ns);
            return false;
        }
        received_at_ = now_ns;
    }
    // El servidor emite continuamente: se drena sin bloquear y con trabajo acotado.
    for (int count = 0; count < 64; ++count) {
        const ssize_t bytes = recv(socket_, frame_.data() + pending_, frame_.size() - pending_, 0);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            disconnect(now_ns);
            return false;
        }
        if (bytes == 0) { disconnect(now_ns); return false; }
        pending_ += size_t(bytes);
        if (pending_ == frame_.size()) {
            if (!decode(frame_, latest_, connected_)) { disconnect(now_ns); return false; }
            received_at_ = now_ns;
            pending_ = 0;
        }
    }
    if (now_ns - received_at_ > 250000000) { disconnect(now_ns); return false; }
    if (!connected_) return false;
    if (focused) pad = latest_;
    return true;
}

}  // namespace nx::plat::switch2bridge
