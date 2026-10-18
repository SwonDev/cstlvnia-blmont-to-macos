#pragma once
#include <string>
#include <SDL3/SDL.h>

namespace nx::plat {
void install_mac_video_menu(SDL_Window* window, const std::string& user_root);
bool request_mac_video_mode(SDL_Window* window, const std::string& mode);
void mac_video_event(const SDL_Event& event);
void toggle_mac_fullscreen(SDL_Window* window);
}
