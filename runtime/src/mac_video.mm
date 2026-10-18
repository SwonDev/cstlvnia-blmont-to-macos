#import <AppKit/AppKit.h>
#include "mac_video.h"
#include "platform.h"
#include <nx/runtime.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <utility>

namespace {
SDL_Window* video_window = nullptr;
std::filesystem::path settings;
std::string pending;

void save_mode(const std::string& mode) {
    if (settings.empty()) return;
    const auto temporary = settings.string() + ".tmp";
    std::ofstream output(temporary);
    if (!(output << mode << '\n')) { nx::log("video: no se puede guardar el modo de ventana"); return; }
    output.close();
    std::error_code error;
    std::filesystem::rename(temporary, settings, error);
    if (error) nx::log("video: no se puede actualizar el modo de ventana: %s", error.message().c_str());
}

bool apply_windowed(SDL_Window* window, const std::string& mode) {
    if (mode == "maximized") return SDL_MaximizeWindow(window);
    if (mode == "window") return SDL_RestoreWindow(window);
    const int width = mode == "1080" ? 1920 : 2560;
    const int height = mode == "1080" ? 1080 : 1440;
    float density = SDL_GetWindowPixelDensity(window);
    if (!(density > 0)) density = 1;
    return SDL_RestoreWindow(window) && SDL_SetWindowSize(window, int(std::lround(width / density)),
                                                       int(std::lround(height / density)));
}
}

@interface NXVideoMenu : NSObject <NSMenuDelegate>
@property(nonatomic, strong) NSMenuItem* fullscreenItem;
@property(nonatomic, strong) NSMenuItem* outputItem;
@end

@implementation NXVideoMenu
- (void)selectMode:(NSMenuItem*)sender {
    const char* modes[] = {"native", "1080", "1440", "maximized"};
    if (sender.tag >= 0 && sender.tag < 4)
        nx::plat::request_mac_video_mode(video_window, modes[sender.tag]);
}
- (void)menuNeedsUpdate:(NSMenu*)menu {
    (void)menu;
    int width = 0, height = 0;
    SDL_GetWindowSizeInPixels(video_window, &width, &height);
    self.outputItem.title = [NSString stringWithFormat:@"Salida actual: %d × %d píxeles", width, height];
    self.fullscreenItem.state = (SDL_GetWindowFlags(video_window) & SDL_WINDOW_FULLSCREEN) ? NSControlStateValueOn : NSControlStateValueOff;
    if (const auto* mode = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(video_window)))
        self.fullscreenItem.title = [NSString stringWithFormat:@"Pantalla completa nativa (%d × %d)",
            int(std::lround(mode->w * mode->pixel_density)), int(std::lround(mode->h * mode->pixel_density))];
}
@end

namespace nx::plat {
namespace { NXVideoMenu* controller; }

bool request_mac_video_mode(SDL_Window* window, const std::string& mode) {
    if (!window || (mode != "native" && mode != "1080" && mode != "1440" && mode != "maximized" && mode != "window")) return false;
    bool ok;
    if (mode == "native") {
        pending.clear();
        ok = SDL_SetWindowFullscreenMode(window, nullptr) && SDL_SetWindowFullscreen(window, true);
    } else if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
        pending = mode;
        ok = SDL_SetWindowFullscreen(window, false);
        if (!ok) pending.clear();
    } else {
        ok = apply_windowed(window, mode);
    }
    if (ok) { report_video_window_mode(mode=="native" ? 1 : mode=="maximized" ? 2 : 3); save_mode(mode); }
    else nx::log("video: no se pudo aplicar %s: %s", mode.c_str(), SDL_GetError());
    return ok;
}

void toggle_mac_fullscreen(SDL_Window* window) {
    request_mac_video_mode(window, SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN ? "window" : "native");
}

void mac_video_event(const SDL_Event& event) {
    if (event.type != SDL_EVENT_WINDOW_LEAVE_FULLSCREEN && event.type != SDL_EVENT_WINDOW_ENTER_FULLSCREEN) return;
    if (!video_window || event.window.windowID != SDL_GetWindowID(video_window)) return;
    report_video_window_mode(event.type==SDL_EVENT_WINDOW_ENTER_FULLSCREEN ? 1 : 3);
    if (event.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN) {
        const auto mode = pending.empty() ? std::string("window") : std::exchange(pending, {});
        if (apply_windowed(video_window, mode)) { report_video_window_mode(mode=="maximized" ? 2 : 3); save_mode(mode); }
    } else if (event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN) save_mode("native");
}

void install_mac_video_menu(SDL_Window* window, const std::string& user_root) {
    video_window = window;
    settings = std::filesystem::path(user_root) / "video-window.txt";
    controller = [NXVideoMenu new];
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Vídeo"];
    menu.delegate = controller;
    NSArray<NSString*>* titles = @[@"Pantalla completa nativa", @"Ventana de 1920 × 1080 píxeles",
                                   @"Ventana de 2560 × 1440 píxeles", @"Maximizar ventana"];
    for (NSInteger index = 0; index < titles.count; ++index) {
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:titles[index] action:@selector(selectMode:) keyEquivalent:@""];
        item.target = controller; item.tag = index;
        [menu addItem:item];
        if (index == 0) controller.fullscreenItem = item;
    }
    [menu addItem:[NSMenuItem separatorItem]];
    controller.outputItem = [[NSMenuItem alloc] initWithTitle:@"Salida Retina" action:nil keyEquivalent:@""];
    [menu addItem:controller.outputItem];
    NSMenuItem* root = [[NSMenuItem alloc] initWithTitle:@"Vídeo" action:nil keyEquivalent:@""];
    root.submenu = menu;
    if (!NSApp.mainMenu) NSApp.mainMenu = [NSMenu new];
    [NSApp.mainMenu addItem:root];
    std::ifstream input(settings);
    std::string mode;
    if (std::getline(input, mode) && mode.size() <= 16) request_mac_video_mode(window, mode);
}
}
