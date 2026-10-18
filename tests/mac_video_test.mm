#import <AppKit/AppKit.h>
#include "mac_video.h"
#undef NDEBUG
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <unistd.h>
#include <tuple>

namespace nx { void log(const char*, ...) {} namespace plat { void report_video_window_mode(int) {} } }
int main() {
    @autoreleasepool {
        assert(SDL_Init(SDL_INIT_VIDEO));
        SDL_Window* window = SDL_CreateWindow("Prueba de vídeo", 640, 360,
            SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
        assert(window);
        const auto* desktop = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
        assert(desktop);
        std::printf("Pantalla: %d×%d, densidad %.1f\n", desktop->w, desktop->h, desktop->pixel_density);
        char temporary[] = "/tmp/castlevania-video-XXXXXX";
        const char* directory = mkdtemp(temporary); assert(directory);
        nx::plat::install_mac_video_menu(window, directory);
        NSMenu* video = [NSApp.mainMenu itemWithTitle:@"Vídeo"].submenu;
        assert(video && video.numberOfItems == 6);
        for (auto [mode, width, height] : {std::tuple{"1080", 1920, 1080}, std::tuple{"1440", 2560, 1440}}) {
            assert(nx::plat::request_mac_video_mode(window, mode));
            assert(SDL_SyncWindow(window));
            int w = 0, h = 0; assert(SDL_GetWindowSizeInPixels(window, &w, &h));
            std::printf("Vídeo: %s → %d×%d píxeles, densidad %.1f\n", mode, w, h, SDL_GetWindowPixelDensity(window));
            assert(w == width && h == height);
        }
        assert(!nx::plat::request_mac_video_mode(window, "inválido"));
        std::string saved; std::ifstream(std::filesystem::path(directory) / "video-window.txt") >> saved;
        assert(saved == "1440");
        SDL_Event event{}; event.type = SDL_EVENT_USER; nx::plat::mac_video_event(event);
        SDL_DestroyWindow(window); SDL_Quit();
        std::filesystem::remove_all(directory);
        std::puts("Menú nativo, píxeles Retina y persistencia delimitada: PASS");
    }
}
