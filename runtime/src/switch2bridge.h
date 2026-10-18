#pragma once

#include "platform.h"

#include <array>
#include <span>

namespace nx::plat::switch2bridge {

bool decode(std::span<const uint8_t> frame, PadState& pad, bool& connected);
std::string default_path();

// Un consumidor del socket existente; no configura ni envía órdenes al puente.
class Client {
public:
    explicit Client(std::string path = default_path());
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    bool sample(PadState& pad, bool focused, uint64_t now_ns);

private:
    void disconnect(uint64_t now_ns);
    std::string path_;
    int socket_ = -1;
    std::array<uint8_t, 20> frame_{};
    size_t pending_ = 0;
    uint64_t retry_at_ = 0, received_at_ = 0;
    bool connected_ = false;
    PadState latest_;
};

}  // namespace nx::plat::switch2bridge
