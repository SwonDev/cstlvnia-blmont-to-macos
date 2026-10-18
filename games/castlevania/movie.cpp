#if defined(__APPLE__)
#include <nx/runtime.h>
#include <string_view>
#include <atomic>
#include <array>
#include "movie.h"
#include "yuv.h"

namespace nx {
namespace {
constexpr auto avc_component = "OMX.google.h264.decoder";
std::atomic<bool> software_avc = false;

void append_avc_component(CPU& cpu, uint64_t vector) {
    const auto ctor = find_symbol("_ZN7android7String8C1EPKc");
    const auto dtor = find_symbol("_ZN7android7String8D1Ev");
    const auto add = find_symbol("_ZN7android10VectorImpl3addEPKv");
    if (!ctor || !dtor || !add) fatal("Faltan las operaciones originales del selector multimedia");
    // String8 contiene un puntero de 64 bits en este SDK. Su constructor y la
    // copia virtual de VectorImpl conservan la propiedad original del búfer.
    uint64_t string8 = 0;
    CPU helper = cpu;
    helper.x[0] = reinterpret_cast<uint64_t>(&string8);
    helper.x[1] = reinterpret_cast<uint64_t>(avc_component);
    call(helper, ctor);
    try {
        helper.x[0] = vector;
        helper.x[1] = reinterpret_cast<uint64_t>(&string8);
        call(helper, add);
        if (int64_t(helper.x[0]) < 0) fatal("No se puede añadir el decodificador AVC por software");
    } catch (...) {
        helper.x[0] = reinterpret_cast<uint64_t>(&string8);
        call(helper, dtor);
        throw;
    }
    helper.x[0] = reinterpret_cast<uint64_t>(&string8);
    call(helper, dtor);
}

constexpr auto output_converter = "_ZN5movie11DecoderImpl26ConvertDecoderOutputFormatEPhiiiiS1_PiNS_19DecoderOutputFormatEm";
void software_video_output(CPU& cpu) {
    const auto format = ld<uint32_t>(cpu.x[31]);
    const auto capacity = ld<uint64_t>(cpu.x[31] + 8);
    // Perfil comprobado contra la salida SoftAVC de esta revisión: I420 1920x1088,
    // recorte visible 1920x1080. Unity anuncia nv12-uv-offset=1920*1080:
    // leer el croma tras las 1088 líneas de origen y empaquetarlo tras las 1080 de destino.
    if (software_avc.load(std::memory_order_relaxed) && format == 4 &&
        cpu.x[2] == 1920 && cpu.x[3] == 1080 && cpu.x[4] == 1920 && cpu.x[5] == 1080 &&
        capacity == 3133440 && cpu.x[1] && cpu.x[6] && cpu.x[7]) {
        if (!game::i420_to_nv12({reinterpret_cast<const uint8_t*>(cpu.x[1]), capacity},
                               {reinterpret_cast<uint8_t*>(cpu.x[6]), capacity}, 1920, 1080, 1920, 1088))
            fatal("No se puede adaptar el fotograma I420 del decodificador original");
        st<uint32_t>(cpu.x[7], 1920u * 1080u * 3u / 2u);
        cpu.x[0] = 0;
        return;
    }
    const auto original = find_symbol(output_converter);
    if (!original) fatal("No se encuentra el conversor multimedia original");
    call(cpu, original);
}

void software_video_codecs(CPU& cpu) {
    const char* mime = reinterpret_cast<const char*>(cpu.x[0]);
    const bool video = mime && std::string_view(mime).starts_with("video/") && !(cpu.x[1] & 1);
    const auto vector = cpu.x[4];
    if (video) {
        // Flags de OMXCodec, contrastados con su implementación original: software=8, hardware=16.
        cpu.x[3] = (uint32_t(cpu.x[3]) & ~16u) | 8u;
        const char* component = reinterpret_cast<const char*>(cpu.x[2]);
        if (component && std::string_view(component).starts_with("OMX.Nvidia.")) cpu.x[2] = 0;
    }
    const char* component = reinterpret_cast<const char*>(cpu.x[2]);
    const bool fallback = video && std::string_view(mime) == "video/avc" &&
        (!component || std::string_view(component) == avc_component);
    const auto target = find_symbol(hle_current_name());
    if (!target) fatal("No se encuentra el selector original de códecs");
    call(cpu, target);
    // La tabla de selección de Switch solo ofrece AVC NVIDIA, aunque su plugin
    // SoftOMX registra el decodificador Google. Usamos ese componente existente.
    if (fallback && vector && ld<uint64_t>(vector + 16) == 0) {
        append_avc_component(cpu, vector);
        software_avc.store(true, std::memory_order_relaxed);
        log("movie: seleccionado el decodificador H.264 original por software");
    }
}
}
void game::movie_readiness(CPU& cpu) {
    const auto vtable = ld<uint64_t>(cpu.x[0]);
    call(cpu, ld<uint64_t>(vtable + 0x160));
    cpu.x[0] = cpu.x[0] >= (software_avc.load(std::memory_order_relaxed) ? 1u : 3u);
}

void game::install_movie_policy() {
    // Unity espera tres imágenes, pero SoftAVC sólo presta dos búferes y espera
    // su devolución. El umbral de esta ruta debe permitir arrancar el consumo.
    constexpr uint64_t ready = 0x126d77c;
    constexpr std::array<uint32_t, 9> expected = {
        0xa9bf7bfdu, 0x910003fdu, 0xf9400008u, 0xf940b108u, 0xd63f0100u,
        0xf100081fu, 0x1a9f97e0u, 0xa8c17bfdu, 0xd65f03c0u,
    };
    const auto* module = module_at(NX_BASE);
    if (!module || std::string_view(module->info->name) != "main" ||
        module->info->text_size < ready + sizeof(expected) || !module->table[ready / 4])
        fatal("No se encuentra la comprobación multimedia de Castlevania 1.1.0");
    for (size_t k = 0; k < expected.size(); k++)
        if (ld<uint32_t>(module->base + ready + k * 4) != expected[k])
            fatal("La revisión de Castlevania no coincide con la política multimedia verificada");
    module->table[ready / 4] = movie_readiness;
}
NX_HLE(output_converter, software_video_output);
NX_HLE("_ZN7android8OMXCodec18findMatchingCodecsEPKcbS2_jPNS_6VectorINS_7String8EEE", software_video_codecs);
}
#endif
