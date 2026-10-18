# Créditos y procedencia

**Castlevania Belmont’s Curse for macOS** adapta el runtime MIT de SuperMarioWonderRecomp a este título. SwonDev mantiene la adaptación macOS, sus herramientas y esta documentación, con asistencia de herramientas de desarrollo e IA y revisión mediante contratos y pruebas. La procedencia y las revisiones fijadas están en [UPSTREAM.md](UPSTREAM.md).

## Código y componentes

| Proyecto y autores | Uso en el port | Fuente y licencia |
|---|---|---|
| BelmanteGu y contribuidores de SuperMarioWonderRecomp | Base del recompilador y runtime, adaptada para Castlevania. No se atribuye al upstream soporte previo de este título. | [Revisión 010ed7a](https://github.com/BelmanteGu/SuperMarioWonderRecomp/tree/010ed7a596a621c15d3a8dfa5795ca8c3668b066), [MIT conservada](licenses/SuperMarioWonderRecomp-MIT.txt). |
| Ryujinx Team and Contributors; mantenedores de Ryubing | Traductor offline Maxwell → SPIR-V y Spv.Generator. El bootstrap obtiene la fuente fijada en el espacio privado. | [Fuente a82350bb](https://codeberg.org/Software_arch/Ryubing/src/commit/a82350bb774f70fcbd41c9987bf67a3775409963), [MIT conservada](licenses/Ryubing-MIT.txt). |
| Advanced Micro Devices | FidelityFX FSR, EASU/RCAS utilizados por la presentación del runtime. | [FidelityFX FSR](https://github.com/GPUOpen-Effects/FidelityFX-FSR), [MIT conservada](licenses/FidelityFX-FSR-MIT.txt). |
| SDL y contribuidores | Ventana, entrada, audio y rutas de recursos. | [SDL](https://github.com/libsdl-org/SDL), licencia zlib; se copia la de la instalación al paquete local. |
| Khronos Group y contribuidores de MoltenVK | Vulkan sobre Metal en macOS. | [MoltenVK](https://github.com/KhronosGroup/MoltenVK), Apache 2.0; se copia la de la instalación al paquete local. |
| Khronos Group, LunarG y contribuidores | Cabeceras, loader y validación Vulkan/SPIR-V. | [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers), [Vulkan-Loader](https://github.com/KhronosGroup/Vulkan-Loader), [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools), con sus propias licencias. |
| SwonDev y contribuidores de Switch2Bridge | Puente para el mando Switch 2 Pro; este runtime consume directamente sus tramas. No se incluye una copia del demonio o la shim. | [Switch2Bridge](https://github.com/SwonDev/Switch2Bridge), [licencia del proyecto](https://github.com/SwonDev/Switch2Bridge/blob/d7dc2f240ea4a267a84bc44f89668048380db1ba/LICENSE). |

## Antecedentes y herramientas

Gracias a **jhaizhou-ops / jhaizhou y contribuidores de [D2R Mobile](https://github.com/jhaizhou-ops/d2r-mobile)** por el antecedente de adaptación ARM64/Horizon para Apple. D2R Mobile inició el aprendizaje del método; **no es la base del runtime de Castlevania**. El trabajo relacionado permanece en el [PR #87](https://github.com/jhaizhou-ops/d2r-mobile/pull/87); no se afirma que esté integrado o aprobado.

[Unicorn](https://github.com/unicorn-engine/unicorn) permitió contrastar estados de instrucciones; [Capstone](https://github.com/capstone-engine/capstone), [UnityPy](https://github.com/K0lb3/UnityPy) y [python-lz4](https://github.com/python-lz4/python-lz4) se usan en análisis y preparación. La reconstrucción local previa también utilizó [LibHac](https://www.nuget.org/packages/LibHac/0.19.0), [hactool](https://github.com/SciresM/hactool) y documentación de [AOSP](https://android.googlesource.com/platform/frameworks/av/+/android-5.1.1_r1/include/media/stagefright/OMXCodec.h) como referencias. Estas menciones no convierten sus licencias en MIT ni indican que todo ese código forme parte de la app.

Apple Game Porting Toolkit, Retro Porting Toolkit, SwitchRecomp, envytools, Switchbrew y la comunidad homebrew aportan herramientas, especificaciones y métodos relacionados. La [metodología](docs/METODOLOGIA.md) registra los hallazgos de esta adaptación sin reivindicar la invención de las fases generales de porting.

## Juego y capturas

**Castlevania: Belmont’s Curse**, sus imágenes, personajes, interfaz, música, películas y marcas pertenecen a **KONAMI y a los demás titulares identificados por el juego**. [Información oficial](https://www.konami.com/games/castlevania/belmonts_curse/eu/en/).

La [galería](docs/GALERIA.md) publica catorce capturas técnicas de la adaptación local, sin retoque, con contexto y manifiesto de hashes. **Las capturas no están bajo MIT** y no acreditan autorización editorial. Las doce primeras son históricas; las dos últimas documentan las pruebas del nuevo paquete, con su alcance concreto. La licencia del código no concede derechos sobre esos contenidos. El repositorio no distribuye ROM, claves, firmware, partidas, código AOT del juego, su caché SPIR-V ni una app con recursos del juego.

**Diablo II: Resurrected** y sus recursos pertenecen a sus titulares, incluyendo Blizzard Entertainment. Se cita únicamente como antecedente; no se publican imágenes o recursos de ese juego aquí.

Este port es independiente y no está afiliado, patrocinado ni aprobado por KONAMI, Nintendo, Apple o los desarrolladores del juego. Las marcas identifican el título y las plataformas compatibles; no presentan el proyecto como oficial.
