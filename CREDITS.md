# Créditos y procedencia

Este cuaderno reúne documentación original de las adaptaciones locales de SwonDev, con asistencia de herramientas de desarrollo. No incorpora el runtime ni código de los repositorios enlazados. Las descripciones y plantillas fueron redactadas para este proyecto; los enlaces permiten consultar los antecedentes sin copiarlos.

## Proyectos fundamentales

| Proyecto y autores | Papel en el aprendizaje | Referencia |
|---|---|---|
| jhaizhou-ops / jhaizhou y contribuidores de D2R Mobile | Antecedente inicial de adaptación ARM64/Horizon para Apple y trabajo Mac. No es la base del runtime de Castlevania. | [d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile), [licencia MIT](https://github.com/jhaizhou-ops/d2r-mobile/blob/9393b00ea9aca52d9254214da93738f26582562f/LICENSE) |
| BelmanteGu y contribuidores de SuperMarioWonderRecomp | Base del runtime que se adaptó localmente al segundo título. | [Revisión usada 010ed7a](https://github.com/BelmanteGu/SuperMarioWonderRecomp/tree/010ed7a596a621c15d3a8dfa5795ca8c3668b066), [MIT](https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/010ed7a596a621c15d3a8dfa5795ca8c3668b066/LICENSE) |
| SwonDev / Switch2Bridge y sus contribuidores | Transporte del mando Switch 2 Pro. La adaptación implementa un consumidor directo específico. | [Repositorio](https://github.com/SwonDev/Switch2Bridge), [licencia del propio proyecto](https://github.com/SwonDev/Switch2Bridge/blob/d7dc2f240ea4a267a84bc44f89668048380db1ba/LICENSE), [integración](docs/SWITCH2BRIDGE.md) |

La contribución a D2R se mantiene en su [PR borrador #87](https://github.com/jhaizhou-ops/d2r-mobile/pull/87); no se ha duplicado aquí. La existencia de ese PR no implica merge o aprobación upstream.

## Herramientas y antecedentes técnicos

Gracias a los autores y contribuidores de [SDL](https://github.com/libsdl-org/SDL), [MoltenVK](https://github.com/KhronosGroup/MoltenVK), [Vulkan/SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools), [Unicorn](https://github.com/unicorn-engine/unicorn), [UnityPy](https://github.com/K0lb3/UnityPy), [Ryubing, traductor documentado por el upstream](https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/010ed7a596a621c15d3a8dfa5795ca8c3668b066/docs/nvn/TRANSLATOR.md), [LibHac 0.19.0 de Alex Barney](https://www.nuget.org/packages/LibHac/0.19.0), [hactool](https://github.com/SciresM/hactool) y [AOSP](https://android.googlesource.com/platform/frameworks/av/+/android-5.1.1_r1/include/media/stagefright/OMXCodec.h). Sus implementaciones, especificaciones y pruebas permitieron investigar fronteras concretas. Las herramientas no quedan relicenciadas por este documento ni se redistribuyen en este repositorio.

Apple Game Porting Toolkit, Retro Porting Toolkit y SwitchRecomp ya documentan procesos relacionados. La [comparación con referencias versionadas](docs/PROYECTOS-RELACIONADOS.md) reconoce ese solapamiento; las fases de porting, QA y publicación no son una invención de este cuaderno.

## Juegos y capturas

**Castlevania: Belmont’s Curse** y sus imágenes, personajes, interfaz y marcas pertenecen a KONAMI y a los demás titulares del contenido. Información oficial: [KONAMI](https://www.konami.com/games/castlevania/belmonts_curse/eu/en/). Las doce capturas técnicas seleccionadas se conservan localmente, sin retoque. **No están publicadas en este repositorio ni quedan bajo MIT.** La [galería](docs/GALERIA.md) explica el límite encontrado en las directrices del editor.

**Diablo II: Resurrected** y sus recursos pertenecen a sus respectivos titulares, incluyendo Blizzard Entertainment. Se cita como antecedente técnico y se acredita su proyecto de compatibilidad; este repositorio no incluye imágenes ni recursos de ese juego.

No existe afiliación, patrocinio o aprobación de los editores, Nintendo o Apple. El repositorio no contiene juegos ni ofrece su descarga. El nombre genérico describe el cuaderno de trabajo y no concede derechos adicionales sobre marcas o capturas.
