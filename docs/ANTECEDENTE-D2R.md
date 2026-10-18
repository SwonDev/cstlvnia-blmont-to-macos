# Antecedente: D2R Mobile y escritorio

El aprendizaje empezó con [jhaizhou-ops/d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile), proyecto MIT que adapta una revisión concreta de Diablo II: Resurrected. Su [arquitectura versionada](https://github.com/jhaizhou-ops/d2r-mobile/blob/cfa70b3d022095144ce9caee62887ce5f5235c44/dm/README.md) fue la referencia inicial. El runtime del caso Castlevania procede de otro upstream, SuperMarioWonderRecomp; compartir método no implica compartir implementación.

La contribución Mac está presentada como [PR borrador #87](https://github.com/jhaizhou-ops/d2r-mobile/pull/87). Este cuaderno no duplica ese PR ni anuncia que ya esté integrado. No modifica esa instalación. Las capturas públicas de este repositorio corresponden sólo a Castlevania.

## Lecciones reutilizables

- **Control de escritorio completo:** el clic debe llegar al consumidor original de NPC, diálogo, inventario y alijo; ocultar el joystick táctil o dibujar otro cursor no arregla las rutas de entrada.
- **Coordenadas coherentes:** comparar punto físico, transformación DPI, hotspot y geometría efectiva después de abrir/mover paneles. Un hover correcto en un test puede usar otro contenedor que el clic real.
- **Gesto hasta la liberación:** conservar el dueño de DOWN/HELD/UP. Una confirmación que vacíe la mano no debe transformar el mismo gesto en caminar o lanzar habilidad. Probar destino, comando y efecto real por separado.
- **Objeto sostenido visible:** una transferencia lógica puede funcionar mientras el renderer oculta el objeto. Verificar estado y dibujo del arrastre, no sólo bytes de inventario.
- **Texto y layout:** probar cadenas preparadas, propietario del widget, idiomas, escalas, categorías y títulos largos; no ajustar coordenadas fijas contra una única captura.
- **Defaults efectivos:** el preset elegido fue habilidades 1–6 y pociones QWER. Debe llegar al perfil del personaje y recuperarse en frío. No trasladarlo automáticamente a otro juego.
- **Español en dos capas:** contenido original y textos añadidos por el host tienen inventarios distintos. Una voz concreta presente no demuestra todo el catálogo ni escucha por altavoces.
- **Salida nativa:** abrir por LaunchServices y verificar atribución de audio, icono, dependencias y datos en Library; ejecutar el binario desde Terminal no valida la experiencia instalada.
- **Cierre y guardado:** observar commits y recuperación, sin confundir espera de guardado con el tiempo de la fase final de salida. Una notificación o timeout aislado no demuestra pérdida de progreso.

Los informes locales distinguieron pruebas de contratos, GUI aislada, promoción a la app real y reinicio. Una comparación de archivos antes de usar el perfil acredita preservación en ese instante; después de jugar, los cambios esperados deben explicarse. No mantener una afirmación de identidad previa como si siguiera vigente.

## Atribución

El proyecto original y sus contribuidores reciben el crédito por su compatibilidad y arquitectura. Las mejoras locales no convierten al cuaderno en autor del juego, del runtime original ni de sus recursos. Las licencias del código upstream se consultan en su propio repositorio; no se extienden a contenido comercial.
