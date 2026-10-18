#pragma once
#include "nvn_shaders.h"
#include <array>

namespace nx::nvn {
const std::vector<const ShaderInfo*>* program_shaders(uint64_t program);
struct StagePrograms {
    std::array<uint64_t, 6> slots{};
    void bind(uint64_t program, uint32_t mask) {
        for (unsigned stage = 0; stage < slots.size(); stage++)
            if (mask & (1u << stage)) slots[stage] = program;
    }
    uint64_t operator[](size_t stage) const { return slots[stage]; }
};
inline const std::vector<const ShaderInfo*>* graphics_program_shaders(const StagePrograms& programs) {
    thread_local std::vector<const ShaderInfo*> result;
    result.clear();
    constexpr ShaderStage stages[] = {ShaderStage::Vertex, ShaderStage::Fragment};
    for (unsigned stage = 0; stage < 2; stage++) {
        const auto* shaders = programs[stage] ? program_shaders(programs[stage]) : nullptr;
        if (shaders) for (const auto* shader : *shaders)
            if (shader->stage == stages[stage]) { result.push_back(shader); break; }
    }
    return &result;
}
}
