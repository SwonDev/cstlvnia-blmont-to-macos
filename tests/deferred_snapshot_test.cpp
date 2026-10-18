#include "nvn/nvn.h"

#include <array>
#include <cstdio>

namespace {
auto child = std::make_shared<nx::nvn::CmdList>();
}

namespace nx::nvn {
std::shared_ptr<CmdList> command_list(uint64_t handle) { return handle == 42 ? child : nullptr; }
}

int main() {
    using namespace nx::nvn;
    std::array<uint64_t, 128> handles;
    for (size_t n = 0; n < handles.size(); ++n) handles[n] = n + 100;
    auto root = std::make_shared<CmdList>();
    Cmd small{Cmd::BindDeferred};
    small.mask = 4, small.n = 1, small.a[0] = reinterpret_cast<uint64_t>(handles.data());
    root->cmds.push_back(small);
    Cmd large = small;
    large.n = 128;
    child->cmds.push_back(large);
    Cmd call{Cmd::CallList};
    call.a[0] = 42;
    root->cmds.push_back(call);
    auto snapshot = deferred_snapshot({root});
    if (snapshot.at(small.a[0]).size() != 128) return 1;
    handles[127] = 999;
    if (snapshot.at(small.a[0])[127] != 227) return 2;
    root->cmds.clear();
    small.mask = 2, small.n = 2;
    root->cmds.push_back(small);
    snapshot = deferred_snapshot({root});
    if (snapshot.at(small.a[0]).size() != 4 || snapshot.at(small.a[0])[3] != 103) return 3;
    std::puts("Snapshot diferido: arrays compartidos, listas anidadas y tamaño de rangos: PASS");
}
