cmake_minimum_required(VERSION 3.25)
project(CastlevaniaMac C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The game this build is for: games/<NX_GAME>/ holds what only that game needs (game.cpp: nx/game.h), the shared
# runtime is the rest. One build directory per game.
set(NX_GAME "castlevania" CACHE STRING "Game to build (a folder under games/)")
option(NX_BUILD_GAME "Compilar el juego; OFF permite ejecutar contratos sin datos comerciales" ON)
set(NX_GAME_DIR "${CMAKE_SOURCE_DIR}/games/${NX_GAME}")
if(NOT EXISTS "${NX_GAME_DIR}/game.cpp")
    message(FATAL_ERROR "games/${NX_GAME}/game.cpp not found")
endif()
set(NX_MODULES main sdk)
include("${NX_GAME_DIR}/modules.cmake" OPTIONAL)

# Generated code of the game (python -m recomp <nso> <gen dir>/<module> --name <module> --base ...). Never committed.
# Hades keeps the layout from before there were other games (private/gen); the others use private/<game>/gen.
if(NX_GAME STREQUAL "hades")
    set(NX_GEN_DEFAULT "${CMAKE_SOURCE_DIR}/private/gen")
else()
    set(NX_GEN_DEFAULT "${CMAKE_SOURCE_DIR}/private/${NX_GAME}/gen")
endif()
set(NX_GEN_DIR "${NX_GEN_DEFAULT}" CACHE PATH "Recompiled module sources")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-unused-label -Wno-unused-value -Wno-parentheses-equality -Wno-unused-variable
                        -Wno-unused-but-set-variable -Wno-constant-logical-operand -Wno-unused-function)
endif()
if(MSVC)
    add_compile_options(/arch:AVX2)
endif()
if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Unit tests of runtime pieces that build without the game (docs/nvn/WINDOWS-PLAN.md, "Test plan"): ctest.
option(NX_TESTS "Build the standalone unit tests" OFF)
if(NX_TESTS AND NOT ANDROID)
    enable_testing()
    add_executable(gpu_pick_test tests/gpu_pick_test.cpp runtime/src/nvn/nvn_gpu_pick.cpp)
    target_compile_definitions(gpu_pick_test PRIVATE NX_GPU_PICK_STANDALONE)
    target_include_directories(gpu_pick_test PRIVATE runtime/src/nvn)
    add_test(NAME gpu_pick COMMAND gpu_pick_test)
    add_executable(program_bindings_test tests/program_bindings_test.cpp)
    target_include_directories(program_bindings_test PRIVATE runtime/src/nvn)
    add_test(NAME program_bindings COMMAND program_bindings_test)
    add_executable(quads_test tests/quads_test.cpp)
    target_include_directories(quads_test PRIVATE runtime/src/nvn)
    add_test(NAME quads COMMAND quads_test)
    add_executable(tiling_test tests/tiling_test.cpp runtime/src/nvn/nvn_tiling.cpp)
    target_include_directories(tiling_test PRIVATE runtime/src/nvn)
    add_test(NAME tiling COMMAND tiling_test --selftest)
    add_executable(astc_test tests/astc_test.cpp runtime/src/nvn/nvn_astc.cpp)
    target_include_directories(astc_test PRIVATE runtime/src/nvn)
    add_test(NAME astc COMMAND astc_test --selftest)
    add_executable(texture_handles_test tests/texture_handles_test.cpp)
    target_include_directories(texture_handles_test PRIVATE runtime/src/nvn)
    add_test(NAME texture_handles COMMAND texture_handles_test)
    add_executable(weak_import_test tests/weak_import_test.cpp runtime/src/loader.cpp)
    target_include_directories(weak_import_test PRIVATE runtime/include runtime/src)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/weak_fixture")
    add_test(NAME weak_import COMMAND weak_import_test "${CMAKE_CURRENT_BINARY_DIR}/weak_fixture")
    add_executable(svc_info_test tests/svc_info_test.cpp runtime/src/svc_info.cpp)
    target_include_directories(svc_info_test PRIVATE runtime/include runtime/src)
    add_test(NAME svc_info COMMAND svc_info_test)
    add_executable(time_service_test tests/time_service_test.cpp runtime/src/svc_time.cpp)
    target_include_directories(time_service_test PRIVATE runtime/include runtime/src)
    add_test(NAME time_service COMMAND time_service_test)
    add_executable(audio_out_test tests/audio_out_test.cpp runtime/src/hle_audio.cpp)
    target_include_directories(audio_out_test PRIVATE runtime/include runtime/src)
    add_test(NAME audio_out COMMAND audio_out_test)
    add_executable(lm_packet_test tests/lm_packet_test.cpp)
    target_include_directories(lm_packet_test PRIVATE runtime/src)
    add_test(NAME lm_packet COMMAND lm_packet_test)
    add_executable(touch_desktop_test tests/touch_desktop_test.cpp runtime/src/hle_hid.cpp)
    target_include_directories(touch_desktop_test PRIVATE runtime/include runtime/src)
    add_test(NAME touch_desktop COMMAND touch_desktop_test)
    add_executable(thread_control_test tests/thread_control_test.cpp runtime/src/kernel_thread_control.cpp)
    target_include_directories(thread_control_test PRIVATE runtime/include runtime/src)
    add_test(NAME thread_control COMMAND thread_control_test)
    add_executable(exception_frame_test tests/exception_frame_test.cpp runtime/src/guest_exception.cpp)
    target_include_directories(exception_frame_test PRIVATE runtime/include runtime/src)
    add_test(NAME exception_frame COMMAND exception_frame_test)
    if(APPLE)
        enable_language(OBJCXX)
        set(CMAKE_OBJCXX_STANDARD 20)
        find_package(SDL3 REQUIRED CONFIG)
        find_library(NX_APPKIT AppKit REQUIRED)
        add_executable(mac_video_test tests/mac_video_test.mm runtime/src/mac_video.mm)
        set_source_files_properties(runtime/src/mac_video.mm tests/mac_video_test.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
        target_include_directories(mac_video_test PRIVATE runtime/include runtime/src)
        target_link_libraries(mac_video_test PRIVATE SDL3::SDL3 ${NX_APPKIT})
        add_test(NAME mac_video COMMAND mac_video_test)
        add_executable(video_options_test tests/video_options_test.cpp games/castlevania/video_options.cpp)
        target_include_directories(video_options_test PRIVATE runtime/include runtime/src)
        add_test(NAME video_options COMMAND video_options_test)
        add_executable(loading_animation_test tests/loading_animation_test.cpp games/castlevania/loading_animation.cpp)
        target_include_directories(loading_animation_test PRIVATE runtime/include runtime/src)
        add_test(NAME loading_animation_start COMMAND loading_animation_test)
        add_executable(resolution_policy_test tests/resolution_policy_test.cpp games/castlevania/resolution.cpp)
        target_include_directories(resolution_policy_test PRIVATE runtime/include runtime/src)
        add_test(NAME resolution_policy COMMAND resolution_policy_test)
        add_executable(movie_policy_test tests/movie_policy_test.cpp games/castlevania/movie.cpp)
        target_include_directories(movie_policy_test PRIVATE runtime/include)
        add_test(NAME movie_policy COMMAND movie_policy_test)
        add_executable(fs_mount_test tests/fs_mount_test.cpp runtime/src/hle_fs.cpp)
        target_include_directories(fs_mount_test PRIVATE runtime/include runtime/src)
        add_test(NAME fs_mount COMMAND fs_mount_test)
    endif()
    set_tests_properties(weak_import PROPERTIES ENVIRONMENT "NX_LOG=${CMAKE_CURRENT_BINARY_DIR}/weak_fixture/fixture.log")
    add_executable(shader_cache_test tests/shader_cache_test.cpp runtime/src/nvn/nvn_shaders.cpp
                   runtime/src/nvn/nvn_spirv_reflect.cpp)
    target_compile_definitions(shader_cache_test PRIVATE NX_SHADERS_STANDALONE)
    target_include_directories(shader_cache_test PRIVATE runtime/src/nvn)
    if(EXISTS "${CMAKE_SOURCE_DIR}/private/wonder/spirv")  # the game's own shaders: never in the repository
        add_test(NAME shader_cache COMMAND shader_cache_test "${CMAKE_SOURCE_DIR}/private/wonder")
    endif()
    if(APPLE)
        add_executable(mac_memory_test tests/mac_memory_test.cpp runtime/src/mac_memory.cpp)
        target_include_directories(mac_memory_test PRIVATE runtime/src)
        add_test(NAME mac_memory COMMAND mac_memory_test)
        add_executable(switch2bridge_test tests/switch2bridge_test.cpp runtime/src/switch2bridge.cpp)
        target_include_directories(switch2bridge_test PRIVATE runtime/src)
        add_test(NAME switch2bridge COMMAND switch2bridge_test)
        find_path(NX_TEST_VULKAN_INCLUDE vulkan/vulkan.h HINTS /opt/homebrew/include /usr/local/include)
        add_executable(separate_bindings_test tests/separate_bindings_test.cpp runtime/src/nvn/nvn_separate.cpp)
        target_include_directories(separate_bindings_test PRIVATE runtime/include runtime/src "${NX_TEST_VULKAN_INCLUDE}")
        add_test(NAME separate_bindings COMMAND separate_bindings_test)
        add_executable(deferred_binding_policy_test tests/deferred_binding_policy_test.cpp runtime/src/nvn/nvn_binds.cpp runtime/src/nvn/nvn_deferred.cpp)
        target_include_directories(deferred_binding_policy_test PRIVATE runtime/include runtime/src "${NX_TEST_VULKAN_INCLUDE}")
        add_test(NAME deferred_default COMMAND deferred_binding_policy_test)
        add_test(NAME deferred_override COMMAND deferred_binding_policy_test --desactivar)
        add_executable(texel_view_test tests/texel_view_test.cpp runtime/src/nvn/nvn_texel_writes.cpp)
        target_include_directories(texel_view_test PRIVATE runtime/include runtime/src "${NX_TEST_VULKAN_INCLUDE}")
        add_test(NAME texel_view COMMAND texel_view_test)
        add_executable(texel_upload_test tests/texel_upload_test.cpp runtime/src/nvn/nvn_texel_uploads.cpp)
        target_include_directories(texel_upload_test PRIVATE runtime/include runtime/src "${NX_TEST_VULKAN_INCLUDE}")
        add_test(NAME texel_upload_order COMMAND texel_upload_test)
        add_executable(depth_mode_test tests/depth_mode_test.cpp runtime/src/nvn/nvn_depth_mode.cpp)
        target_include_directories(depth_mode_test PRIVATE runtime/include runtime/src "${NX_TEST_VULKAN_INCLUDE}")
        add_test(NAME depth_mode COMMAND depth_mode_test)
        add_executable(deferred_snapshot_test tests/deferred_snapshot_test.cpp runtime/src/nvn/nvn_deferred.cpp)
        target_include_directories(deferred_snapshot_test PRIVATE runtime/include runtime/src "${NX_TEST_VULKAN_INCLUDE}")
        add_test(NAME deferred_snapshot COMMAND deferred_snapshot_test)
        add_executable(format_layout_test tests/format_layout_test.cpp runtime/src/nvn/nvn_texture_layout.cpp)
        target_include_directories(format_layout_test PRIVATE runtime/include runtime/src "${NX_TEST_VULKAN_INCLUDE}")
        add_test(NAME format_layout COMMAND format_layout_test)
        add_executable(command_memory_test tests/command_memory_test.cpp runtime/src/nvn/nvn_cmd.cpp)
        target_include_directories(command_memory_test PRIVATE runtime/include runtime/src "${NX_TEST_VULKAN_INCLUDE}")
        add_test(NAME command_memory COMMAND command_memory_test)
        add_executable(buffer_copy_test tests/buffer_copy_test.cpp runtime/src/nvn/nvn_buffer_copy.cpp runtime/src/nvn/nvn_buffer_texture.cpp)
        target_include_directories(buffer_copy_test PRIVATE runtime/include runtime/src "${NX_TEST_VULKAN_INCLUDE}")
        add_test(NAME buffer_copy COMMAND buffer_copy_test)
        add_executable(texture_identity_test tests/texture_identity_test.cpp runtime/src/nvn/nvn_incarnation.cpp)
        target_include_directories(texture_identity_test PRIVATE runtime/include runtime/src "${NX_TEST_VULKAN_INCLUDE}")
        add_test(NAME texture_identity COMMAND texture_identity_test)
    endif()
    if(WIN32)
        add_executable(memguard_test tests/memguard_test.cpp)
        target_link_libraries(memguard_test PRIVATE psapi)
        add_test(NAME memguard COMMAND memguard_test)
    endif()
endif()

# Without the generated code (a checkout with no game data) only the tests above are built.
if(NOT NX_BUILD_GAME)
    return()
endif()
file(GLOB NX_GEN_PROBE "${NX_GEN_DIR}/main/*.cpp")
if(NOT NX_GEN_PROBE)
    message(WARNING "No generated code in ${NX_GEN_DIR}/main: building the unit tests only")
    return()
endif()

set(NX_COMPILED_MODULES)
set(NX_LINKED_MODULES "${CMAKE_CURRENT_BINARY_DIR}/linked_modules.cpp")
file(WRITE "${NX_LINKED_MODULES}" "#include <nx/module.h>\n")
foreach(mod IN LISTS NX_MODULES)
    if(NOT mod MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR "Nombre de módulo no válido: ${mod}")
    endif()
    file(APPEND "${NX_LINKED_MODULES}" "extern const nx::ModuleInfo nx_module_${mod};\n")
endforeach()
file(APPEND "${NX_LINKED_MODULES}" "namespace nx { std::span<const ModuleInfo* const> linked_modules() { static const ModuleInfo* const modules[] = {")
foreach(mod IN LISTS NX_MODULES)
    file(APPEND "${NX_LINKED_MODULES}" "&nx_module_${mod},")
endforeach()
file(APPEND "${NX_LINKED_MODULES}" "}; return modules; } }\n")
foreach(mod IN LISTS NX_MODULES)
    file(GLOB src CONFIGURE_DEPENDS "${NX_GEN_DIR}/${mod}/*.cpp")
    add_library(game_${mod} STATIC ${src})
    list(APPEND NX_COMPILED_MODULES game_${mod})
    target_include_directories(game_${mod} PRIVATE runtime/include "${NX_GEN_DIR}/${mod}")
    if(ANDROID)
        # Hidden visibility: calls between recompiled functions are direct, not through the PLT.
        # armv8.2-a+crypto: cpu.h's AES/SHA paths need __ARM_FEATURE_AES/SHA2 (the NDK default armv8-a has neither).
        target_compile_options(game_${mod} PRIVATE -O2 -fPIC -fvisibility=hidden -fno-semantic-interposition
                               -march=armv8.2-a+crypto)
    elseif(APPLE)
        target_compile_options(game_${mod} PRIVATE -O2 -fvisibility=hidden)
    elseif(UNIX)
        # Linux x86-64: AVX2/FMA/BMI as the Windows build (/arch:AVX2); no FMA contraction, so a guest fmul + fadd
        # rounds twice, as on the Switch.
        target_compile_options(game_${mod} PRIVATE -O2 -fvisibility=hidden -march=x86-64-v3 -ffp-contract=off)
    endif()
endforeach()

set(NX_RUNTIME_SOURCES
    runtime/src/guest_exception.cpp
    "${NX_LINKED_MODULES}"
    runtime/src/main.cpp
    runtime/src/sampler.cpp
    runtime/src/loader.cpp
    runtime/src/memory.cpp
    runtime/src/kernel.cpp
    runtime/src/kernel_thread_control.cpp
    runtime/src/kernel_watch.cpp
    runtime/src/ipc.cpp
    runtime/src/svc_am.cpp
    runtime/src/svc_set.cpp
    runtime/src/svc_misc.cpp
    runtime/src/svc_time.cpp
    runtime/src/svc_process.cpp
    runtime/src/svc_info.cpp
    runtime/src/hle_fs.cpp
    runtime/src/svc_account.cpp
    runtime/src/hle_ro.cpp
    runtime/src/hle_vi.cpp
    runtime/src/hle_hid.cpp
    runtime/src/hle_net.cpp
    runtime/src/hle_audio.cpp
    runtime/src/hle_pl.cpp
    runtime/src/input_script.cpp
    runtime/src/touch_layout.cpp
    runtime/src/perf_hint.cpp
    runtime/src/frame_stats.cpp
    runtime/src/write_watch.cpp
    runtime/src/nvn/nvn_caps.cpp
    runtime/src/nvn/nvn_renderpass.cpp
    runtime/src/nvn/nvn_gpu_busy.cpp
    runtime/src/nvn/nvn_janitor.cpp
    runtime/src/nvn/nvn_trace.cpp
    runtime/src/nvn/nvn_device.cpp
    runtime/src/nvn/nvn_objects.cpp
    runtime/src/nvn/nvn_incarnation.cpp
    runtime/src/nvn/nvn_texture_layout.cpp
    runtime/src/nvn/nvn_program.cpp
    runtime/src/nvn/nvn_queue.cpp
    runtime/src/nvn/nvn_cmd.cpp
    runtime/src/nvn/nvn_state.cpp
    runtime/src/nvn/nvn_format.cpp
    runtime/src/nvn/nvn_enums.cpp
    runtime/src/nvn/nvn_sampler.cpp
    runtime/src/nvn/nvn_sampler_objects.cpp
    runtime/src/nvn/nvn_draw_state.cpp
    runtime/src/nvn/nvn_raster.cpp
    runtime/src/nvn/nvn_worker.cpp
    runtime/src/nvn/nvn_shaders.cpp
    runtime/src/nvn/nvn_spirv_reflect.cpp
    runtime/src/nvn/nvn_tiling.cpp
    runtime/src/nvn/nvn_vk.cpp
    runtime/src/nvn/nvn_gpu_pick.cpp
    runtime/src/nvn/nvn_image.cpp
    runtime/src/nvn/nvn_frames.cpp
    runtime/src/nvn/nvn_dedup.cpp
    runtime/src/nvn/nvn_written.cpp
    runtime/src/nvn/nvn_replay.cpp
    runtime/src/nvn/nvn_debug.cpp
    runtime/src/nvn/nvn_binds.cpp
    runtime/src/nvn/nvn_deferred.cpp
    runtime/src/nvn/nvn_separate.cpp
    runtime/src/nvn/nvn_buffer_copy.cpp
    runtime/src/nvn/nvn_texel_writes.cpp
    runtime/src/nvn/nvn_buffer_texture.cpp
    runtime/src/nvn/nvn_copy.cpp
    runtime/src/nvn/nvn_pipeline.cpp
    runtime/src/nvn/nvn_texel_uploads.cpp
    runtime/src/nvn/nvn_dummy.cpp
    runtime/src/nvn/nvn_pipeline_cache.cpp
    runtime/src/nvn/nvn_upload.cpp
    runtime/src/nvn/nvn_astc.cpp
    runtime/src/nvn/nvn_spirv_patch.cpp
    runtime/src/nvn/nvn_spirv_clip.cpp
    runtime/src/nvn/nvn_ultrawide.cpp
    runtime/src/nvn/nvn_present.cpp
    runtime/src/nvn/nvn_pipeline_cache.cpp
    runtime/src/nvn/nvn_bc.cpp
    runtime/src/nvn/nvn_draw.cpp
    runtime/src/nvn/nvn_sets.cpp
    runtime/src/nvn/nvn_clears.cpp
    runtime/src/nvn/nvn_mask_clear.cpp
    runtime/src/nvn/nvn_depth_mode.cpp
    runtime/src/nvn/nvn_draw_trace.cpp
    runtime/src/vk_bridge.cpp
    runtime/src/vk_swapchain.cpp
    runtime/src/vk_gen.cpp
    runtime/src/vk_present.cpp
)
file(GLOB NX_GAME_SOURCES CONFIGURE_DEPENDS "${NX_GAME_DIR}/*.cpp")
list(APPEND NX_RUNTIME_SOURCES ${NX_GAME_SOURCES})

# Vulkan headers (the same registry version tools/gen_vk.py generated from). The loader comes with the GPU driver.
if(DEFINED ENV{VULKAN_SDK})
    set(NX_VULKAN_SDK "$ENV{VULKAN_SDK}" CACHE PATH "Vulkan SDK")
else()
    set(NX_VULKAN_SDK "C:/VulkanSDK/1.4.357.0" CACHE PATH "Vulkan SDK")
endif()

if(ANDROID)
    # NativeActivity app: lib<game>.so, entry android_main (android_native_app_glue).
    set(GLUE "${ANDROID_NDK}/sources/android/native_app_glue")
    add_library(${NX_GAME} SHARED ${NX_RUNTIME_SOURCES} runtime/src/platform_android.cpp
        runtime/src/platform_android_input.cpp runtime/src/platform_android_audio.cpp runtime/src/memguard_android.cpp
        "${GLUE}/android_native_app_glue.c")
    target_include_directories(${NX_GAME} PRIVATE runtime/include runtime/src "${NX_GAME_DIR}" "${NX_VULKAN_SDK}/Include" "${GLUE}")
    # Our ANativeActivity_onCreate (platform_android.cpp) runs first, on the UI thread, then calls the glue's.
    set_source_files_properties("${GLUE}/android_native_app_glue.c" PROPERTIES
        COMPILE_DEFINITIONS "ANativeActivity_onCreate=glue_ANativeActivity_onCreate")
    target_link_libraries(${NX_GAME} PRIVATE ${NX_COMPILED_MODULES} android log aaudio)
    target_compile_options(${NX_GAME} PRIVATE -fvisibility=hidden -fno-semantic-interposition -march=armv8.2-a+crypto)
    # The glue's ANativeActivity_onCreate must be exported. 16 KB page alignment: Android 15+ devices with 16 KB pages.
    target_link_options(${NX_GAME} PRIVATE -u ANativeActivity_onCreate -Wl,-z,max-page-size=16384)
elseif(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set_source_files_properties(runtime/src/mac_video.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    # macOS: SDL3 (window, input, audio) and the Vulkan loader + MoltenVK from Homebrew
    # (brew install sdl3 vulkan-headers vulkan-loader molten-vk).
    find_package(SDL3 REQUIRED CONFIG)
    find_path(NX_VULKAN_INCLUDE vulkan/vulkan.h HINTS /opt/homebrew/include /usr/local/include)
    add_executable(${NX_GAME} ${NX_RUNTIME_SOURCES} runtime/src/platform_mac.cpp runtime/src/mac_memory.cpp
        runtime/src/switch2bridge.cpp runtime/src/memguard_mac.cpp runtime/src/mac_video.mm)
    target_include_directories(${NX_GAME} PRIVATE runtime/include runtime/src "${NX_GAME_DIR}" "${NX_VULKAN_INCLUDE}")
    find_library(NX_APPKIT AppKit REQUIRED)
    target_link_libraries(${NX_GAME} PRIVATE ${NX_COMPILED_MODULES} SDL3::SDL3 ${NX_APPKIT})
    target_compile_options(${NX_GAME} PRIVATE -fvisibility=hidden)
    # El ejecutable carga cerca de 4 GiB, por debajo de los rangos del invitado. El juego utiliza su
    # own thread with a 64 MiB stack (platform_mac.cpp), so the main thread's stack size does not matter.
elseif(UNIX)
    # Linux: SDL3 (window, input, audio) and the system Vulkan loader, as on macOS. SDL3 from the distribution or a
    # local build (-DCMAKE_PREFIX_PATH=<its install prefix>).
    find_package(SDL3 REQUIRED CONFIG)
    find_package(Threads REQUIRED)
    find_path(NX_VULKAN_INCLUDE vulkan/vulkan.h HINTS "${NX_VULKAN_SDK}/include" /usr/include /usr/local/include)
    add_executable(${NX_GAME} ${NX_RUNTIME_SOURCES} runtime/src/platform_linux.cpp runtime/src/memguard_linux.cpp)
    target_include_directories(${NX_GAME} PRIVATE runtime/include runtime/src "${NX_GAME_DIR}" "${NX_VULKAN_INCLUDE}")
    target_link_libraries(${NX_GAME} PRIVATE ${NX_COMPILED_MODULES} SDL3::SDL3 Threads::Threads ${CMAKE_DL_LIBS})
    target_compile_options(${NX_GAME} PRIVATE -fvisibility=hidden -march=x86-64-v3 -ffp-contract=off)
    # The executable (PIE) loads near 0x55_5555_0000_0000, far above the guest ranges (0x40_0000_0000 and up). The
    # game runs on its own thread with a 64 MiB stack (platform_linux.cpp).
else()
    add_executable(${NX_GAME} ${NX_RUNTIME_SOURCES} runtime/src/platform_win.cpp runtime/src/memguard_win.cpp)
    target_include_directories(${NX_GAME} PRIVATE runtime/include runtime/src "${NX_GAME_DIR}" "${NX_VULKAN_SDK}/Include")
    target_link_libraries(${NX_GAME} PRIVATE ${NX_COMPILED_MODULES} xinput xaudio2 ole32 psapi)
    # The runtime uses the portable C library (getenv, fopen, ...) on every host: no "_s" variants.
    target_compile_definitions(${NX_GAME} PRIVATE _CRT_SECURE_NO_WARNINGS _SILENCE_CXX20_U8PATH_DEPRECATION_WARNING)
    if(MSVC)
        # Keep host allocations low so the guest ranges (0x40_0000_0000..0x70_0000_0000) are always free; a big
        # main-thread stack (recompiled calls nest deeply); a link map for tools/symbolize.py.
        target_link_options(${NX_GAME} PRIVATE /STACK:67108864 /HIGHENTROPYVA:NO /MAP)
    endif()
endif()
