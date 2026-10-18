#pragma once
#include <nx/cpu.h>
#include <vector>

namespace nx {
class ExceptionFrame;
struct GuestException {
    uint64_t unwind;
    ExceptionFrame* handler;
};

class ExceptionFrame {
public:
    ExceptionFrame(uint64_t function, uint64_t lsda, uint64_t personality, bool indirect);
    ~ExceptionFrame();
    ExceptionFrame(const ExceptionFrame&) = delete;
    ExceptionFrame& operator=(const ExceptionFrame&) = delete;
    void before_call(const CPU& cpu, uint64_t pc);
    void deactivate() { active = false; }
    uint64_t enter(CPU& cpu, const GuestException& exception);

    ExceptionFrame* previous;
    uint64_t function, lsda, personality, pc = 0;
    bool indirect, active = false;
    JmpState saved;
    std::vector<GuestException> pending;
};
}
