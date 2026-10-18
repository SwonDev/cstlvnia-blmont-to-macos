#include "nvn_program_bindings.h"
#include <cstdio>
namespace {
nx::nvn::ShaderInfo vertex, fragment, second_fragment, compute;
std::vector<const nx::nvn::ShaderInfo*> main_program{&vertex, &fragment};
std::vector<const nx::nvn::ShaderInfo*> other_program{&vertex, &second_fragment};
std::vector<const nx::nvn::ShaderInfo*> compute_program{&compute};
}
namespace nx::nvn {
const std::vector<const ShaderInfo*>* program_shaders(uint64_t id) {
    return id == 100 ? &main_program : id == 200 ? &other_program : id == 300 ? &compute_program : nullptr;
}
}
int main() {
    using namespace nx::nvn;
    vertex.stage = ShaderStage::Vertex; fragment.stage = second_fragment.stage = ShaderStage::Fragment;
    compute.stage = ShaderStage::Compute;
    StagePrograms bound;
    bound.bind(100, 3); bound.bind(0, 0x3C);  // secuencia real de Unity: conserva VS y FS
    auto* draw = graphics_program_shaders(bound);
    if (draw->size() != 2 || (*draw)[0] != &vertex || (*draw)[1] != &fragment || bound[5]) return 1;
    bound.bind(300, 32); draw = graphics_program_shaders(bound);
    if (draw->size() != 2 || bound[5] != 300) return 2;
    bound.bind(200, 2); draw = graphics_program_shaders(bound);
    if (draw->size() != 2 || (*draw)[0] != &vertex || (*draw)[1] != &second_fragment) return 3;
    bound.bind(0, 1); draw = graphics_program_shaders(bound);
    if (draw->size() != 1 || (*draw)[0] != &second_fragment || bound[5] != 300) return 4;
    bound.bind(100, 0); if (bound[0] || bound[1] != 200) return 5;
    bound.bind(0, 32); if (bound[1] != 200 || bound[5]) return 6;
    bound.bind(999, 3); if (!graphics_program_shaders(bound)->empty()) return 7;
    std::puts("Programas NVN: máscara parcial, desactivar otras etapas, VS/FS separados y cómputo independiente: PASS");
}
