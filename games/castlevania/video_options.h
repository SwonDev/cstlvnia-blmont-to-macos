#pragma once
#include <cstdint>
namespace nx::game {
void video_native_size(uint32_t& w, uint32_t& h);
void video_render_size(uint32_t& w, uint32_t& h);
bool select_render_size(uint32_t w, uint32_t h);
}
