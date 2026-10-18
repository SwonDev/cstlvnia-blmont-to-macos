#include <nx/runtime.h>
#include <nx/exception.h>
#include <array>

namespace nx {
namespace {
thread_local ExceptionFrame* top = nullptr;
struct Context {
    ExceptionFrame* frame;
    uint64_t ip;
    std::array<uint64_t, 32> registers{};
    bool installed = false;
};
thread_local Context* current_context = nullptr;

struct ContextScope {
    Context* previous = current_context;
    explicit ContextScope(Context& context) { current_context = &context; }
    ~ContextScope() { current_context = previous; }
};

void original(CPU& cpu) {
    const auto target = find_symbol(hle_current_name());
    if (!target) fatal("Falta implementación original de %s", hle_current_name());
    call(cpu, target);
}

uint32_t invoke_personality(CPU& cpu, ExceptionFrame& frame, uint64_t unwind, uint32_t flags, Context& context) {
    CPU caller = cpu;
    ContextScope scope(context);
    cpu.x[0] = 1;
    cpu.x[1] = flags;
    cpu.x[2] = ld<uint64_t>(unwind);
    cpu.x[3] = unwind;
    cpu.x[4] = reinterpret_cast<uint64_t>(&context);
    try {
        call(cpu, frame.indirect ? ld<uint64_t>(frame.personality) : frame.personality);
    } catch (...) {
        cpu = caller;
        throw;
    }
    const uint32_t result = uint32_t(cpu.x[0]);
    cpu = caller;
    return result;
}

Context make_context(ExceptionFrame& frame) {
    Context context{&frame, frame.pc + 4};
    for (int k = 0; k < 13; k++) context.registers[19 + k] = frame.saved.x[k];
    return context;
}

void raise_exception(CPU& cpu) {
    if (!top) { original(cpu); return; }
    const uint64_t unwind = cpu.x[0];
    for (auto* frame = top; frame; frame = frame->previous) {
        if (!frame->active) continue;
        auto context = make_context(*frame);
        const auto result = invoke_personality(cpu, *frame, unwind, 1, context);
        if (result == 6) {
            static thread_local uint64_t reports = 0;
            if (++reports <= 8) log("eh: excepción %llx, manejador %s, llamada %s", (unsigned long long)unwind,
                                    describe(frame->function).c_str(), describe(frame->pc).c_str());
            throw GuestException{unwind, frame};
        }
        if (result != 8) fatal("Búsqueda de excepción: personality devolvió %u", result);
    }
    cpu.x[0] = 5;  // _URC_END_OF_STACK: __cxa_throw conserva su comportamiento de terminación.
}

void resume_exception(CPU& cpu) {
    for (auto* frame = top; frame; frame = frame->previous)
        for (auto it = frame->pending.rbegin(); it != frame->pending.rend(); ++it)
            if (it->unwind == cpu.x[0]) throw *it;
    original(cpu);
}

void resume_or_rethrow(CPU& cpu) {
    if (top && ld<uint64_t>(cpu.x[0] + 16) == 0) raise_exception(cpu);
    else original(cpu);
}

bool ours(const CPU& cpu) { return current_context && cpu.x[0] == reinterpret_cast<uint64_t>(current_context); }
void get_lsda(CPU& cpu) { if (!ours(cpu)) return original(cpu); cpu.x[0] = current_context->frame->lsda; }
void get_region(CPU& cpu) { if (!ours(cpu)) return original(cpu); cpu.x[0] = current_context->frame->function; }
void get_ip(CPU& cpu) { if (!ours(cpu)) return original(cpu); cpu.x[0] = current_context->ip; }
void get_ip_info(CPU& cpu) {
    if (!ours(cpu)) return original(cpu);
    if (cpu.x[1]) st<uint32_t>(cpu.x[1], 0);
    cpu.x[0] = current_context->ip;
}
void get_cfa(CPU& cpu) { if (!ours(cpu)) return original(cpu); cpu.x[0] = current_context->registers[31]; }
void get_gr(CPU& cpu) {
    if (!ours(cpu)) return original(cpu);
    if (cpu.x[1] >= 32) fatal("Registro de excepción inválido: %llu", (unsigned long long)cpu.x[1]);
    cpu.x[0] = current_context->registers[cpu.x[1]];
}
void set_gr(CPU& cpu) {
    if (!ours(cpu)) return original(cpu);
    if (cpu.x[1] >= 32) fatal("Registro de excepción inválido: %llu", (unsigned long long)cpu.x[1]);
    current_context->registers[cpu.x[1]] = cpu.x[2];
}
void set_ip(CPU& cpu) {
    if (!ours(cpu)) return original(cpu);
    current_context->ip = cpu.x[1];
    current_context->installed = true;
}
}

ExceptionFrame::ExceptionFrame(uint64_t function_, uint64_t lsda_, uint64_t personality_, bool indirect_)
    : previous(top), function(function_), lsda(lsda_), personality(personality_), indirect(indirect_) { top = this; }
ExceptionFrame::~ExceptionFrame() {
    if (top != this) fatal("Pila de excepciones inconsistente");
    top = previous;
}
void ExceptionFrame::before_call(const CPU& cpu, uint64_t pc_) {
    saved.save(cpu);
    pc = pc_;
    active = true;
}
uint64_t ExceptionFrame::enter(CPU& cpu, const GuestException& exception) {
    if (!active) return 0;
    auto context = make_context(*this);
    const uint32_t flags = 2 | (exception.handler == this ? 4 : 0);
    const auto result = invoke_personality(cpu, *this, exception.unwind, flags, context);
    if (result == 8) return 0;
    if (result != 7 || !context.ip || !context.installed)
        fatal("Instalación de excepción inválida: resultado %u", result);
    pending.push_back(exception);
    saved.restore(cpu);
    cpu.x[0] = context.registers[0];
    cpu.x[1] = context.registers[1];
    return context.ip;
}

NX_HLE("_Unwind_RaiseException", raise_exception);
NX_HLE("_Unwind_Resume", resume_exception);
NX_HLE("_Unwind_Resume_or_Rethrow", resume_or_rethrow);
NX_HLE("_Unwind_GetLanguageSpecificData", get_lsda);
NX_HLE("_Unwind_GetRegionStart", get_region);
NX_HLE("_Unwind_GetIP", get_ip);
NX_HLE("_Unwind_GetIPInfo", get_ip_info);
NX_HLE("_Unwind_GetCFA", get_cfa);
NX_HLE("_Unwind_GetGR", get_gr);
NX_HLE("_Unwind_SetGR", set_gr);
NX_HLE("_Unwind_SetIP", set_ip);
}
