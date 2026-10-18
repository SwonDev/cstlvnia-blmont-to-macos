#include <nx/game.h>
#if defined(__APPLE__)
#include "movie.h"
#include "resolution.h"
#endif

namespace nx {

const game::Info game::kInfo = {
    "castlevania", "Castlevania: Belmont's Curse", 0x0100BA4026E94000ull,
    "dev.swondev.castlevania", "../trabajo/runtime-data", 0x1000000, true, true, true, true,
};

// El perfil conserva la configuración y el contenido originales del juego.
void game::after_init() {
#if defined(__APPLE__)
    install_movie_policy();
    install_resolution_policy();
#endif
}
void game::renderer_start() {}
void game::first_frame() {}

}
