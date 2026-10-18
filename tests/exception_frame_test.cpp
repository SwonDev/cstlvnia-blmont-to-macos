#include <nx/runtime.h>
#include <nx/exception.h>
#undef NDEBUG
#include <cassert>
#include <map>
#include <string>
#include <vector>

namespace {
auto& registry() { static std::map<std::string, nx::HleFn> r; return r; }
std::vector<int> phases;
bool handled = true;
uint64_t header[4] = {0x434c4e47432b2b00ull};
void invoke(const char* name, nx::CPU& c) { assert(registry().contains(name)); registry().at(name)(c); }
uint64_t pointer(const void* value) { return reinterpret_cast<uint64_t>(value); }
}
namespace nx {
HleRegistrar::HleRegistrar(const char* name, HleFn fn) { registry()[name] = fn; }
void log(const char*, ...) {}
[[noreturn]] void fatal(const char*, ...) { std::abort(); }
uint64_t find_symbol(const char*) { return 99; }
const char* hle_current_name() { return "_Unwind_GetIP"; }
std::string describe(uint64_t) { return "fixture"; }
void call(CPU& c, uint64_t target) {
    if (target == 99) { c.x[0] = 123; return; }
    assert(target == 42 && c.x[0] == 1);
    uint64_t flags = c.x[1], context = c.x[4], exception = c.x[3];
    c.x[0] = context; invoke("_Unwind_GetLanguageSpecificData", c);
    uint64_t lsda = c.x[0];
    c.x[0] = context; invoke("_Unwind_GetIP", c); assert(c.x[0] == (lsda == 10 ? 104 : 204));
    if (flags == 1) { phases.push_back(lsda == 10 ? 1 : 2); c.x[0] = lsda == 20 && handled ? 6 : 8; return; }
    assert(flags == (lsda == 20 ? 6 : 2));
    phases.push_back(lsda == 10 ? 3 : 4);
    c.x[0] = context; c.x[1] = 0; c.x[2] = exception; invoke("_Unwind_SetGR", c);
    c.x[0] = context; c.x[1] = 1; c.x[2] = lsda == 20 ? 7 : 0; invoke("_Unwind_SetGR", c);
    c.x[0] = context; c.x[1] = lsda == 10 ? 150 : 250; invoke("_Unwind_SetIP", c);
    c.x[0] = 7;
}
}
int main() {
    nx::CPU c{};
    c.x[19] = 77; c.x[31] = 1000;
    {
        nx::ExceptionFrame outer(200, 20, 42, false);
        outer.before_call(c, 200);
        try {
            nx::ExceptionFrame inner(100, 10, 42, false);
            inner.before_call(c, 100);
            try { c.x[0] = pointer(header); invoke("_Unwind_RaiseException", c); assert(false); }
            catch (const nx::GuestException& e) {
                c.x[19] = 1; c.x[31] = 5;
                assert(inner.enter(c, e) == 150 && c.x[0] == pointer(header) && c.x[1] == 0);
                assert(c.x[19] == 77 && c.x[31] == 1000);
                c.x[0] = pointer(header); invoke("_Unwind_Resume", c); assert(false);
            }
        } catch (const nx::GuestException& e) {
            assert(outer.enter(c, e) == 250 && c.x[1] == 7);
        }
    }
    assert((phases == std::vector<int>{1, 2, 3, 4}));
    phases.clear(); handled = false;
    {
        nx::ExceptionFrame frame(100, 10, 42, false); frame.before_call(c, 100);
        c.x[0] = pointer(header); invoke("_Unwind_RaiseException", c); assert(c.x[0] == 5);
    }
    assert((phases == std::vector<int>{1}));
    c.x[0] = 0; invoke("_Unwind_GetIP", c); assert(c.x[0] == 123);
    std::puts("Excepciones: búsqueda previa, limpieza, captura, registros y contexto ajeno: PASS");
}
