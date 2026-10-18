# Proyectos relacionados y decisión de alcance

Investigación de fuentes primarias públicas del **18 de octubre de 2026**. Se leyeron documentación, manifiestos, licencias y API GitHub; no se ejecutaron sus motores ni se probó su compatibilidad en esta investigación. Las revisiones siguientes son las observadas ese día, no un seguimiento continuo de HEAD.

## Resultado

**Hay proyectos similares y antecedentes sólidos.** No corresponde anunciar otra metodología inédita, otro starter universal ni un motor nuevo. Este repositorio se publica como **cuaderno complementario de casos originales y plantillas**, para conservar el aprendizaje de las adaptaciones locales solicitadas. Su valor es explicar qué ocurrió, qué se midió y qué se concluyó erróneamente durante esos casos. La galería contextualizada se conserva localmente por el límite de publicación explicado en su documento. Las prácticas generales ya existen en las referencias de abajo.

No se ha demostrado que este cuaderno sea único en GitHub. Una búsqueda finita no permite esa conclusión. Cuando una mejora pertenece a un componente existente, se propone contribuir a ese upstream; el trabajo D2R ya tiene su [PR #87](https://github.com/jhaizhou-ops/d2r-mobile/pull/87). No se abre otro PR equivalente ni se duplica su implementación.

## Comparación

| Proyecto | Qué documenta o implementa | Cómo se relaciona con este cuaderno |
|---|---|---|
| [Apple Game Porting Toolkit](https://github.com/apple/game-porting-toolkit/tree/4f42344c90be22112f46becc3fb9687f423f2f88) | Discovery, hitos, ejecución, validación, recursos Metal y ejemplos. | Ya ofrece metodología y comparación real; este cuaderno relata casos de adaptación específicos y no sustituye ese flujo. |
| [Retro Porting Toolkit](https://github.com/RetroPortingToolKit/RetroPortingToolkit.com/tree/715e42c1d91448342dd49bbf197c31c38717fecc) | Ecosistema, documentación de recompilación, release, QA, IA y vocabulario de estados. | Fuerte solapamiento con fases y publicación. No copiar sus guías ni reivindicar esas prácticas como propias. |
| [recomp-starter](https://github.com/RetroPortingToolKit/recomp-starter/tree/41da76b54de45c90ce46aadc70d7bf9ca801dcc6) | Arranque de un proyecto, separado del motor y de los datos de juegos. | Evaluarlo antes de inventar estructura; aquí no se publica otro starter ejecutable. |
| [bgyss/SwitchRecomp](https://github.com/bgyss/SwitchRecomp/tree/0f99e1c5c1e78fdc8e01e58a0f8dc8622e82976b) | Workspace exploratorio, especificaciones, manifests, matriz por escena y packaging. | Ya tiene QA/release; no alegar ausencia de estas herramientas en Switch. |
| [SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp/tree/2a5e99e16b6ed1678c87daed9aeca4e2a553b6c5) | AArch64 a C++, NVN a Vulkan/MoltenVK, runtime y regresiones de una revisión concreta. | Base real del runtime del caso Castlevania, usada localmente desde un SHA anterior 010ed7a. No demuestra soporte upstream del nuevo título. |
| [d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile/tree/9393b00ea9aca52d9254214da93738f26582562f) | Reescritura ARM64, servicios y gráficos Apple para una revisión concreta de D2R. | Antecedente del método y adaptación Mac; no es un backend para cualquier juego ni base del segundo runtime. |
| [Switch2Bridge](https://github.com/SwonDev/Switch2Bridge/tree/d7dc2f240ea4a267a84bc44f89668048380db1ba) | Transporte del Switch 2 Pro hacia un consumidor Wine/SDL. | Proyecto propio ya existente; se enlaza y se documenta el consumidor nativo específico, sin duplicarlo. |

## Fuentes sobre validación y release

Apple exige ejecución y comparación, no sólo compilar: [porting-validate fijado](https://github.com/apple/game-porting-toolkit/blob/4f42344c90be22112f46becc3fb9687f423f2f88/game-porting-skills/skills/porting-validate/SKILL.md). Su [página oficial GPTK](https://developer.apple.com/games/game-porting-toolkit/) contextualiza las herramientas; la licencia del repositorio no se aplica automáticamente a todos los productos enlazados.

Retro Porting Toolkit ya recomienda allowlist, excluir saves y diagnósticos, probar como jugador y relanzar: [release](https://github.com/RetroPortingToolKit/RetroPortingToolkit.com/blob/715e42c1d91448342dd49bbf197c31c38717fecc/data/docs/04_guides/08_release-a-port/index.md), [verificación de IA](https://github.com/RetroPortingToolKit/RetroPortingToolkit.com/blob/715e42c1d91448342dd49bbf197c31c38717fecc/data/docs/05_agents/03_verification-rituals/index.md), [estados](https://github.com/RetroPortingToolKit/RetroPortingToolkit.com/blob/715e42c1d91448342dd49bbf197c31c38717fecc/data/docs/06_reference/08_status-vocabulary/index.md).

SwitchRecomp incluye [checklist de release](https://github.com/bgyss/SwitchRecomp/blob/0f99e1c5c1e78fdc8e01e58a0f8dc8622e82976b/docs/RELEASE-CHECKLIST.md), [packaging](https://github.com/bgyss/SwitchRecomp/blob/0f99e1c5c1e78fdc8e01e58a0f8dc8622e82976b/docs/packaging.md) y [matriz](https://github.com/bgyss/SwitchRecomp/blob/0f99e1c5c1e78fdc8e01e58a0f8dc8622e82976b/docs/validation-matrix-template.md). Wonder tiene [regresiones](https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/2a5e99e16b6ed1678c87daed9aeca4e2a553b6c5/docs/REGRESSION.md); D2R mantiene [documentación Mac](https://github.com/jhaizhou-ops/d2r-mobile/blob/9393b00ea9aca52d9254214da93738f26582562f/docs/MAC.md) y [procedencia de derivados](https://github.com/jhaizhou-ops/d2r-mobile/blob/9393b00ea9aca52d9254214da93738f26582562f/docs/DERIVED.md).

También se encontraron antecedentes de aceptación instalada fuera del porting de consolas: [RunOnMine](https://github.com/ademisler/RunOnMine/blob/b31b014c650dbd1e8018eff930bcba93ea7a4fe2/docs/acceptance.md) y [tibotattle](https://github.com/adamallcock/tibotattle/blob/7f521836fe7b6e8df7dbb88b53f04a2511dac6ba/docs/runbooks/macos-stable-release-runbook.md). No se auditaron sus implementaciones ni términos para reutilizarlas.

## Licencias: no extrapolar

| Material consultado | Verificación | Uso en este cuaderno |
|---|---|---|
| Apple | [Apache-2.0](https://github.com/apple/game-porting-toolkit/blob/4f42344c90be22112f46becc3fb9687f423f2f88/LICENSE) | Referencia enlazada; sin copiar código o skills. |
| Sitio Retro Porting Toolkit | No se verificó LICENSE raíz general en la revisión consultada. | Descripción original y enlaces. No atribuir MIT a todos sus artículos. |
| Starter independiente | [MIT](https://github.com/RetroPortingToolKit/recomp-starter/blob/41da76b54de45c90ce46aadc70d7bf9ca801dcc6/LICENSE) | No se copia. No extender su licencia al sitio o motores enlazados. |
| SwitchRecomp | [Cargo.toml](https://github.com/bgyss/SwitchRecomp/blob/0f99e1c5c1e78fdc8e01e58a0f8dc8622e82976b/Cargo.toml#L14-L16) declara MIT OR Apache-2.0; sin texto LICENSE/COPYING localizado. | No asumir alcance sobre todas las plantillas/docs; sólo enlazar. |
| Wonder / D2R | [MIT Wonder](https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/2a5e99e16b6ed1678c87daed9aeca4e2a553b6c5/LICENSE), [MIT D2R](https://github.com/jhaizhou-ops/d2r-mobile/blob/9393b00ea9aca52d9254214da93738f26582562f/LICENSE) | Crédito a las bases; el código no se incluye aquí y la licencia no cubre juegos. |
| Switch2Bridge | [Texto MIT del proyecto](https://github.com/SwonDev/Switch2Bridge/blob/d7dc2f240ea4a267a84bc44f89668048380db1ba/LICENSE) | Enlace al repo mantenido. GitHub API lo etiqueta NOASSERTION, por lo que se consultó el texto. |

Las licencias se vuelven a revisar cuando se incorpora material, no sólo cuando se añade un enlace. El nuevo cuaderno licencia únicamente su material original y excluye las capturas comerciales.
