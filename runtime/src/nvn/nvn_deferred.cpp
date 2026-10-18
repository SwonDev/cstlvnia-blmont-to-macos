#include "nvn.h"

namespace nx::nvn {

// Copia las tablas diferidas al enviar las listas, incluidas las listas anidadas.
DeferredValues deferred_snapshot(const std::vector<std::shared_ptr<CmdList>>& lists) {
    DeferredValues v;
    std::vector<std::pair<std::shared_ptr<CmdList>, int>> todo;
    for (auto& l : lists) todo.push_back({l, 0});
    while (!todo.empty()) {
        auto [l, depth] = todo.back();
        todo.pop_back();
        if (!l) continue;
        for (const Cmd& c : l->cmds) {
            if (c.op == Cmd::CallList && depth < 8) {
                todo.push_back({command_list(c.a[0]), depth + 1});
            } else if (c.op == Cmd::BindDeferred && c.n <= (c.mask == 4 ? 128 : 64)) {
                uint32_t words = c.mask == 2 || c.mask == 3 ? 2 : 1;  // rangos {dirección, tamaño} o handles
                auto& values = v[c.a[0]];
                const size_t before = values.size(), needed = size_t(c.n) * words;
                if (needed > before) {
                    values.resize(needed);
                    for (size_t k = before; k < needed; ++k) values[k] = ld<uint64_t>(c.a[0] + 8 * k);
                }
            }
        }
    }
    return v;
}

}  // namespace nx::nvn
