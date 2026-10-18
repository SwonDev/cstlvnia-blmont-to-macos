# Port compilable de Castlevania para macOS

El objetivo es que una persona clone este repositorio, aporte los ExeFS/RomFS descifrados de su propia revisión 1.1.0 y el icono de su Control, y ejecute un comando para generar, compilar, empaquetar e instalar una app arm64 equivalente a la adaptación local. La documentación del método permanece como apoyo, no como sustituto del código.

## Fronteras de aceptación

1. **Entrada:** identificar las tres imágenes ExeFS por hash y build ID; rechazar módulos de otra revisión sin mutar originales. Todos los productos van a `private/`.
2. **Preparación reproducible:** dependencias fijadas, generación AOT completa con excepciones y perfil, extracción/traducción/validación de los 4.774 shaders necesarios desde datos aportados. Sin caché comercial publicada ni dependencia del workspace original.
3. **Construcción:** CMake/Ninja compila runtime, cliente Switch2Bridge y código generado. Se ejecutan los contratos y pruebas Python sin datos comerciales.
4. **Producto:** paquete autónomo con SDL/MoltenVK/licencias, icono aportado, recursos internos, firma local y guardados fuera del bundle. Instalación no pisa una app existente ni datos del usuario.
5. **Entrega:** generación y construcción desde las fuentes exportadas con entradas locales propias; contratos adicionales desde un checkout limpio, recorrido acotado, README completo y capturas contextualizadas de Castlevania. No publicar juego, claves, partidas o binarios derivados.

Los tests de entrada se observan por la interfaz de preparación; los de empaquetado por su CLI/producto; los del runtime conservan las fronteras existentes. La aceptación final exige ejecutar generación/build/paquete; un test con mocks no sustituye ese recorrido.

## Trabajo y dependencias

| Unidad | Depende de | Resultado |
|---|---|---|
| Importar fuente publicable y procedencia | Revisión de privacidad y MIT | Runtime y adaptación completos, sin programa generado. |
| Automatizar entrada, dependencias y AOT | Fuente importada | CLI ejecutable por etapas y comprobación de revisión. |
| Completar preparación de shaders | Investigación de variantes | Caché reproducible sin copiar la caché privada. |
| Empaquetar e instalar | Binario y recursos preparados | App autónoma validada, con instalación explícita. |
| Checkout limpio y publicación | Todas las anteriores | Build/QA y árbol público revisados; mismo repo renombrado. |

La adaptación no añade soporte Intel ni otros contenidos por inferencia. Las capturas conservan derechos de sus titulares; no se declara aprobación editorial, ni se garantiza ausencia de reclamaciones. Las limitaciones de pruebas físicas continúan explícitas.
